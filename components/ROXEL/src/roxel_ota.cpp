#include "roxel_ota.h"

inline static const char *OTA_EVENT_ID = "ota";

inline static void __roxel__ota_update_task__(void *pvParameters)
{
    RoxelOTA *ota = static_cast<RoxelOTA *>(pvParameters);
    bool init_result = ota->_initialize_partition();
    if (!init_result)
    {
        vTaskDelete(NULL);
    }
    else
    {
        ROXEL_LOGI("[OTA] Update task is started");
    }
    ota_chunk_t chunk;
    size_t written = 0;
    while (init_result)
    {
        if (IS_NULL(ota->_queue_stack_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(ota->_queue_stack_handler, &chunk, portMAX_DELAY) == pdPASS)
        {
            if (chunk.len == 0)
            {
                break;
            }
            esp_ota_write(ota->_ota_handler, chunk.data, chunk.len);
            written += chunk.len;
        }
    }
    if (NOT_NULL(ota))
    {
        ota->_task_update_handler = NULL;
    }
    if (written > 0)
    {
        ota->_finish_ota_update(written);
    }
    else
    {
        ota->_error_reboot("[OTA] Partition is not written. Rebooting...");
    }
    ROXEL_LOGW("[OTA] Update task is finished");
    vTaskDelete(NULL);
}

inline static void __roxel__ota_server_task__(void *pvParameters)
{
    RoxelOTA *ota = static_cast<RoxelOTA *>(pvParameters);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    ROXEL_LOGI("[OTA] Server task is started");
    while (NOT_NULL(ota))
    {
        if (IS_NULL(ota->_queue_stack_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        int client_sock = accept(ota->_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock > 0)
        {

            ota_chunk_t chunk;
            while ((chunk.len = recv(client_sock, chunk.data, sizeof(chunk.data), 0)) > 0)
            {
                xQueueSend(ota->_queue_stack_handler, &chunk, portMAX_DELAY);
            }
            chunk.len = 0;
            xQueueSend(ota->_queue_stack_handler, &chunk, portMAX_DELAY);
            break;
        }
    }
    if (NOT_NULL(ota))
    {
        ota->_task_server_handler = NULL;
        ota->_stop_server();
    }
    ROXEL_LOGW("[OTA] Server task is finished");
    vTaskDelete(NULL);
}

RoxelOTA::RoxelOTA(const uint16_t port)
{
    _identificator = CRC32(OTA_EVENT_ID);
    _port = port;
}

uint16_t RoxelOTA::port(void) const
{
    return _port;
}

bool RoxelOTA::isExecuted(void)
{
    return _is_execute.load(std::memory_order_relaxed);
}

bool RoxelOTA::isRunning(void)
{
    return _is_running.load(std::memory_order_relaxed);
}

bool RoxelOTA::identify(uint32_t hash)
{
    return hash == _identificator;
}

void RoxelOTA::launch(void)
{
    atomic_exchange(&_is_running, true);
}

void RoxelOTA::execute(void)
{
    bool old = atomic_exchange(&_is_execute, true);
    if (!old)
    {
        if (!_create_queue())
        {
            _error_reboot("[OTA] Allocating queue stack is failed. Rebooting...");
            return;
        }
        if (!_start_server())
        {
            _remove_queue();
            _error_reboot("[OTA] Server starting is failed. Rebooting...");
            return;
        }
        if (!_start_tasks())
        {
            _remove_queue();
            _stop_server();
            _error_reboot("[OTA] Starting tasks is failed. Rebooting...");
            return;
        }
    }
}

bool RoxelOTA::_initialize_partition(void)
{
    if (_partition_init)
    {
        return 1;
    }
    ROXEL_LOGI("[OTA] Initializing update partition...");
    _partition = esp_ota_get_next_update_partition(NULL);
    if (IS_NULL(_partition))
    {
        _error_reboot("[OTA] Partition initialization failed. Rebooting...");
        return 0;
    }
    _partition_init = esp_ota_begin(_partition, OTA_SIZE_UNKNOWN, &_ota_handler) == ESP_OK;
    if (!_partition_init)
    {
        _error_reboot("[OTA] Partition initialization failed. Rebooting...");
    }
    return _partition_init;
}

void RoxelOTA::_finish_ota_update(size_t written)
{
    if (_partition_init)
    {
        ROXEL_LOGI("[OTA] Partition is written: %d bytes", written);
        _stop_ota_update();
        ROXEL_LOGI("[OTA] Setting up boot partition...");
        if (esp_ota_set_boot_partition(_partition) == ESP_OK)
        {
            ROXEL_LOGI("[OTA] Boot partition is installed successfully. Rebooting...");
        }
        else
        {
            ROXEL_LOGE("[OTA] Boot partition installation failed. Rebooting...");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    }
}

void RoxelOTA::_remove_queue(void)
{
    if (NOT_NULL(_queue_stack_handler))
    {
        xQueueReset(_queue_stack_handler);
        DELETE_QUEUE(_queue_stack_handler);
    }
}

bool RoxelOTA::_create_queue(void)
{
    _queue_stack_handler = xQueueCreate(8, sizeof(ota_chunk_t));
    return NOT_NULL(_queue_stack_handler);
}

bool RoxelOTA::_start_tasks(void)
{
    bool result = true;
    result &= xTaskCreate(__roxel__ota_update_task__, "ota_update", 8192, this, 6, &_task_update_handler) == pdPASS;
    result &= xTaskCreate(__roxel__ota_server_task__, "ota_server", 8192, this, 5, &_task_server_handler) == pdPASS;
    return result;
}

void RoxelOTA::_stop_ota_update(void)
{
    if (_partition_init)
    {
        esp_ota_end(_ota_handler);
        _partition_init = 0;
    }
}

bool RoxelOTA::_start_server(void)
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (_socket < 0)
    {
        ROXEL_LOGE("[OTA] Unable to create socket: errno %d", errno);
        return 0;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ROXEL_LOGE("[OTA] Socket bind failed: errno %d", errno);
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
        return 0;
    }

    if (listen(_socket, 5) < 0)
    {
        ROXEL_LOGE("[OTA] Listen failed: errno %d", errno);
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
        return false;
    }

    ROXEL_LOGI("[OTA] Server listening on port %d", _port);
    return 1;
}

void RoxelOTA::_stop_server(void)
{
    if (_socket < 0)
    {
        return;
    }
    ROXEL_LOGW("[OTA] Server is shutting down...");
    shutdown(_socket, SHUT_RDWR);
    close(_socket);
    _socket = -1;
    ROXEL_LOGW("[OTA] Server socket closed");
}

void RoxelOTA::_error_reboot(const char *message)
{
    _stop_ota_update();
    ROXEL_LOGE("%s", message);
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}