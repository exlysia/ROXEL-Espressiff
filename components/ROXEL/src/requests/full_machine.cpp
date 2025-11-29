#include "requests/full_machine.h"

inline static const char *REQUEST_EVENT_ID = "req";

inline static void __x10__request_machine_send__(X10_RequestMachine *machine, int client_id, const char *id, cJSON *respond)
{
    if (IS_NULL(respond))
    {
        return;
    }
    cJSON *response = cJSON_CreateObject();
    if (IS_NULL(respond))
    {
        cJSON_Delete(respond);
        return;
    }
    cJSON_AddStringToObject(response, "e", REQUEST_EVENT_ID);
    cJSON_AddStringToObject(response, "n", id);
    cJSON_AddItemToObject(response, "d", respond);
    if (cJSON_PrintPreallocated(response, machine->respond_buffer, machine->respond_buffer_size, false))
    {
        size_t written = strlen(machine->respond_buffer);
        machine->respond_buffer[written++] = '\r';
        machine->respond_buffer[written++] = '\n';
        machine->respond_buffer[written++] = '\r';
        machine->respond_buffer[written++] = '\n';
        if (written < machine->respond_buffer_size)
        {
            machine->respond_buffer[written] = '\0';
        }
        if (client_id > -1)
        {
            machine->_cli_manager->send_to_client(client_id, machine->respond_buffer, written);
        }
        else
        {
            machine->_cli_manager->broadcast(machine->respond_buffer, written);
        }
    }
    cJSON_Delete(response);
}

