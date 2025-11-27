#include "requests/fast_machine.h"

inline static void __frm_udp_server_task__(void *pvParameters)
{
    FastRequestMachine *machine = static_cast<FastRequestMachine *>(pvParameters);
    char rx_buffer[128];
    char addr_str[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
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
            rx_buffer[len] = 0;

            inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
            int state = machine->_cli_manager->ping(ntohl(client_addr.sin_addr.s_addr));
            if (state != 1)
                continue;

            ROXEL_LOGW("[REQUEST MACHINE | FAST] Package: %s", rx_buffer);
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
                _clean_trash();
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
    if (!_create_server() || !_start_tasks())
    {
        _stop_server();
        _is_running = false;
    }
}

void FastRequestMachine::stop(void)
{
    _stop_task();
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

void FastRequestMachine::_stop_task(void)
{
    ROXEL_LOGI("[REQUEST MACHINE | FAST] Server task is killed");
    DELETE_TASK(_udp_server_task_handler);
}

bool FastRequestMachine::_start_tasks(void)
{
    return xTaskCreate(__frm_udp_server_task__, "frm_server", 4096, this, 4, &_udp_server_task_handler) == pdPASS;
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
    _stop_task();
    DELETE(_request_hashes);
    DELETE(_requests);
}