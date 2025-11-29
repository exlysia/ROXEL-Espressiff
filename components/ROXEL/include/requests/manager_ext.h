#ifndef _REQUEST_MANAGER_EXT_H_
#define _REQUEST_MANAGER_EXT_H_

#include <stdint.h>
#include <string.h>

typedef struct
{
    char *data = nullptr;
    int8_t client_id = -1;
    uint32_t hash = 0;
} FastRequestTransaction;

typedef struct
{
    char *data = nullptr;
    int8_t client_id = -1;
    uint32_t hash = 0;
    bool connect = 0;
} RequestTransaction;

typedef struct
{
    int8_t client_id;
    int position;
    char *data;
} FastRequestExecution;

typedef struct
{
    int8_t client_id;
    bool connect;
    int position;
    char *data;
} RequestExecution;

#endif