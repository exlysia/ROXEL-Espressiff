#ifndef _ROXEL_UDP_H_
#define _ROXEL_UDP_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "roxel_macro.h"

#include "cli_manager.h"
#include "lwip/sockets.h"

class roxel_udp
{
public:
    roxel_udp(uint16_t port, x10_cli_manager *cli_manager);
    ~roxel_udp();
    void launch(void);
    void stop(void);
    bool isRunning(void) const;

    x10_cli_manager *_cli_manager;
    TaskHandle_t _udp_task_handler;
    int _socket = -1;

    void _stop_server(void);

private:
    bool _is_running = 0;
    uint16_t _port;

    bool _create_server(void);
    bool _launch_task(void);
    void _stop_task(void);
};

#endif