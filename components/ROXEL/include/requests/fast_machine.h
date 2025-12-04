#ifndef _FAST_REQUEST_MACHINE_H_
#define _FAST_REQUEST_MACHINE_H_

#include "../utils/cli_manager.h"
#include "../utils/roxel_macro.h"
#include "manager_ext.h"
#include "ext.h"
#include <lwip/sockets.h>

class FastRequestMachine
{
public:
    FastRequestMachine(uint16_t port, x10_cli_manager *cli_manager);
    ~FastRequestMachine();
    bool load(RequestLoader *loader);
    void launch(void);
    void stop(void);
    bool isRunning(void) const;

    QueueHandle_t _transaction_queue_handler = NULL;
    QueueHandle_t _execution_queue_handler = NULL;
    
    TaskHandle_t _transaction_task_handler = NULL;
    TaskHandle_t _execution_task_handler = NULL;
    TaskHandle_t _udp_server_task_handler = NULL;

    SemaphoreHandle_t _instances_lock = NULL;

    uint32_t *_request_hashes = nullptr;
    RequestImpl **_requests = nullptr;
    size_t _request_count = 0;

    x10_cli_manager *_cli_manager;
    uint32_t _identificator = 0;
    int _socket = -1;

    bool _create_server(void);
    void _stop_server(void);

private:
    bool _is_running = 0;
    uint16_t _udp_port;

    bool _start_server_task(void);
    void _stop_server_task(void);
    bool _start_tasks(void);
    bool _initialize_query(void);
    void _load_requests(RequestLoader *loader);
    size_t _accept_requests(RequestImpl **requests, size_t count);
    int8_t _validate_request(RequestImpl *request);
    bool _allocate_requests(size_t size);
    void _release_resources(void);
    void _clean_trash(void);
};

#endif