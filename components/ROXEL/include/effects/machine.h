#ifndef _ROXEL_EFFECT_MACHINE_H_
#define _ROXEL_EFFECT_MACHINE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "../utils/machine_impl.h"
#include "../utils/cli_manager.h"
#include "../utils/roxel_macro.h"
#include "ext.h"

typedef struct
{
    const char *id;
    uint32_t hash = 0;
    int value;
    bool broadcast = false;
    int8_t client_id = -1;
} MediatorTransaction;

class X10_EffectMachine : public x10_machine_impl
{
public:
    X10_EffectMachine(x10_cli_manager *cli_manager);
    ~X10_EffectMachine();
    bool load(EffectLoader *loader);

    bool identify(uint32_t hash, cJSON *json) override;
    void on_connect(int client_id) override;
    void process(int client_id, uint32_t hash, const char *event, cJSON *json) override;
    void build_packet(void);

    QueueHandle_t _mediator_queue_handler = NULL;
    QueueHandle_t _hook_update_queue_handler = NULL;
    QueueHandle_t _hook_receive_queue_handler = NULL;

    TaskHandle_t _mediator_task_handler = NULL;
    TaskHandle_t _hook_receive_task_handler = NULL;

    SemaphoreHandle_t _buffer_lock = NULL;

    uint32_t *_effect_hashes = nullptr;
    int *_effect_states = nullptr;
    EffectImpl **_effects = nullptr;
    size_t _effect_count = 0;

    size_t single_content_size = 0;
    char *single_buffer = nullptr;

    size_t content_written = 0;
    size_t content_size = 0;
    char *buffer = nullptr;

    x10_cli_manager *_cli_manager;

private:
    uint32_t _identificator = 0;

    bool _initialize_query(void);
    bool _start_tasks(void);
    void _release_resources(void);

    size_t _accept_effects(EffectImpl **effects, size_t count);
    int8_t _validate_effect(EffectImpl *effect);
    bool _allocate_effects(size_t size);
    bool _allocate_single_payload_cache(EffectLoader *loader);
    bool _allocate_payload_cache(EffectLoader *loader);
    void _load_effects(EffectLoader *loader);
    void _clean_trash(void);
    void _free_buffer(void);
};

#endif