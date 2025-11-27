#ifndef _ROXEL_ROUTER_H_
#define _ROXEL_ROUTER_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../requests/full_machine.h"
#include "../requests/fast_machine.h"
#include "../utils/roxel_macro.h"
#include "../utils/cli_manager.h"
#include "../effects/machine.h"
#include "../net/tcp/ext.h"
#include "../update/ota.h"

class X10_router
{
public:
    X10_router(x10_cli_manager *cli_manager, uint16_t frm_port, QueueHandle_t *on_connect_handler, QueueHandle_t *message_handler);
    ~X10_router();
    void launch(void);
    void stop(void);
    void attachOverTheAir(OTA *ota);

    TaskHandle_t _router_on_connect_task_handler = NULL;
    TaskHandle_t _router_task_handler = NULL;

    QueueHandle_t *_on_connect_handler;
    QueueHandle_t *_message_handler;
    x10_cli_manager *_cli_manager;

    X10_RequestMachine *request_full_machine(void) const;
    FastRequestMachine *request_fast_machine(void) const;
    X10_EffectMachine *effect_machine(void) const;
    OTA *ota_instance(void) const;

private:
    FastRequestMachine *_fast_request_machine = nullptr;
    X10_RequestMachine *_full_request_machine = nullptr;
    X10_EffectMachine *_effect_machine = nullptr;
    OTA *_ota_instance = nullptr;
};

#endif