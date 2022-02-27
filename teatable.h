#define ENTRY_NUM 1000
#define STRING_LEN 100

#include <stdio.h>


typedef struct
{
    /* data */
    u_int32_t src_ip;
    u_int32_t dst_ip;
//    u_int8_t pro;
//    u_int16_t src_port;
//    u_int16_t dst_port;
}Ip_tuple;

int cmp_ip_tuple(Ip_tuple a, Ip_tuple b);

typedef struct{
    Ip_tuple entries[4];
    char str[STRING_LEN];
}Tea_entry;

typedef struct{
    Tea_entry* tea_entrys;
}Tea_table;

void init_table(Tea_table* tea_table);
void insert_entry(Tea_table* tea_table, Ip_tuple entry, int index);
Tea_entry lookup_entry(Tea_table* tea_table, Ip_tuple entry, int index);
void free_table(Tea_table* tea_table);
