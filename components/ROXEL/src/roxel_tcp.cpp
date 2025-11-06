#include "roxel_tcp.h"

static const char *X10_CONTENT_AUTH_HEAD = "AUTH";
static const size_t X10_CONTENT_AUTH_HEAD_SIZE = strlen(X10_CONTENT_AUTH_HEAD);
static const char *X10_CONTENT_DIVIDER = ":";

inline static void __x10__client_payload_process__(x10_tcp_cli_transact *transaction, bool *is_authorized, char *buffer, int len)
{
    if (!(*is_authorized))
    {
        bool result = true;
        result &= strncmp(buffer, X10_CONTENT_AUTH_HEAD, X10_CONTENT_AUTH_HEAD_SIZE) == 0;
        result &= strncmp(buffer + X10_CONTENT_AUTH_HEAD_SIZE, X10_CONTENT_DIVIDER, 1) == 0;
        if (result)
        {
            if (transaction->tcp->_cli_manager->auth(transaction->client->hash, buffer + X10_CONTENT_AUTH_HEAD_SIZE + 1))
            {
                x10_tcp_on_connect connect;
                connect.client_id = transaction->id;
                xQueueSend(transaction->tcp->_tcp_on_connect_queue_handler, &connect, portMAX_DELAY);
                *is_authorized = 1;
                return;
            }
        }
    }
    else
    {
        if (transaction->tcp->_cli_manager->ping(transaction->client->hash))
        {
            x10_tcp_message message;
            message.message = (char *)pvPortMalloc(len + 1);
            if (IS_NULL(message.message))
            {
                return;
            }
            strcpy(message.message, buffer);
            message.id = transaction->id;
            message.length = len;

            if (xQueueSend(transaction->tcp->_tcp_queue_handler, &message, 0) != pdPASS)
            {
                vPortFree(message.message);
            }
        }
    }
}

inline static void __x10__client_tcp_task__(void *pvParameters)
{
    x10_tcp_cli_transact *transaction = static_cast<x10_tcp_cli_transact *>(pvParameters);
    int sock = NOT_NULL(transaction) && NOT_NULL(transaction->client) ? transaction->client->sock : -1;
    bool is_authorized = 0;

    char rx_buffer[512];
    char buffer[256];
    int frame_len = 0;

    if (NOT_NULL(transaction) && NOT_NULL(transaction->client))
    {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int flag = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    }

    ROXEL_LOGI("[TCP-CLI] Task started (sock=%d)", sock);

    while (NOT_NULL(transaction) && NOT_NULL(transaction->tcp) && transaction->tcp->isRunning())
    {
        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len > 0)
        {
            rx_buffer[len] = '\0';

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
                if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
                    buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
                {

                    buffer[i] = '\0';
                    __x10__client_payload_process__(transaction, &is_authorized, buffer, i);
                    size_t remain = frame_len - (i + 4);
                    memmove(buffer, buffer + i + 4, remain);
                    frame_len = remain;
                    i = 0;
                    continue;
                }
                i++;
            }
        }
        else if (len == 0)
        {
            ROXEL_LOGW("[TCP-CLI] Client closed connection (sock=%d)", sock);
            break;
        }
        else
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                continue;
            }
            else
            {
                ROXEL_LOGE("[TCP-CLI] recv failed: errno=%d", errno);
                break;
            }
        }
    }
    ROXEL_LOGI("[TCP-CLI] Task finished (sock=%d)", sock);
    if (NOT_NULL(transaction))
    {
        if (NOT_NULL(transaction->client))
        {
            int index = transaction->tcp->_cli_manager->find(transaction->client->hash);
            if (index > -1)
            {
                transaction->tcp->_cli_manager->deactivate(index);
            }
            vPortFree(transaction->client);
        }
        vPortFree(transaction);
    }
    vTaskDelete(NULL);
}

