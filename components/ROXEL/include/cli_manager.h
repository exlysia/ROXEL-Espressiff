#ifndef _ROXEL_CLI_MANAGER_H_
#define _ROXEL_CLI_MANAGER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "lwip/sockets.h"
#include "roxel_macro.h"

#define X10_CLIENT_SUSPICIOUS_TIMEOUT 30 * 1000L
#define X10_CLIENT_NO_PING_TIMEOUT 15 * 1000L
#define X10_MAX_CLIENTS 5

typedef struct
{
    char ip_buf[16];
    int sock;
    const char *ip;
    uint32_t hash;
    uint16_t port;
    uint64_t last_active_ms = 0;
    uint64_t created_at;
    bool authorized = false;
    bool activated = false;
} x10_client;

class x10_cli_manager
{
public:
    x10_cli_manager(const char *token);
    ~x10_cli_manager();
    void shedule_clients(void);
    void stop_sheduler(void);
    int activate(x10_client *client);
    void deactivate(int index, bool with_lock = 1);
    void deactivate_all(void);
    void deactivate_suspicious(void);
    int find(uint32_t hash);
    size_t token_size(void) const;
    const char *token(void) const;
    bool auth(uint32_t hash, const char* token);
    int ping(uint32_t hash);
    bool send_to_client(int id, const void *dataptr, size_t size);
    void broadcast(const void *dataptr, size_t size);

    TaskHandle_t _cli_task_shedule_handler = NULL;
private:
    x10_client _clients[X10_MAX_CLIENTS];
    SemaphoreHandle_t _lock = NULL;

    const char *_token;
    size_t _token_size;

    uint64_t ms(void);
};

#endif