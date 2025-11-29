#include "requests/fast_machine.h"

inline static const char *FAST_REQUEST_EVENT_ID = "f_req";

inline static void __frm_execution_task__(void *pvParameters)
{
    FastRequestMachine *machine = static_cast<FastRequestMachine *>(pvParameters);
    FastRequestExecution execution;

    ROXEL_LOGI("[REQUEST MACHINE | FAST] Execution task is started");
    while (NOT_NULL(machine))
    {
        if (IS_NULL(machine->_execution_queue_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(machine->_execution_queue_handler, &execution, portMAX_DELAY))
        {
            RequestImpl *instance = execution.position > -1 && execution.position < machine->_request_count ? machine->_requests[execution.position] : nullptr;
            if (NOT_NULL(instance))
            {
                cJSON *data = cJSON_ParseWithOpts(execution.data, NULL, false);
                if (data)
                {
                    instance->update_call(execution.client_id, data);
                    cJSON_Delete(data);
                }
            }
            if (NOT_NULL(execution.data))
            {
                vPortFree(execution.data);
                execution.data = nullptr;
            }
        }
    }
    vTaskDelete(NULL);
}

inline static void __frm_transaction_task__(void *pvParameters)
{
    FastRequestMachine *machine = static_cast<FastRequestMachine *>(pvParameters);
    FastRequestTransaction transaction;

    ROXEL_LOGI("[REQUEST MACHINE | FAST] Transaction task is started");
    while (NOT_NULL(machine))
    {
        if (IS_NULL(machine->_transaction_queue_handler) || IS_NULL(machine->_execution_queue_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(machine->_transaction_queue_handler, &transaction, portMAX_DELAY))
        {
            int position = -1;
            xSemaphoreTake(machine->_instances_lock, portMAX_DELAY);
            for (size_t i = 0; i < machine->_request_count; i++)
            {
                if (machine->_request_hashes[i] == transaction.hash)
                {
                    position = i;
                    break;
                }
            }
            xSemaphoreGive(machine->_instances_lock);
            if (position < 0)
            {
                continue;
            }
            FastRequestExecution execution;
            execution.client_id = transaction.client_id;
            execution.position = position;
            execution.data = transaction.data;
            transaction.data = nullptr;
            if (xQueueSend(machine->_execution_queue_handler, &execution, portMAX_DELAY) != pdPASS)
            {
                if (NOT_NULL(transaction.data))
                {
                    vPortFree(transaction.data);
                    transaction.data = nullptr;
                    execution.data = nullptr;
                }
            }
        }
    }
    vTaskDelete(NULL);
}

inline static void __frm_udp_message_process__(FastRequestMachine *machine, int8_t client_id, const char *buffer, size_t len)
{
    if (IS_NULL(machine->_transaction_queue_handler))
    {
        return;
    }
    cJSON *json = cJSON_ParseWithOpts(buffer, NULL, false);
    const char *event = NULL;
    if (!json || !cJSON_IsObject(json) || IS_NULL((event = cJSON_GetStringValue(cJSON_GetObjectItem(json, "e")))))
    {
        if (NOT_NULL(json))
        {
            cJSON_Delete(json);
        }
        return;
    }
    if (CRC32(event) != machine->_identificator)
    {
        cJSON_Delete(json);
        return;
    }
    const char *id = nullptr;
    cJSON *data_item = cJSON_GetObjectItem(json, "d");
    if (IS_NULL((id = cJSON_GetStringValue(cJSON_GetObjectItem(json, "n")))) || IS_NULL(data_item) || !cJSON_IsObject(data_item))
    {
        cJSON_Delete(json);
        return;
    }
    FastRequestTransaction transaction;
    transaction.data = cJSON_PrintUnformatted(data_item);
    transaction.client_id = client_id;
    transaction.hash = CRC32(id);
    if (NOT_NULL(transaction.data))
    {
        if (xQueueSend(machine->_transaction_queue_handler, &transaction, portMAX_DELAY) != pdPASS)
        {
            vPortFree(transaction.data);
        }
    }
    cJSON_Delete(json);
}

inline static void __frm_udp_server_task__(void *pvParameters)
{
    FastRequestMachine *machine = static_cast<FastRequestMachine *>(pvParameters);
    char addr_str[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    char rx_buffer[512];
    char buffer[256];
    int frame_len = 0;

    while (machine->isRunning())
    {
        int sock = machine->_socket;
        if (sock < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len >= 0)
        {
            rx_buffer[len] = '\0';

            inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
            uint32_t hash = ntohl(client_addr.sin_addr.s_addr);
            int client_id = machine->_cli_manager->find(hash);
            int state = machine->_cli_manager->ping(hash);
            if (state != 1 || client_id < 0)
                continue;

            if (frame_len + len >= sizeof(buffer))
            {
                frame_len = 0;
                continue;
            }

            memcpy(buffer + frame_len, rx_buffer, len);
            frame_len += len;

            size_t i = 0;
            while (i + 3 < frame_len)
            {
                if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
                {
                    buffer[i] = '\0';
                    __frm_udp_message_process__(machine, client_id, buffer, i);
                    size_t remain = frame_len - (i + 4);
                    memmove(buffer, buffer + i + 4, remain);
                    frame_len = remain;
                    i = 0;
                    continue;
                }
                i++;
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (NOT_NULL(machine))
    {
        machine->_stop_server();
        machine->_udp_server_task_handler = NULL;
    }
    vTaskDelete(NULL);
}

FastRequestMachine::FastRequestMachine(uint16_t port, x10_cli_manager *cli_manager)
{
    _instances_lock = xSemaphoreCreateMutex();
    _identificator = CRC32(FAST_REQUEST_EVENT_ID);
    _cli_manager = cli_manager;
    _udp_port = port;
}

FastRequestMachine::~FastRequestMachine()
{
    _release_resources();
    stop();
}

bool FastRequestMachine::load(RequestLoader *loader)
{
    if (IS_NULL(loader) || IS_NULL(loader->requests))
    {
        ROXEL_LOGW("[REQUEST MACHINE | FAST] Request-objects is NULL or not passed");
        return 0;
    }
    if (loader->count > 0)
    {
        _request_count = loader->count;
        size_t accepted = _accept_requests(loader->requests, loader->count);
        ROXEL_LOG(accepted > 0 ? ESP_LOG_INFO : ESP_LOG_ERROR, "[REQUEST MACHINE | FAST] Accepted objects: %d", accepted);
        if (accepted > 0)
        {
            if (_allocate_requests(accepted))
            {
                if (!_initialize_query())
                {
                    ROXEL_LOGE("[REQUEST MACHINE | FAST] Queue initialization is failed");
                    _release_resources();
                    return 0;
                }
                _clean_trash();
                _load_requests(loader);
                if (!_start_tasks())
                {
                    ROXEL_LOGE("[REQUEST MACHINE | FAST] Creation tasks is failed");
                    _release_resources();
                    return 0;
                }
                return 1;
            }
            else
            {
                ROXEL_LOGE("[REQUEST MACHINE | FAST] Memory allocating failed");
                return 0;
            }
        }
    }
    else
    {
        ROXEL_LOGW("[REQUEST MACHINE | FAST] Request-objects is not passed");
    }
    return 0;
}

void FastRequestMachine::launch(void)
{
    _is_running = true;
    if (!_create_server() || !_start_server_task())
    {
        _stop_server_task();
        _is_running = false;
    }
}

void FastRequestMachine::stop(void)
{
    _stop_server_task();
    _stop_server();
    _is_running = false;
}

bool FastRequestMachine::isRunning(void) const
{
    return _is_running;
}

bool FastRequestMachine::_create_server(void)
{
    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (_socket < 0)
    {
        ROXEL_LOGE("[REQUEST MACHINE | FAST] Unable to create socket: errno %d", errno);
        return 0;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_udp_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ROXEL_LOGE("[REQUEST MACHINE | FAST] Socket bind failed: errno %d", errno);
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
        return 0;
    }

    ROXEL_LOGI("[REQUEST MACHINE | FAST] Server listening on port %d", _udp_port);
    return 1;
}

void FastRequestMachine::_stop_server(void)
{
    if (_socket < 0)
    {
        return;
    }
    ROXEL_LOGW("[REQUEST MACHINE | FAST] Server is stopped");
    shutdown(_socket, SHUT_RDWR);
    close(_socket);
    _socket = -1;
}

bool FastRequestMachine::_start_server_task(void)
{
    return xTaskCreate(__frm_udp_server_task__, "frm_server", 4096, this, 4, &_udp_server_task_handler) == pdPASS;
}

void FastRequestMachine::_stop_server_task(void)
{
    ROXEL_LOGI("[REQUEST MACHINE | FAST] Server task is killed");
    DELETE_TASK(_udp_server_task_handler);
}

bool FastRequestMachine::_initialize_query(void)
{
    _transaction_queue_handler = xQueueCreate(32, sizeof(FastRequestTransaction));
    _execution_queue_handler = xQueueCreate(32, sizeof(FastRequestExecution));
    return NOT_NULL(_transaction_queue_handler) && NOT_NULL(_execution_queue_handler);
}

bool FastRequestMachine::_start_tasks(void)
{
    bool result = true;
    result &= xTaskCreate(__frm_transaction_task__, "frm_transact", 4096, this, 4, &_transaction_task_handler) == pdPASS;
    result &= xTaskCreate(__frm_execution_task__, "frm_execute", 4096, this, 4, &_execution_task_handler) == pdPASS;
    return result;
}

size_t FastRequestMachine::_accept_requests(RequestImpl **requests, size_t count)
{
    size_t accepted = 0;
    for (size_t i = 0; i < count; i++)
    {
        RequestImpl *request = requests[i];
        if (NOT_NULL(request) && request->type() != RequestType::FAST)
            continue;
        int8_t result = _validate_request(request);
        accepted += result == 1 ? 1 : 0;
        if (result != 1)
        {
            ROXEL_LOGW("[REQUEST MACHINE | FAST] '%s' object is not passed in position %d ", NOT_NULL(request) ? request->id() : "NULL", i);
        }
    }
    return accepted;
}

int8_t FastRequestMachine::_validate_request(RequestImpl *request)
{
    if (IS_NULL(request))
    {
        return -1;
    }
    if (request->type() != RequestType::FAST)
    {
        return 0;
    }
    if (NOT_NULL(_requests))
    {
        for (size_t i = 0; i < _request_count; i++)
        {
            RequestImpl *instance = _requests[i];
            if (NOT_NULL(instance) && instance->hash() == request->hash())
            {
                return 0;
            }
        }
    }
    return 1;
}

bool FastRequestMachine::_allocate_requests(size_t size)
{
    _request_count = size;
    _request_hashes = CREATE_ARRAY(uint32_t, size);
    _requests = CREATE_ARRAY(RequestImpl *, size);
    return NOT_NULL(_requests) && NOT_NULL(_request_hashes);
}

void FastRequestMachine::_load_requests(RequestLoader *loader)
{
    size_t shift = 0;
    for (size_t i = 0; i < loader->count; i++)
    {
        RequestImpl *request = loader->requests[i];
        if (_validate_request(request) == 1)
        {
            request->_respond_queue = NULL;
            request->_position = shift;
            _request_hashes[shift] = request->hash();
            _requests[shift++] = request;
        }
    }
}

void FastRequestMachine::_clean_trash(void)
{
    if (IS_NULL(_requests) || IS_NULL(_request_hashes))
    {
        return;
    }
    for (size_t i = 0; i < _request_count; i++)
    {
        _request_hashes[i] = 0;
        _requests[i] = nullptr;
    }
}

void FastRequestMachine::_release_resources(void)
{
    DELETE_TASK(_transaction_task_handler);
    DELETE_TASK(_execution_task_handler);
    DELETE_QUEUE(_transaction_queue_handler);
    DELETE_QUEUE(_execution_queue_handler);
    DELETE_MUTEX(_instances_lock);
    DELETE(_request_hashes);
    DELETE(_requests);
}