inline static void __x10__server_tcp_task__(void *pvParameters)
{
    roxel_tcp *tcp = static_cast<roxel_tcp *>(pvParameters);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (tcp->isRunning())
    {
        int client_sock = accept(tcp->_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock >= 0)
        {
            ROXEL_LOGI("[TCP] New client connected [%s:%d]", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            x10_tcp_cli_transact *transaction = (x10_tcp_cli_transact *)pvPortMalloc(sizeof(x10_tcp_cli_transact));
            if (IS_NULL(transaction))
            {
                continue;
            }
            transaction->client = (x10_client *)pvPortMalloc(sizeof(x10_client));
            transaction->tcp = tcp;
            if (NOT_NULL(transaction->client))
            {
                snprintf(transaction->client->ip_buf, sizeof(transaction->client->ip_buf), "%s", inet_ntoa(client_addr.sin_addr));
                transaction->client->port = ntohs(client_addr.sin_port);
                transaction->client->sock = client_sock;
                transaction->client->ip = transaction->client->ip_buf;
                transaction->client->hash = ntohl(client_addr.sin_addr.s_addr);
                transaction->id = tcp->_cli_manager->activate(transaction->client);
                if (transaction->id > -1)
                {
                    if (xTaskCreate(__x10__client_tcp_task__, "x10_tcp_cli", 4096, transaction, 5, NULL) == pdPASS)
                    {
                        continue;
                    }
                }
            }
            if (NOT_NULL(transaction->client))
                vPortFree(transaction->client);
            vPortFree(transaction);
        }
        else
        {
            if (errno == EBADF)
                break;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (NOT_NULL(tcp))
    {
        tcp->_stop_server();
        tcp->_tcp_task_handler = NULL;
    }
    vTaskDelete(NULL);
}

roxel_tcp::roxel_tcp(uint16_t port, x10_cli_manager *cli_manager)
{
    _cli_manager = cli_manager;
    _port = port;

    _tcp_on_connect_queue_handler = xQueueCreate(10, sizeof(x10_tcp_on_connect));
    _tcp_queue_handler = xQueueCreate(10, sizeof(x10_tcp_message));
    if (IS_NULL(_tcp_queue_handler))
    {
        ROXEL_LOGW("[TCP] Failed to create message queue");
    }
}

roxel_tcp::~roxel_tcp()
{
    stop();
    if (NOT_NULL(_tcp_queue_handler))
    {
        vQueueDelete(_tcp_queue_handler);
    }
}

void roxel_tcp::launch(void)
{
    _is_running = true;
    if (!_create_server() || !_launch_task())
    {
        _stop_server();
        _is_running = false;
    }
}

void roxel_tcp::stop(void)
{
    _stop_task();
    _stop_server();
    _clear_queue();
    _is_running = false;
}

bool roxel_tcp::isRunning(void) const
{
    return _is_running;
}

bool roxel_tcp::_create_server(void)
{
    _socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (_socket < 0)
    {
        ROXEL_LOGE("[TCP] Unable to create socket: errno %d", errno);
        return 0;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ROXEL_LOGE("[TCP] Socket bind failed: errno %d", errno);
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
        return 0;
    }

    if (listen(_socket, 5) < 0)
    {
        ROXEL_LOGE("[TCP] Listen failed: errno %d", errno);
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
        return false;
    }

    ROXEL_LOGI("[TCP] Server listening on port %d", _port);
    return 1;
}

bool roxel_tcp::_launch_task(void)
{
    bool result = xTaskCreate(__x10__server_tcp_task__, "x10.tcp", 4096, this, 5, &_tcp_task_handler) == pdPASS;
    if (!result)
    {
        ROXEL_LOGE("[TCP] Server task is not started... Aborting.");
    }
    return result;
}

void roxel_tcp::_stop_server(void)
{
    if (_socket < 0)
    {
        return;
    }
    if (NOT_NULL(_cli_manager))
    {
        _cli_manager->deactivate_all();
    }
    ROXEL_LOGW("[TCP] Server is shutting down...");
    shutdown(_socket, SHUT_RDWR);
    close(_socket);
    _socket = -1;
    ROXEL_LOGW("[TCP] Server socket closed");
}

void roxel_tcp::_stop_task(void)
{
    ROXEL_LOGI("[TCP] Server task is killed");
    DELETE_TASK(_tcp_task_handler);
}

void roxel_tcp::_clear_queue(void)
{
    if (NOT_NULL(_tcp_queue_handler))
    {
        xQueueReset(_tcp_queue_handler);
    }
}