inline static void __x10__request_machine_respond_task__(void *pvParameters)
{
    X10_RequestMachine *machine = static_cast<X10_RequestMachine *>(pvParameters);
    RequestRespond respond;

    ROXEL_LOGI("[REQUEST MACHINE | FULL] Respond task is started");
    while (NOT_NULL(machine))
    {
        if (IS_NULL(machine->_respond_queue_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(machine->_respond_queue_handler, &respond, portMAX_DELAY))
        {
            __x10__request_machine_send__(machine, respond.client_id, respond.id, respond.respond);
        }
    }
    vTaskDelete(NULL);
}

inline static void __x10__request_machine_execution_task__(void *pvParameters)
{
    X10_RequestMachine *machine = static_cast<X10_RequestMachine *>(pvParameters);
    RequestExecution execution;

    ROXEL_LOGI("[REQUEST MACHINE | FULL] Execution task is started");
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
            if (execution.connect)
            {
                if (NOT_NULL(instance))
                {
                    instance->update_connect(execution.client_id);
                }
                else
                {
                    for (size_t i = 0; i < machine->_request_count; i++)
                    {
                        if (NOT_NULL(machine->_requests[i]))
                        {
                            machine->_requests[i]->update_connect(execution.client_id);
                        }
                    }
                }
            }
            else
            {
                if (NOT_NULL(instance))
                {
                    cJSON *data = cJSON_ParseWithOpts(execution.data, NULL, false);
                    if (data)
                    {
                        instance->update_call(execution.client_id, data);
                        cJSON_Delete(data);
                    }
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

inline static void __x10__request_machine_transaction_task__(void *pvParameters)
{
    X10_RequestMachine *machine = static_cast<X10_RequestMachine *>(pvParameters);
    RequestTransaction transaction;

    ROXEL_LOGI("[REQUEST MACHINE | FULL] Transaction task is started");
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
            if (!transaction.connect)
            {
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
            }
            RequestExecution execution;
            execution.client_id = transaction.client_id;
            execution.connect = transaction.connect;
            execution.position = position;
            if (!transaction.connect)
            {
                execution.data = transaction.data;
                transaction.data = nullptr;
            }
            else
            {
                execution.data = nullptr;
            }
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

X10_RequestMachine::X10_RequestMachine(x10_cli_manager *cli_manager)
{
    _instances_lock = xSemaphoreCreateMutex();
    _identificator = CRC32(REQUEST_EVENT_ID);
    _cli_manager = cli_manager;
}

X10_RequestMachine::~X10_RequestMachine()
{
    _release_resources();
}

bool X10_RequestMachine::load(RequestLoader *loader)
{
    if (IS_NULL(loader) || IS_NULL(loader->requests))
    {
        ROXEL_LOGW("[REQUEST MACHINE | FULL] Request-objects is NULL or not passed");
        return 0;
    }
    if (loader->count > 0)
    {
        _request_count = loader->count;
        size_t accepted = _accept_requests(loader->requests, loader->count);
        ROXEL_LOG(accepted > 0 ? ESP_LOG_INFO : ESP_LOG_ERROR, "[REQUEST MACHINE | FULL] Accepted objects: %d", accepted);
        if (accepted > 0)
        {
            if (_allocate_requests(accepted) && _allocate_respond_cache())
            {
                if (!_initialize_query())
                {
                    ROXEL_LOGE("[REQUEST MACHINE | FULL] Queue initialization is failed");
                    _release_resources();
                    return 0;
                }
                _clean_trash();
                _load_requests(loader);
                if (!_start_tasks())
                {
                    ROXEL_LOGE("[REQUEST MACHINE | FULL] Creation tasks is failed");
                    _release_resources();
                    return 0;
                }
                return 1;
            }
            else
            {
                ROXEL_LOGE("[REQUEST MACHINE | FULL] Memory allocating failed");
                return 0;
            }
        }
    }
    else
    {
        ROXEL_LOGW("[REQUEST MACHINE | FULL] Request-objects is not passed");
    }
    return 0;
}

bool X10_RequestMachine::identify(uint32_t hash, cJSON *json)
{
    return hash == _identificator;
}

void X10_RequestMachine::on_connect(int client_id)
{
    if (IS_NULL(_transaction_queue_handler))
    {
        return;
    }
    RequestTransaction transaction;
    transaction.client_id = client_id;
    transaction.data = nullptr;
    transaction.connect = 1;
    xQueueSend(_transaction_queue_handler, &transaction, portMAX_DELAY);
}

void X10_RequestMachine::process(int client_id, uint32_t hash, const char *event, cJSON *json)
{
    if (IS_NULL(_transaction_queue_handler))
    {
        return;
    }
    const char *id = nullptr;
    cJSON *data_item = cJSON_GetObjectItem(json, "d");
    if (IS_NULL((id = cJSON_GetStringValue(cJSON_GetObjectItem(json, "n")))) || IS_NULL(data_item) || !cJSON_IsObject(data_item))
    {
        return;
    }
    RequestTransaction transaction;
    transaction.data = cJSON_PrintUnformatted(data_item);
    transaction.hash = CRC32(id);
    transaction.client_id = client_id;
    transaction.connect = 0;
    if (NOT_NULL(transaction.data))
    {
        if (xQueueSend(_transaction_queue_handler, &transaction, portMAX_DELAY) != pdPASS)
        {
            vPortFree(transaction.data);
        }
    }
}

bool X10_RequestMachine::_initialize_query(void)
{
    _transaction_queue_handler = xQueueCreate(32, sizeof(RequestTransaction));
    _execution_queue_handler = xQueueCreate(32, sizeof(RequestExecution));
    _respond_queue_handler = xQueueCreate(32, sizeof(RequestRespond));
    return NOT_NULL(_transaction_queue_handler) && NOT_NULL(_execution_queue_handler) && NOT_NULL(_respond_queue_handler);
}

bool X10_RequestMachine::_start_tasks(void)
{
    bool result = true;
    result &= xTaskCreate(__x10__request_machine_transaction_task__, "x10_req_transact", 4096, this, 4, &_transaction_task_handler) == pdPASS;
    result &= xTaskCreate(__x10__request_machine_execution_task__, "x10_req_exec", 4096, this, 4, &_execution_task_handler) == pdPASS;
    result &= xTaskCreate(__x10__request_machine_respond_task__, "x10_req_resp", 4096, this, 4, &_respond_task_handler) == pdPASS;
    return result;
}

void X10_RequestMachine::_release_resources(void)
{
    DELETE_TASK(_transaction_task_handler);
    DELETE_TASK(_execution_task_handler);
    DELETE_TASK(_respond_task_handler);
    DELETE_QUEUE(_transaction_queue_handler);
    DELETE_QUEUE(_execution_queue_handler);
    DELETE_QUEUE(_respond_queue_handler);
    DELETE_MUTEX(_instances_lock);
    DELETE(_request_hashes);
    DELETE(_requests);
    _free_buffer();
}

size_t X10_RequestMachine::_accept_requests(RequestImpl **requests, size_t count)
{
    size_t accepted = 0;
    for (size_t i = 0; i < count; i++)
    {
        RequestImpl *request = requests[i];
        if (NOT_NULL(request) && request->type() != RequestType::FULL)
            continue;
        int8_t result = _validate_request(request);
        accepted += result == 1 ? 1 : 0;
        if (result != 1)
        {
            ROXEL_LOGW("[REQUEST MACHINE | FULL] '%s' object is not passed in position %d ", NOT_NULL(request) ? request->id() : "NULL", i);
        }
    }
    return accepted;
}

int8_t X10_RequestMachine::_validate_request(RequestImpl *request)
{
    if (IS_NULL(request))
    {
        return -1;
    }
    if (request->type() != RequestType::FULL)
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

bool X10_RequestMachine::_allocate_requests(size_t size)
{
    _request_count = size;
    _request_hashes = CREATE_ARRAY(uint32_t, size);
    _requests = CREATE_ARRAY(RequestImpl *, size);
    return NOT_NULL(_requests) && NOT_NULL(_request_hashes);
}

bool X10_RequestMachine::_allocate_respond_cache(void)
{
    respond_buffer_size = 1024;
    ALLOCATE_BUFFER_MEMORY(respond_buffer, respond_buffer_size);
    return NOT_NULL(respond_buffer);
}

void X10_RequestMachine::_load_requests(RequestLoader *loader)
{
    size_t shift = 0;
    for (size_t i = 0; i < loader->count; i++)
    {
        RequestImpl *request = loader->requests[i];
        if (_validate_request(request) == 1)
        {
            request->_respond_queue = _respond_queue_handler;
            request->_position = shift;
            _request_hashes[shift] = request->hash();
            _requests[shift++] = request;
        }
    }
}

void X10_RequestMachine::_clean_trash(void)
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

void X10_RequestMachine::_free_buffer(void)
{
    if (NOT_NULL(respond_buffer))
    {
        heap_caps_free(respond_buffer);
        respond_buffer = nullptr;
    }
}