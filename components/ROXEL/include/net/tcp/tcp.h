#ifndef _ROXEL_TCP_H_
#define _ROXEL_TCP_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "lwip/sockets.h"

#include "../../utils/cli_manager.h"
#include "../../utils/roxel_macro.h"
#include "ext.h"

class roxel_tcp;

typedef struct
{
    x10_client *client;
    roxel_tcp *tcp;
    int id;
} x10_tcp_cli_transact;

class roxel_tcp
{
public:
    roxel_tcp(uint16_t port, x10_cli_manager *cli_manager);
    ~roxel_tcp();
    void launch(void);
    void stop(void);
    bool isRunning(void) const;

    QueueHandle_t _tcp_on_connect_queue_handler = NULL;
    QueueHandle_t _tcp_queue_handler = NULL;
    TaskHandle_t _tcp_task_handler = NULL;
    x10_cli_manager *_cli_manager;
    int _socket = -1;

    void _stop_server(void);

private:
    bool _is_running = 0;
    uint16_t _port;

    bool _create_server(void);
    bool _launch_task(void);
    void _stop_task(void);
    void _clear_queue(void);
};

#endif