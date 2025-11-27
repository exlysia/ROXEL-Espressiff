#ifndef _ROXEL_OTA_H_
#define _ROXEL_OTA_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <stdatomic.h>
#include <stdint.h>
#include "esp_flash_partitions.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "../utils/roxel_macro.h"

#define ROXEL_OTA_CHUNK_SIZE 2048

typedef struct
{
    size_t len;
    uint8_t data[ROXEL_OTA_CHUNK_SIZE];
} ota_chunk_t;

class RoxelOTA
{
public:
    RoxelOTA(const uint16_t port);
    uint16_t port(void) const;
    bool isExecuted(void);
    bool isRunning(void);
    bool identify(uint32_t hash);
    void launch(void);
    void execute(void);

    TaskHandle_t _task_update_handler = NULL;
    TaskHandle_t _task_server_handler = NULL;

    QueueHandle_t _queue_stack_handler = NULL;
    int _socket = -1;

    const esp_partition_t *_partition = nullptr;
    esp_ota_handle_t _ota_handler;
    bool _partition_init = 0;

    bool _initialize_partition(void);
    void _finish_ota_update(size_t written);
    void _error_reboot(const char *message);
    void _stop_server(void);

private:
    uint32_t _identificator = 0;
    uint16_t _port = 3232;

    std::atomic_bool _is_running = 0;
    std::atomic_bool _is_execute = 0;
    void _remove_queue(void);
    bool _create_queue(void);
    bool _start_tasks(void);
    bool _start_server(void);
    void _stop_ota_update(void);
};

typedef RoxelOTA OTA;

#endif