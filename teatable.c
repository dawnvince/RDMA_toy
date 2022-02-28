
#define ENTRY_NUM 1000

#include "teatable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int cmp_ip_tuple(Ip_tuple a, Ip_tuple b){
    if(a.dst_ip == b.dst_ip && a.src_ip == b.src_ip)return 1;
    else return 0;
}

Tea_entry* init_table(){
    Tea_entry *tea_entrys = malloc(ENTRY_NUM * sizeof(Tea_entry));
    memset(tea_entrys,0,sizeof(ENTRY_NUM * sizeof(Tea_entry)));
    return tea_entrys;
}

void insert_entry(Tea_entry* tea_entrys, Ip_tuple entry, int index){
    if(index < 0){
        printf("index error\n");
        return;
    }
    if(tea_entrys[index].entries[0].dst_ip == 0){
        tea_entrys[index].entries[0] = entry;
        tea_entrys[(index + ENTRY_NUM - 1) % ENTRY_NUM].entries[2] = entry;
        return;
    }
    if(tea_entrys[index].entries[1].dst_ip == 0){
        tea_entrys[index].entries[1] = entry;
        tea_entrys[(index + ENTRY_NUM - 1) % ENTRY_NUM].entries[3] = entry;
        return;
    }
    if(tea_entrys[index].entries[2].dst_ip == 0){
        tea_entrys[index].entries[2] = entry;
        tea_entrys[(index + 1) % ENTRY_NUM].entries[0] = entry;
        return;
    }
    if(tea_entrys[index].entries[3].dst_ip == 0){
        tea_entrys[index].entries[3] = entry;
        tea_entrys[(index + 1) % ENTRY_NUM].entries[1] = entry;
        return;
    }
    int r = rand() % 2;
    tea_entrys[index].entries[r] = entry;
    tea_entrys[(index + ENTRY_NUM - 1) % ENTRY_NUM].entries[r + 2] = entry;
}


Tea_entry lookup_entry(Tea_entry* tea_entrys, Ip_tuple entry, int index){
    return tea_entrys[index];
}

void free_table(Tea_entry* tea_entrys){
    free(tea_entrys);
}


