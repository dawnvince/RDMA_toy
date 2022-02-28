#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <infiniband/verbs.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <endian.h>
#include <byteswap.h>
#include <stdbool.h>
#include <netdb.h>

#include "teatable.h"


#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#define log_err(M, ...) fprintf(stderr, "[ERROR] (%s:%d:%s: errno: %s) " M "\n",\
                __FILE__, __LINE__, __func__, clean_errno(), ##__VA_ARGS__)
#define scheck(A, M, ...) if(!(A)) {log_err(M, ##__VA_ARGS__);perror(strerror(errno));\
                exit(errno);  }
#define check(A, M, ...) if(!(A)) {log_err(M, ##__VA_ARGS__); errno=0; goto error;}
#define INFO(fmt, args...) { fprintf(stderr, "[INFO]: %s(): " fmt, __func__, ##args); }
#define INFOHeader(fmt, args...) { fprintf(stderr, "INFO: %s(): ==============" fmt "================\n", __func__, ##args); }


#define IB_PORT         1
#define MAX_POLL_CQ_TIMEOUT 5000 // 5s
// #define SERVER
#define REQ_SIZE        99

struct ConfigInfo{
    bool is_server;          /* if the current node is server */
    int msg_size;           /* the size of each echo message */
    int num_concurr_msgs;   /* the number of messages can be sent concurrently */
    char *sock_port;         /* socket port number */
    char *server_name;       /* server name */

    int gid_index;

};

struct ConfigInfo initConfig(){
    struct ConfigInfo config_info;
    config_info.msg_size = 100;
    config_info.num_concurr_msgs = 1;
    config_info.sock_port = "8977";

    /* =============CONFIG HERE!!!============ */
    config_info.gid_index = 2;

#ifdef SERVER
    config_info.is_server = true;
    config_info.server_name = NULL;
#else
    config_info.is_server = false;
    config_info.server_name = "172.16.185.134";
#endif
    return config_info;
}


struct QP_Info {
    uint64_t addr;   // buffer address
    uint32_t rkey;   // remote key
    uint32_t qp_num; // QP number
    uint16_t lid;    // LID of the IB port
    uint8_t gid[16]; // GID
} __attribute__((packed));


//using verbs
struct IBRes {
    struct ibv_context      *ctx;
    struct ibv_pd       *pd;    // protection domain
    struct ibv_mr       *mr;    // memory region
    struct ibv_cq       *cq;
    struct ibv_qp       *qp;
    struct ibv_port_attr     port_attr;
    struct ibv_device_attr   dev_attr;
    struct QP_Info           remote_props; // save remote side info

    Tea_entry   *ib_buf;
    size_t  ib_buf_size;

    char    *ib_content;
    size_t  ib_content_size;
    struct  ibv_mr       *cmr;

    union ibv_gid my_gid;
};


struct ConfigInfo config_info;


int post_send(struct IBRes *ib_res, uint32_t req_size, int opcode){
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;

    // struct ibv_sge sge = {
    //     .addr   = (uintptr_t)ib_res->buf,
    //     .length = req_size,
    //     .lkey   = res->mr->lkey
    // };

    // struct ibv_send_wr sr = {
    //     .wr_id      = 0,
    //     .sg_list    = &sge,
    //     .num_sge    = 1,
    //     .opcode     = opcode,
    //     .send_flags = IBV_SEND_SIGNALED,
    //     .next       = NULL
    // };

    // prepare the scatter / gather entry
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)ib_res->ib_buf;
    sge.length = req_size;
    sge.lkey = ib_res->mr->lkey;

    // prepare the send work request
    memset(&sr, 0, sizeof(sr));

    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;

    sr.num_sge = 1;
    sr.opcode = opcode;
    sr.send_flags = IBV_SEND_SIGNALED;

    if(opcode != IBV_WR_SEND){
        sr.wr.rdma.remote_addr = ib_res->remote_props.addr;
        sr.wr.rdma.rkey = ib_res->remote_props.rkey;
    }

    int ret = 0;
    ret = ibv_post_send(ib_res->qp, &sr, &bad_wr);
    check(ret == 0, "Failed to post SR");

    switch (opcode){
        case IBV_WR_SEND:
            INFO("Send request was posted\n");
            break;
        case IBV_WR_RDMA_READ:
            INFO("RDMA read request was posted\n");
            break;
        case IBV_WR_RDMA_WRITE:
            INFO("RDMA write request was posted\n");
            break;
        default:
            INFO("Unknown request was posted\n");
            break;
    }
    return 0;
 error:
    return -1;
}

