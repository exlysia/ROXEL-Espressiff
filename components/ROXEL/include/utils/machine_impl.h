#ifndef _ROXEL_MACHINE_IMPL_H_
#define _ROXEL_MACHINE_IMPL_H_

#include "cJSON.h"
#include <stdint.h>

class x10_machine_impl
{
public:
    virtual bool identify(uint32_t hash, cJSON *json)
    {
        return 0;
    }

    virtual void on_connect(int client_id)
    {
    }

    virtual void process(int client_id, uint32_t hash, const char *event, cJSON *json)
    {
    }
};

#endif