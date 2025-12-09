#ifndef _ROXEL_NETWORK_H_
#define _ROXEL_NETWORK_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../utils/roxel_macro.h"
#include "dns.h"

class roxel_network;

typedef roxel_network Network;

typedef enum
{
    NET_MODE_FULL,
    NET_MODE_SAVE
} NetworkMode;

class roxel_network
{
public:
    roxel_network(const char *ssid, const char *passkey, NetworkMode mode = NetworkMode::NET_MODE_SAVE);
    ~roxel_network();
    void useStaticConfiguration(const X10NET_Config &config = X10NET_Config());
    void initialize(void);
    bool await(void);
    bool state(void);
    void release(void);
    NetworkMode mode(void) const;

    TaskHandle_t _task_handler = NULL;
    bool _task_is_running = false;

    void __push_connection_state__(bool state);

private:
    bool _updates_available = false;
    bool _sta_state = false;
    const char *_passkey;
    const char *_ssid;

    NetworkMode _mode = NetworkMode::NET_MODE_SAVE;
    roxel_dns *_dns = nullptr;

    void __sys__init_event_handlers__(void);
    void __sys__stop_network__(void);
    void __start_task__(void);
};

#endif