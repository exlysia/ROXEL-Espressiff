#ifndef _ROXEL_TCP_EXT_H_
#define _ROXEL_TCP_EXT_H_

#include <string.h>
#include <stdint.h>

typedef struct
{
    char *message;
    size_t length;
    int id;
} x10_tcp_message;

typedef struct
{
    int client_id;
} x10_tcp_on_connect;

#endif