// sig 0 indicates unsignaled/write
int post_send_client_read(struct IBRes *ib_res, uint32_t req_size, int index){
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;

    // struct ibv_sge sge = {
    //     .addr   = (uintptr_t)ib_res->buf,
    //     .length = req_size,
    //     .lkey   = res->mr->lkey
    // };

    // struct ibv_send_wr sr = {
    //     .wr_id      = 0,
    //     .sg_list    = &sge,
    //     .num_sge    = 1,
    //     .opcode     = opcode,
    //     .send_flags = IBV_SEND_SIGNALED,
    //     .next       = NULL
    // };

    // prepare the scatter / gather entry
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)ib_res->ib_buf;
    sge.length = req_size;
    sge.lkey = ib_res->mr->lkey;

    // prepare the send work request
    memset(&sr, 0, sizeof(sr));

    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;

    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_READ;
    sr.send_flags = IBV_SEND_SIGNALED;

    sr.wr.rdma.remote_addr = 
                ib_res->remote_props.addr + index * sizeof(Tea_entry);
    sr.wr.rdma.rkey = ib_res->remote_props.rkey;

    INFO("sizeof Ip_tuple is %d\n", sizeof(Ip_tuple));
    INFO("sizeof Tea_entry is %d\n", sizeof(Tea_entry));
    INFO("addr ibbuf is %lx\n", ib_res->ib_buf);
    INFO("addr sge.addr is %lx\n", sge.addr);
    INFO("addr remote_props.addr is %lx\n", ib_res->remote_props.addr);
    INFO("addr sr.wr.rdma.remote_addr is %lx\n", sr.wr.rdma.remote_addr);


    int ret = 0;
    ret = ibv_post_send(ib_res->qp, &sr, &bad_wr);
    check(ret == 0, "Failed to post SR");

    INFO("RDMA read request was posted\n");

    return 0;
 error:
    return -1;
}

int post_send_client_write(struct IBRes *ib_res, uint32_t req_size, int index){
    struct ibv_send_wr sr;
    struct ibv_sge sge;
    struct ibv_send_wr *bad_wr = NULL;

    // struct ibv_sge sge = {
    //     .addr   = (uintptr_t)ib_res->buf,
    //     .length = req_size,
    //     .lkey   = res->mr->lkey
    // };

    // struct ibv_send_wr sr = {
    //     .wr_id      = 0,
    //     .sg_list    = &sge,
    //     .num_sge    = 1,
    //     .opcode     = opcode,
    //     .send_flags = IBV_SEND_SIGNALED,
    //     .next       = NULL
    // };

    // prepare the scatter / gather entry
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)ib_res->ib_content;
    sge.length = ib_res->ib_buf_size;
    sge.lkey = ib_res->cmr->lkey;

    // prepare the send work request
    memset(&sr, 0, sizeof(sr));

    sr.next = NULL;
    sr.wr_id = 0;
    sr.sg_list = &sge;

    sr.num_sge = 1;
    sr.opcode = IBV_WR_RDMA_WRITE;
    //if(sig)
        sr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)ib_res->ib_buf + 4 * sizeof(Ip_tuple);
            uint64_t tmp = ib_res->remote_props.addr + index * sizeof(Tea_entry);
            sr.wr.rdma.remote_addr = tmp + 4*sizeof(Ip_tuple);
    sr.wr.rdma.rkey = ib_res->remote_props.rkey;

    INFO("sizeof Ip_tuple is %d\n", sizeof(Ip_tuple));
    INFO("sizeof Tea_entry is %d\n", sizeof(Tea_entry));
    INFO("addr ibbuf is %lx\n", ib_res->ib_buf);
    INFO("addr sge.addr is %lx\n", sge.addr);
    INFO("addr remote_props.addr is %lx\n", ib_res->remote_props.addr);
    INFO("addr sr.wr.rdma.remote_addr is %lx\n", sr.wr.rdma.remote_addr);


    int ret = 0;
    ret = ibv_post_send(ib_res->qp, &sr, &bad_wr);
    check(ret == 0, "Failed to post SR");

    INFO("RDMA write request was posted\n");
    
    return 0;
 error:
    return -1;
}

