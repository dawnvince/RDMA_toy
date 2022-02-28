#define ENTRY_NUM 1000
#define STRING_LEN 100

#include <stdio.h>
#include <inttypes.h>


typedef struct
{
    /* data */
    uint32_t src_ip;
    uint32_t dst_ip;
//    uint8_t pro;
//    uint16_t src_port;
//    uint16_t dst_port;
}Ip_tuple;

int cmp_ip_tuple(Ip_tuple a, Ip_tuple b);

typedef struct{
    Ip_tuple entries[4];
    char str[STRING_LEN];
}Tea_entry;

Tea_entry* init_table();
void insert_entry(Tea_entry* tea_entrys, Ip_tuple entry, int index);
Tea_entry lookup_entry(Tea_entry* tea_entrys, Ip_tuple entry, int index);
void free_table(Tea_entry* tea_entrys);
