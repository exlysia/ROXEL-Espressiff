#include "roxel_udp.h"
#include <arpa/inet.h>

inline static void __x10__udp_task__(void *pvParameters)
{
    roxel_udp *udp = static_cast<roxel_udp *>(pvParameters);
    char rx_buffer[128];
    char addr_str[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    const char *PING_MESSAGE = "VERIFY";
    const char *PONG_WAIT_AUTH_MESSAGE = "WAIT_AUTH";
    const char *PONG_SUCCESS_MESSAGE = "OK";
    const char *PONG_CONNECT_MESSAGE = "CONNECT";
    const char *PONG_UNAVAILABLE_MESSAGE = "UNAVAILABLE";
    while (udp->isRunning())
    {
        int sock = udp->_socket;
        if (sock < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (len >= 0)
        {
            rx_buffer[len] = 0;

            if (strncmp(rx_buffer, PING_MESSAGE, strlen(PING_MESSAGE)) == 0)
            {
                inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
                int state = udp->_cli_manager->ping(ntohl(client_addr.sin_addr.s_addr));
                const char *message = NULL;
                switch (state)
                {
                case 0:
                    message = PONG_WAIT_AUTH_MESSAGE;
                    break;

                case 1:
                    message = PONG_SUCCESS_MESSAGE;
                    break;

                case -1:
                    message = PONG_CONNECT_MESSAGE;
                    break;

                default:
                    message = PONG_UNAVAILABLE_MESSAGE;
                    break;
                }
                if (NOT_NULL(message))
                {
                    sendto(sock, message, strlen(message), 0, (struct sockaddr *)&client_addr, addr_len);
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    if (NOT_NULL(udp))
    {
        udp->_stop_server();
        udp->_udp_task_handler = NULL;
    }
    vTaskDelete(NULL);
}

roxel_udp::roxel_udp(uint16_t port, uint16_t fast_port, x10_cli_manager *cli_manager)
{
    _cli_manager = cli_manager;
    _fast_port = port;
    _port = port;
}

roxel_udp::~roxel_udp()
{
    stop();
}

void roxel_udp::launch(void)
{
    _is_running = true;
    if (!_create_server() || !_launch_task())
    {
        _stop_server();
        _is_running = false;
    }
}

void roxel_udp::stop(void)
{
    _stop_task();
    _stop_server();
    _is_running = false;
}

bool roxel_udp::isRunning(void) const
{
    return _is_running;
}

bool roxel_udp::_create_server(void)
{
    _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (_socket < 0)
    {
        ROXEL_LOGE("[UDP] Unable to create socket: errno %d", errno);
        return 0;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        ROXEL_LOGE("[UDP] Socket bind failed: errno %d", errno);
        shutdown(_socket, SHUT_RDWR);
        close(_socket);
        _socket = -1;
        return 0;
    }

    ROXEL_LOGI("[UDP] Server listening on port %d", _port);
    return 1;
}

bool roxel_udp::_launch_task(void)
{
    bool result = xTaskCreate(__x10__udp_task__, "x10.udp", 4096, this, 5, &_udp_task_handler) == pdPASS;
    if (!result)
    {
        ROXEL_LOGE("[UDP] Server task is not started... Aborting.");
    }
    return result;
}

void roxel_udp::_stop_server(void)
{
    if (_socket < 0)
    {
        return;
    }
    ROXEL_LOGW("[UDP] Server is stopped");
    shutdown(_socket, SHUT_RDWR);
    close(_socket);
    _socket = -1;
}

void roxel_udp::_stop_task(void)
{
    ROXEL_LOGI("[UDP] Server task is killed");
    DELETE_TASK(_udp_task_handler);
}