int post_receive(struct IBRes *ib_res, uint32_t req_size){
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;

    // struct ibv_sge list = {
    //     .addr   = (uintptr_t)ib_res->buf,
    //     .length = req_size,
    //     .lkey   = ib_res->mr->lkey
    // };

    // struct ibv_recv_wr rr = {
    //     .wr_id   = 0,
    //     .sg_list = &sge,
    //     .num_sge = 1,
    //     .next    = NULL
    // };

    // prepare the scatter / gather entry
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t)ib_res->ib_buf;
    sge.length = req_size;
    sge.lkey = ib_res->mr->lkey;

    // prepare the receive work request
    memset(&rr, 0, sizeof(rr));

    rr.next = NULL;
    rr.wr_id = 0;
    rr.sg_list = &sge;
    rr.num_sge = 1;

    // post the receive request to the RQ
    int ret = 0;
    ret = ibv_post_recv(ib_res->qp, &rr, &bad_wr);
    check(ret == 0, "Failed to post RR");
    INFO("Receive request was posted\n");

    return 0;
 error:
    return -1;
}


// every signaled send/read/write operation need poll to get result
int poll_completion(struct IBRes *ib_res, struct ibv_wc *wc){
    unsigned long start_time_ms, cur_time_ms;
    struct timeval cur_time;
    int poll_result;

    gettimeofday(&cur_time, NULL);
    start_time_ms = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);

    do{
        poll_result = ibv_poll_cq(ib_res->cq, 1, wc);
        gettimeofday(&cur_time, NULL);
        cur_time_ms = (cur_time.tv_sec * 1000) + (cur_time.tv_usec / 1000);
    }while((poll_result == 0) && 
        ((cur_time_ms - start_time_ms) < MAX_POLL_CQ_TIMEOUT));
    if(poll_result < 0){
        log_err("poll CQ failed");
        return -1;
    }
    else if(poll_result == 0){
        log_err("Completion wasn't found in the CQ after timeout");
        return -1;
    }
    else INFO("Completion was found in CQ with status 0x%x\n", wc->status);
    if(wc->status != IBV_WC_SUCCESS){
        log_err("Got bad completion with status: 0x%x, vendor syndrome: 0x%x\n",
              wc->status, wc->vendor_err);
        return -1;
    }
    INFO("POLL SUCCESS\n");
    return 0;
}


