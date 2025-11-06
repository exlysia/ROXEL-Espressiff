#include "cli_manager.h"
#include "esp_timer.h"

inline static void __x10__cli_shedule_task__(void *pvParameters)
{
    ROXEL_LOGI("[CLI-MANAGER] Sheduler task is started successfully");
    x10_cli_manager *cli_manager = static_cast<x10_cli_manager *>(pvParameters);
    while (NOT_NULL(cli_manager))
    {
        cli_manager->deactivate_suspicious();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    ROXEL_LOGW("[CLI-MANAGER] Sheduler task is stopped");
    vTaskDelete(NULL);
}

x10_cli_manager::x10_cli_manager(const char *token)
{
    _lock = xSemaphoreCreateMutex();
    _token = token;
    if (NOT_NULL(_token))
    {
        _token_size = strlen(token);
    }
}

x10_cli_manager::~x10_cli_manager()
{
    stop_sheduler();
}

void x10_cli_manager::shedule_clients(void)
{
    if (IS_NULL(_cli_task_shedule_handler))
    {
        xTaskCreate(__x10__cli_shedule_task__, "x10_cli_shed", 4096, this, 5, &_cli_task_shedule_handler);
    }
}

void x10_cli_manager::stop_sheduler(void)
{
    if (NOT_NULL(_cli_task_shedule_handler))
    {
        vTaskDelete(_cli_task_shedule_handler);
        _cli_task_shedule_handler = NULL;
    }
}

int x10_cli_manager::activate(x10_client *client)
{
    if (IS_NULL(_lock))
    {
        return 0;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    int result = -1;
    for (uint8_t i = 0; i < X10_MAX_CLIENTS; i++)
    {
        if (!_clients[i].activated)
        {
            strncpy(_clients[i].ip_buf, client->ip, sizeof(_clients[i].ip_buf) - 1);
            _clients[i].ip_buf[sizeof(_clients[i].ip_buf) - 1] = '\0';
            _clients[i].ip = _clients[i].ip_buf;
            _clients[i].hash = client->hash;
            _clients[i].port = client->port;
            _clients[i].sock = client->sock;
            _clients[i].created_at = ms();
            _clients[i].authorized = false;
            _clients[i].activated = true;
            result = i;
            break;
        }
    }
    xSemaphoreGive(_lock);
    return result;
}

void x10_cli_manager::deactivate(int index, bool with_lock)
{
    if (IS_NULL(_lock))
    {
        return;
    }
    if (with_lock)
    {
        xSemaphoreTake(_lock, portMAX_DELAY);
    }
    if (index > -1 && index < X10_MAX_CLIENTS)
    {
        if (_clients[index].activated)
        {
            _clients[index].activated = false;
            int sock = _clients[index].sock;
            if (sock >= 0)
            {
                shutdown(sock, SHUT_RDWR);
                close(sock);
                _clients[index].sock = -1;
            }
        }
    }
    if (with_lock)
    {
        xSemaphoreGive(_lock);
    }
}

void x10_cli_manager::deactivate_all(void)
{
    if (IS_NULL(_lock))
    {
        return;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    for (uint8_t i = 0; i < X10_MAX_CLIENTS; i++)
    {
        if (_clients[i].activated)
        {
            _clients[i].activated = false;
            int sock = _clients[i].sock;
            shutdown(sock, SHUT_RDWR);
            close(sock);
            _clients[i].sock = -1;
        }
    }
    xSemaphoreGive(_lock);
}

void x10_cli_manager::deactivate_suspicious(void)
{
    if (IS_NULL(_lock))
    {
        return;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    for (uint8_t i = 0; i < X10_MAX_CLIENTS; i++)
    {
        uint64_t now = ms();
        if (_clients[i].activated && ((!_clients[i].authorized && now - _clients[i].created_at >= X10_CLIENT_SUSPICIOUS_TIMEOUT) || (_clients[i].authorized && now - _clients[i].last_active_ms >= X10_CLIENT_NO_PING_TIMEOUT)))
        {
            _clients[i].activated = false;
            int sock = _clients[i].sock;
            shutdown(sock, SHUT_RDWR);
            close(sock);
            _clients[i].sock = -1;
        }
    }
    xSemaphoreGive(_lock);
}

int x10_cli_manager::find(uint32_t hash)
{
    if (IS_NULL(_lock))
    {
        return -1;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    int index = -1;
    for (uint8_t i = 0; i < X10_MAX_CLIENTS; i++)
    {
        if (_clients[i].activated && _clients[i].hash == hash)
        {
            index = i;
            break;
        }
    }
    xSemaphoreGive(_lock);
    return index;
}

size_t x10_cli_manager::token_size(void) const
{
    return _token_size;
}

const char *x10_cli_manager::token(void) const
{
    return _token;
}

bool x10_cli_manager::auth(uint32_t hash, const char *token)
{
    if (IS_NULL(_lock) || IS_NULL(token))
    {
        return 0;
    }
    int index = find(hash);
    if (index < 0)
    {
        return 0;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    bool result = 0;
    if (_clients[index].authorized)
    {
        result = 0;
    }
    else
    {
        if (strlen(token) != _token_size)
        {
            result = 0;
        }
        else if (strncmp(_token, token, _token_size) == 0)
        {
            _clients[index].last_active_ms = ms();
            _clients[index].authorized = true;
            result = 1;
        }
    }
    xSemaphoreGive(_lock);
    return result;
}

int x10_cli_manager::ping(uint32_t hash)
{
    if (IS_NULL(_lock))
    {
        return -2;
    }
    int index = find(hash);
    if (index < 0)
    {
        return -1;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    bool result = 0;
    if (_clients[index].authorized)
    {
        _clients[index].last_active_ms = ms();
        result = 1;
    }
    xSemaphoreGive(_lock);
    return result;
}

bool x10_cli_manager::send_to_client(int id, const void *dataptr, size_t size)
{
    if (IS_NULL(_lock) || IS_NULL(dataptr) || size < 1 || (id < 0 || id >= X10_MAX_CLIENTS))
    {
        return 0;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    bool result = 0;
    if (_clients[id].activated && _clients[id].authorized)
    {
        send(_clients[id].sock, dataptr, size, 0);
        result = 1;
    }
    xSemaphoreGive(_lock);
    return result;
}

void x10_cli_manager::broadcast(const void *dataptr, size_t size)
{
    if (IS_NULL(_lock) || IS_NULL(dataptr) || size < 1)
    {
        return;
    }
    xSemaphoreTake(_lock, portMAX_DELAY);
    for (uint8_t i = 0; i < X10_MAX_CLIENTS; i++)
    {
        if (_clients[i].activated && _clients[i].authorized)
        {
            send(_clients[i].sock, dataptr, size, 0);
        }
    }
    xSemaphoreGive(_lock);
}

uint64_t x10_cli_manager::ms(void)
{
    return esp_timer_get_time() / 1000ULL;
}