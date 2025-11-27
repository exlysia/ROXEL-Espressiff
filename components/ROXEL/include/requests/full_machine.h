#ifndef _ROXEL_REQUEST_MACHINE_H_
#define _ROXEL_REQUEST_MACHINE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../utils/machine_impl.h"
#include "../utils/cli_manager.h"
#include "../utils/roxel_macro.h"
#include "fast_machine.h"
#include "manager_ext.h"
#include "ext.h"

class X10_RequestMachine : public x10_machine_impl
{
public:
    X10_RequestMachine(x10_cli_manager *cli_manager);
    ~X10_RequestMachine();
    bool load(RequestLoader *loader);

    bool identify(uint32_t hash, cJSON *json) override;
    void on_connect(int client_id) override;
    void process(int client_id, uint32_t hash, const char *event, cJSON *json) override;

    QueueHandle_t _transaction_queue_handler = NULL;
    QueueHandle_t _execution_queue_handler = NULL;
    QueueHandle_t _respond_queue_handler = NULL;

    TaskHandle_t _transaction_task_handler = NULL;
    TaskHandle_t _execution_task_handler = NULL;
    TaskHandle_t _respond_task_handler = NULL;

    SemaphoreHandle_t _instances_lock = NULL;

    uint32_t *_request_hashes = nullptr;
    RequestImpl **_requests = nullptr;
    size_t _request_count = 0;

    char *respond_buffer = nullptr;
    size_t respond_buffer_size = 0;

    x10_cli_manager *_cli_manager;

private:
    FastRequestMachine *_fast_request_machine = nullptr;

    uint32_t _identificator = 0;

    bool _initialize_query(void);
    bool _start_tasks(void);
    void _release_resources(void);

    size_t _accept_requests(RequestImpl **requests, size_t count);
    int8_t _validate_request(RequestImpl *request);
    bool _allocate_requests(size_t size);
    bool _allocate_respond_cache(void);
    void _load_requests(RequestLoader *loader);
    void _clean_trash(void);
    void _free_buffer(void);
};

#endif