int modify_qp_to_rtr(struct ibv_qp *qp, uint32_t remote_qpn,
                            uint16_t dlid, uint8_t *dgid) {
    struct ibv_qp_attr attr;
    int flags, ret = 0;

    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_256;
    attr.dest_qp_num = remote_qpn;
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = dlid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = IB_PORT;

    if (config_info.gid_index >= 0) {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.port_num = 1;
        memcpy(&attr.ah_attr.grh.dgid, dgid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = config_info.gid_index;
        attr.ah_attr.grh.traffic_class = 0;
    }

    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

    ret = ibv_modify_qp(qp, &attr, flags);
    check(ret == 0, "Failed to change qp to init.")

    INFO("Modify QP to RTR done!\n");
    return 0;
 error:
    return -1;
}


int modify_qp_to_init(struct ibv_qp *qp) {
    struct ibv_qp_attr attr;
    int flags;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = IB_PORT;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
                           IBV_ACCESS_REMOTE_WRITE;

    flags =
        IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;
    int ret = 0;
    ret = ibv_modify_qp(qp, &attr, flags);
    check(ret == 0, "Failed to change qp to init.")

    INFO("Modify QP to INIT done!\n");
    return 0;
 error:
    return -1;
}


int modify_qp_to_rts(struct ibv_qp *qp) {
    struct ibv_qp_attr attr;
    int flags, ret = 0;

    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x12; // 18
    attr.retry_cnt = 6;
    attr.rnr_retry = 0;
    attr.sq_psn = 0;
    attr.max_rd_atomic = 1;

    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT |
            IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;

    ret = ibv_modify_qp(qp, &attr, flags);
    check(ret == 0, "Failed to change qp to rts.")

    INFO("Modify QP to RTS done!\n");
    return 0;
 error:
    return -1;
}


int sock_connect(char* server_name, char* port){
    struct addrinfo *result, *rp;
    struct addrinfo hints = {.ai_flags = AI_PASSIVE,
                             .ai_family = AF_INET,
                             .ai_socktype = SOCK_STREAM};
    int sock_fd = -1, ret = 0;

    ret = getaddrinfo(server_name, port, &hints, &result);
    check(ret==0, "[ERROR] %s", gai_strerror(ret));

    for(rp = result; rp!=NULL; rp = rp->ai_next){
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if(sock_fd == -1)continue;
        
        // SERVER MODE
        if(server_name == NULL)ret = bind(sock_fd, rp->ai_addr, rp->ai_addrlen);
        // CLIENT MODE
        else ret = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
        
        if(ret == 0)break;
        close(sock_fd);
        sock_fd = -1;
    }
    check(rp != NULL, "\nIF SERVER: BIND ERROR\nIF CLIENT: CONNECT ERROR");
    freeaddrinfo(result);

    return sock_fd;
 error:
    if (result)freeaddrinfo(result);
    if (sock_fd > 0)close(sock_fd);
    exit(EXIT_FAILURE);
}


int sock_sync_data(int sock_fd, int xfer_size, char* local_data, char* remote_data){
    int read_bytes = 0;
    int write_bytes = 0;
    write_bytes = write(sock_fd, local_data, xfer_size);
    scheck(write_bytes == xfer_size, "SYNC_WRITE_ERROR");

    read_bytes = read(sock_fd, remote_data, xfer_size);
    scheck(read_bytes == xfer_size, "SYNC_READ_ERROR");

    INFO("SYNCHRONIZED!\n\n");
    return 0;

}


int connect_qp(int sock_fd, struct IBRes *ib_res){
    char tmp_char;
    struct QP_Info local_con_data, remote_con_data, tmp_con_data;

    local_con_data.addr = htonll((uintptr_t)ib_res->ib_buf);
    local_con_data.rkey = htonl(ib_res->mr->rkey);
    local_con_data.qp_num = htonl(ib_res->qp->qp_num);
    local_con_data.lid = htons(ib_res->port_attr.lid);
    memcpy(local_con_data.gid, &ib_res->my_gid, 16);

    sock_sync_data(sock_fd, sizeof(struct QP_Info), \
            (char*)&local_con_data, (char*)&tmp_con_data);

    remote_con_data.addr = ntohll(tmp_con_data.addr);
    remote_con_data.rkey = ntohl(tmp_con_data.rkey);
    remote_con_data.qp_num = ntohl(tmp_con_data.qp_num);
    remote_con_data.lid = ntohs(tmp_con_data.lid);
    memcpy(remote_con_data.gid, &tmp_con_data.gid, 16);

    ib_res->remote_props = remote_con_data;

    INFO("Remote address = 0x%" PRIx64 "\n", remote_con_data.addr);
    INFO("Remote rkey = 0x%x\n", remote_con_data.rkey);
    INFO("Remote QP number = 0x%x\n", remote_con_data.qp_num);
    INFO("Remote LID = 0x%x\n", remote_con_data.lid);

    if(config_info.gid_index > 0){
        printf("Remote GID = ");
        uint8_t* tmp = remote_con_data.gid;
        for(int i = 0;i < 16;++i){
            printf("%d:", tmp[i]);
        }
    }
    int ret = 0;
    // modify the QP to init
    ret = modify_qp_to_init(ib_res->qp);
    check (ret == 0, "Failed to modify qp to init");

    // let the client post RR to be prepared for incoming messages
    if (!config_info.is_server) {
        post_receive(ib_res, REQ_SIZE);
    }

    // modify the QP to RTR
    ret = modify_qp_to_rtr(ib_res->qp, remote_con_data.qp_num,
        remote_con_data.lid, remote_con_data.gid);
    check(ret == 0, "Failed to modify qp to rtr");

    // modify the QP to RTS
    ret = modify_qp_to_rts(ib_res->qp);
    check (ret == 0, "Failed to modify qp to rts");

    sock_sync_data(sock_fd, 1, "Q", &tmp_char);

    return 0;

 error:
    return -1;
}

void close_ib_connection (struct IBRes *ib_res){
    if (ib_res->qp != NULL)ibv_destroy_qp (ib_res->qp);
    if (ib_res->cq != NULL)ibv_destroy_cq (ib_res->cq);
    if (ib_res->mr != NULL)ibv_dereg_mr (ib_res->mr);
    if (ib_res->pd != NULL)ibv_dealloc_pd (ib_res->pd);
    if (ib_res->ctx != NULL)ibv_close_device (ib_res->ctx);
    if (ib_res->ib_buf != NULL)free (ib_res->ib_buf);
}


struct IBRes ib_res;


int main(){
    config_info = initConfig();

    /* ===============init teatable=================*/
    INFOHeader("INIT TEATABLE");
    
    Tea_entry *tea_entrys = init_table();
    Ip_tuple entry = {
        .src_ip = 16,
        .dst_ip = 32,
    };
    insert_entry(tea_entrys, entry, 1);
    char *str = "Packet content here.";
    // strcpy(tea_entrys[1].str, str);

    /* ===============set up ib=================*/
    int ret = 0;
    memset (&ib_res, 0, sizeof(struct IBRes));
    struct ibv_device **dev_list = NULL;

    /* get IB device list */
    dev_list = ibv_get_device_list(NULL);
    check(dev_list != NULL, "Failed to get ib device list.");

    /* create IB context */
    ib_res.ctx = ibv_open_device(*dev_list);
    check(ib_res.ctx != NULL, "Failed to open ib device.");

    /* allocate protection domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    check(ib_res.pd != NULL, "Failed to allocate protection domain.");

    /* query IB port attribute */
    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    check(ret == 0, "Failed to query IB port information.");
    
    /* register mr */
    /* set the buf_size (msg_size + 1) * num_concurr_msgs */
    /* the recv buffer is of size msg_size * num_concurr_msgs */
    /* followed by a sending buffer of size msg_size since we */
    /* assume all msgs are of the same content */
    if(config_info.is_server){
        ib_res.ib_buf_size =  100 * sizeof(Tea_entry) * (config_info.num_concurr_msgs + 1);
        ib_res.ib_buf      = tea_entrys;
        check (ib_res.ib_buf != NULL, "Failed to allocate ib_buf");

        ib_res.mr = ibv_reg_mr (ib_res.pd, (void *)ib_res.ib_buf,
                    ib_res.ib_buf_size,
                    IBV_ACCESS_LOCAL_WRITE |
                    IBV_ACCESS_REMOTE_READ |
                    IBV_ACCESS_REMOTE_WRITE);
        check (ib_res.mr != NULL, "Failed to register mr");
    }
    else{
        ib_res.ib_buf_size = sizeof(Tea_entry) * (config_info.num_concurr_msgs + 1);
        ib_res.ib_buf      = tea_entrys;
        check (ib_res.ib_buf != NULL, "Failed to allocate ib_buf");

        ib_res.mr = ibv_reg_mr (ib_res.pd, (void *)ib_res.ib_buf,
                    ib_res.ib_buf_size,
                    IBV_ACCESS_LOCAL_WRITE |
                    IBV_ACCESS_REMOTE_READ |
                    IBV_ACCESS_REMOTE_WRITE);
        check (ib_res.mr != NULL, "Failed to register mr");

        ib_res.ib_content_size = REQ_SIZE;
        ib_res.ib_content = str;
        check (ib_res.ib_content != NULL, "Failed to allocate ib_content");

        ib_res.cmr = ibv_reg_mr (ib_res.pd, (void *)ib_res.ib_content,
                    ib_res.ib_content_size,
                    IBV_ACCESS_LOCAL_WRITE |
                    IBV_ACCESS_REMOTE_READ |
                    IBV_ACCESS_REMOTE_WRITE);
        check (ib_res.cmr != NULL, "Failed to register cmr");
    }

    /* query IB device attr */
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    check(ret==0, "Failed to query device");
    
    union ibv_gid my_gid;
    memset(&my_gid, 0, sizeof(my_gid));
    if(config_info.gid_index > 0){
        ret = ibv_query_gid(ib_res.ctx, IB_PORT, config_info.gid_index, &my_gid);
        check(ret == 0, "Failed to query GID");

        // print gid
        INFOHeader("GID");
        uint8_t* tmp = (uint8_t*)&my_gid;
        for(int i = 0;i < 16;++i){
            printf("%d:", tmp[i]);
        }
    }
    ib_res.my_gid = my_gid;

    INFOHeader("CREATE CQ AND QP");
    int cq_size = 10;
    ib_res.cq = ibv_create_cq (ib_res.ctx, cq_size, 
                   NULL, NULL, 0);
    check (ib_res.cq != NULL, "Failed to create cq");

    /* create qp */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .cap = {
            .max_send_wr = ib_res.dev_attr.max_qp_wr,
            .max_recv_wr = ib_res.dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };

    ib_res.qp = ibv_create_qp (ib_res.pd, &qp_init_attr);
    check (ib_res.qp != NULL, "Failed to create qp");

    // using TCP to transfer related infomation
    INFOHeader("CONNECT QP");

    int server_sock_fd = -1, peer_sock = -1;
    // SERVER MODE
    if(config_info.is_server){
        INFO("Waiting on port %s for TCP connection\n",config_info.sock_port);
        server_sock_fd = sock_connect(NULL, config_info.sock_port);
        check(server_sock_fd > 0, "Failed to create server socket.");
        listen(server_sock_fd, 5);
        peer_sock = accept(server_sock_fd, NULL, 0);
        check(peer_sock > 0, "Failed to create peer sockfd");
    }
    else{
        peer_sock = sock_connect(config_info.server_name, config_info.sock_port);
    }

    ret = connect_qp(peer_sock, &ib_res);
    check(ret == 0, "Failed to connect_qp");

    if(dev_list != NULL)
        ibv_free_device_list(dev_list);
    INFO("CONNECT QP SUCCESS\n");

    /*==========================RDMA=========================*/
    INFOHeader("RDMA OPERATIONS");

    struct ibv_wc wc;

    char tmp_char;
    sock_sync_data(peer_sock, 1, "Q", &tmp_char);

    // Operation RDMA read/write, Client side operation
    if(!config_info.is_server){
        int index = 1;
        post_send_client_write(&ib_res, REQ_SIZE, index);
        poll_completion(&ib_res, &wc);
        post_send_client_read(&ib_res, sizeof(Tea_entry), index);
        poll_completion(&ib_res, &wc);
        INFO("Contents of server's buffer: %d %d %s\n", 
            ib_res.ib_buf[0].entries[0].src_ip, ib_res.ib_buf[0].entries[0].dst_ip, 
            ib_res.ib_buf[0].str);

    }
    // check if server memory has changed
    sock_sync_data(peer_sock, 1, "W", &tmp_char);


    close_ib_connection(&ib_res);
    if(server_sock_fd != -1)close(server_sock_fd);
    if(peer_sock != -1)close(peer_sock);

    return 0;
 error:
    if(server_sock_fd != -1)close(server_sock_fd);
    if(peer_sock != -1)close(peer_sock);
    if(dev_list != NULL)
        ibv_free_device_list(dev_list);
    return -1;
}