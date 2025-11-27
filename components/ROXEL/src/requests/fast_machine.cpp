#include "requests/fast_machine.h"

inline static void __frm_udp_server_task__(void *pvParameters)
{
    while (1)
    {
        vTaskDelay(1000);
    }
    vTaskDelete(NULL);
}

FastRequestMachine::FastRequestMachine(uint16_t port, x10_cli_manager *cli_manager)
{
    _cli_manager = cli_manager;
    _udp_port = port;
}

FastRequestMachine::~FastRequestMachine()
{
    _release_resources();
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

bool FastRequestMachine::_start_tasks(void)
{
    bool result = true;
    return result;
}

size_t FastRequestMachine::_accept_requests(RequestImpl **requests, size_t count)
{
    size_t accepted = 0;
    for (size_t i = 0; i < count; i++)
    {
        RequestImpl *request = requests[i];
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
    if (NOT_NULL(_requests))
    {
        for (size_t i = 0; i < _request_count; i++)
        {
            RequestImpl *instance = _requests[i];
            if (NOT_NULL(instance) && instance->hash() == request->hash())
            {
                return 0;
            }
            if (instance->type() != RequestType::FAST)
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
    DELETE(_request_hashes);
    DELETE(_requests);
}