#include "router/router.h"

inline static void __x10__router_processing__(int client_id, X10_router *router, const char *event, cJSON *json)
{
    X10_RequestMachine *requests = router->request_full_machine();
    X10_EffectMachine *effects = router->effect_machine();
    OTA *ota = router->ota_instance();
    uint32_t ev_hash = CRC32(event);
    if (NOT_NULL(requests) && requests->identify(ev_hash, json))
    {
        requests->process(client_id, ev_hash, event, json);
        return;
    }
    if (NOT_NULL(effects) && effects->identify(ev_hash, json))
    {
        effects->process(client_id, ev_hash, event, json);
        return;
    }
    if (NOT_NULL(ota) && ota->identify(ev_hash))
    {
        ota->launch();
        return;
    }
}

inline static void __x10__router_on_connect__(X10_router *router, int client_id)
{
    X10_RequestMachine *requests = router->request_full_machine();
    X10_EffectMachine *effects = router->effect_machine();
    if (NOT_NULL(requests))
    {
        requests->on_connect(client_id);
    }
    if (NOT_NULL(effects))
    {
        effects->on_connect(client_id);
    }
}

inline static void __x10__router_on_connect_task__(void *pvParameters)
{
    X10_router *router = static_cast<X10_router *>(pvParameters);
    x10_tcp_on_connect event;
    ROXEL_LOGI("[ROUTER] Connect listener task is started successfully");
    while (1)
    {
        if (IS_NULL(router->_on_connect_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(*router->_on_connect_handler, &event, portMAX_DELAY) == pdPASS)
        {
            __x10__router_on_connect__(router, event.client_id);
        }
    }
    if (NOT_NULL(router->_router_on_connect_task_handler))
    {
        router->_router_on_connect_task_handler = NULL;
    }
    vTaskDelete(NULL);
}

inline static void __x10__router_task__(void *pvParameters)
{
    X10_router *router = static_cast<X10_router *>(pvParameters);
    x10_tcp_message msg;
    ROXEL_LOGI("[ROUTER] Message receiver task is started successfully");
    while (1)
    {
        if (IS_NULL(router->_message_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(*router->_message_handler, &msg, portMAX_DELAY) == pdPASS)
        {
            cJSON *json = cJSON_ParseWithOpts(msg.message, NULL, false);
            const char *event = NULL;
            if (!json || !cJSON_IsObject(json) || IS_NULL((event = cJSON_GetStringValue(cJSON_GetObjectItem(json, "e")))))
            {
                if (NOT_NULL(json))
                {
                    cJSON_Delete(json);
                }
                vPortFree(msg.message);
                continue;
            }
            __x10__router_processing__(msg.id, router, event, json);
            cJSON_Delete(json);
            vPortFree(msg.message);
        }
    }
    if (NOT_NULL(router))
    {
        router->_router_task_handler = NULL;
    }
    vTaskDelete(NULL);
}

X10_router::X10_router(x10_cli_manager *cli_manager, uint16_t frm_port, QueueHandle_t *on_connect_handler, QueueHandle_t *message_handler)
{
    _fast_request_machine = CREATE(FastRequestMachine, frm_port, cli_manager);
    _full_request_machine = CREATE(X10_RequestMachine, cli_manager);
    _effect_machine = CREATE(X10_EffectMachine, cli_manager);
    _on_connect_handler = on_connect_handler;
    _message_handler = message_handler;
    _cli_manager = cli_manager;
}

X10_router::~X10_router()
{
    stop();
}

void X10_router::launch(void)
{
    if (IS_NULL(_router_on_connect_task_handler))
    {
        xTaskCreate(__x10__router_on_connect_task__, "x10_router_conn", 8192, this, 4, &_router_on_connect_task_handler);
    }
    if (IS_NULL(_router_task_handler))
    {
        xTaskCreate(__x10__router_task__, "x10_router", 8192, this, 4, &_router_task_handler);
    }
    if (NOT_NULL(_fast_request_machine))
    {
        _fast_request_machine->launch();
    }
}

void X10_router::stop(void)
{
    if (NOT_NULL(_router_on_connect_task_handler))
    {
        ROXEL_LOGW("[ROUTER] Connect listener task is killed");
        DELETE_TASK(_router_on_connect_task_handler);
    }
    if (NOT_NULL(_router_task_handler))
    {
        ROXEL_LOGW("[ROUTER] Message receiver task is killed");
        DELETE_TASK(_router_task_handler);
    }
    if (NOT_NULL(_fast_request_machine))
    {
        _fast_request_machine->stop();
    }
}

void X10_router::attachOverTheAir(OTA *ota)
{
    if (IS_NULL(_ota_instance))
    {
        _ota_instance = ota;
    }
}

X10_RequestMachine *X10_router::request_full_machine(void) const
{
    return _full_request_machine;
}

FastRequestMachine *X10_router::request_fast_machine(void) const
{
    return _fast_request_machine;
}

X10_EffectMachine *X10_router::effect_machine(void) const
{
    return _effect_machine;
}

OTA *X10_router::ota_instance(void) const
{
    return _ota_instance;
}