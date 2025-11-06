#include "effect_machine.h"

inline static const char *EFFECT_EVENT_ID = "ef";
inline static int EFFECT_EVENT_ID_LEN = 2;

inline static void __x10__effect_machine_hook_receive_task__(void *pvParameters)
{
    X10_EffectMachine *machine = static_cast<X10_EffectMachine *>(pvParameters);
    EffectUpdate update;

    ROXEL_LOGI("[EFFECT MACHINE] Hook updates receive task is started");
    while (NOT_NULL(machine))
    {
        if (IS_NULL(machine->_buffer_lock) || IS_NULL(machine->_hook_receive_queue_handler) || IS_NULL(machine->_mediator_queue_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(machine->_hook_receive_queue_handler, &update, portMAX_DELAY))
        {
            if (update.position < machine->_effect_count)
            {
                if (update.value != machine->_effect_states[update.position])
                {
                    xSemaphoreTake(machine->_buffer_lock, portMAX_DELAY);
                    machine->_effect_states[update.position] = update.value;
                    machine->build_packet();
                    xSemaphoreGive(machine->_buffer_lock);
                    if (update.notify)
                    {
                        MediatorTransaction transaction;
                        transaction.id = machine->_effects[update.position]->id();
                        transaction.value = update.value;
                        transaction.broadcast = true;
                        xQueueSend(machine->_mediator_queue_handler, &transaction, portMAX_DELAY);
                    }
                    else
                    {
                        machine->_effects[update.position]->update(update.value);
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}

inline static void __x10__effect_machine_broadcast(X10_EffectMachine *machine, const char *id, int value)
{
    xSemaphoreTake(machine->_buffer_lock, portMAX_DELAY);
    int written = snprintf(machine->single_buffer, machine->single_content_size, "{\"e\":\"%s\",\"l\":{\"%s\":%d}}\r\n\r\n", EFFECT_EVENT_ID, id, value);
    if (written > 0)
    {
        machine->_cli_manager->broadcast(machine->single_buffer, written);
    }
    xSemaphoreGive(machine->_buffer_lock);
}

inline static void __x10__effect_machine_send_on_connect(X10_EffectMachine *machine, int client_id)
{
    xSemaphoreTake(machine->_buffer_lock, portMAX_DELAY);
    machine->_cli_manager->send_to_client(client_id, machine->buffer, machine->content_written);
    xSemaphoreGive(machine->_buffer_lock);
}

inline static void __x10__effect_machine_mediator_task__(void *pvParameters)
{
    X10_EffectMachine *machine = static_cast<X10_EffectMachine *>(pvParameters);
    MediatorTransaction transaction;

    ROXEL_LOGI("[EFFECT MACHINE] Mediator task is started");
    while (NOT_NULL(machine))
    {
        if (IS_NULL(machine->_buffer_lock) || IS_NULL(machine->_hook_receive_queue_handler) || IS_NULL(machine->_mediator_queue_handler))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (xQueueReceive(machine->_mediator_queue_handler, &transaction, portMAX_DELAY))
        {
            if (transaction.broadcast)
            {
                if (transaction.client_id < 0)
                {
                    __x10__effect_machine_broadcast(machine, transaction.id, transaction.value);
                }
                else
                {
                    __x10__effect_machine_send_on_connect(machine, transaction.client_id);
                }
            }
            else
            {
                uint32_t hash = IS_NULL(transaction.id) ? transaction.hash : CRC32(transaction.id);
                int8_t position = -1;
                for (size_t i = 0; i < machine->_effect_count; i++)
                {
                    if (machine->_effect_hashes[i] == hash)
                    {
                        position = i;
                        break;
                    }
                }
                if (position > -1)
                {
                    EffectUpdate update;
                    update.value = transaction.value;
                    update.position = position;
                    update.notify = false;
                    xQueueSend(machine->_hook_receive_queue_handler, &update, portMAX_DELAY);
                }
            }
        }
    }
    vTaskDelete(NULL);
}

X10_EffectMachine::X10_EffectMachine(x10_cli_manager *cli_manager) : x10_machine_impl()
{
    _buffer_lock = xSemaphoreCreateMutex();
    _identificator = CRC32(EFFECT_EVENT_ID);
    _cli_manager = cli_manager;
}

X10_EffectMachine::~X10_EffectMachine()
{
    _release_resources();
}

bool X10_EffectMachine::load(EffectLoader *loader)
{
    if (IS_NULL(loader) || IS_NULL(loader->effects))
    {
        ROXEL_LOGW("[EFFECT MACHINE] Effect-objects is NULL or not passed");
        return 0;
    }
    if (loader->count > 0)
    {
        _effect_count = loader->count;
        size_t accepted = _accept_effects(loader->effects, loader->count);
        ROXEL_LOG(accepted > 0 ? ESP_LOG_INFO : ESP_LOG_ERROR, "[EFFECT MACHINE] Accepted objects: %d", accepted);
        if (accepted > 0)
        {
            if (_allocate_effects(accepted) && _allocate_payload_cache(loader) && _allocate_single_payload_cache(loader))
            {
                if (!_initialize_query())
                {
                    ROXEL_LOGE("[EFFECT MACHINE] Queue initialization is failed");
                    _release_resources();
                    return 0;
                }
                _clean_trash();
                _load_effects(loader);
                build_packet();
                if (!_start_tasks())
                {
                    ROXEL_LOGE("[EFFECT MACHINE] Creation tasks is failed");
                    _release_resources();
                    return 0;
                }
                return 1;
            }
            else
            {
                ROXEL_LOGE("[EFFECT MACHINE] Memory allocating failed");
                return 0;
            }
        }
    }
    else
    {
        ROXEL_LOGW("[EFFECT MACHINE] Effect-objects is not passed");
    }
    return 0;
}

bool X10_EffectMachine::identify(uint32_t hash, cJSON *json)
{
    return hash == _identificator;
}

void X10_EffectMachine::on_connect(int client_id)
{
    if (IS_NULL(_mediator_queue_handler))
    {
        return;
    }
    MediatorTransaction transaction;
    transaction.client_id = client_id;
    transaction.broadcast = true;
    xQueueSend(_mediator_queue_handler, &transaction, portMAX_DELAY);
}

void X10_EffectMachine::process(int client_id, uint32_t hash, const char *event, cJSON *json)
{
    if (IS_NULL(_mediator_queue_handler))
    {
        return;
    }
    const char *key = nullptr;
    cJSON *value_item = cJSON_GetObjectItem(json, "v");
    if (IS_NULL((key = cJSON_GetStringValue(cJSON_GetObjectItem(json, "k")))) || IS_NULL(value_item) || !cJSON_IsNumber(value_item))
    {
        return;
    }
    MediatorTransaction transaction;
    transaction.value = (int)cJSON_GetNumberValue(value_item);
    transaction.hash = CRC32(key);
    transaction.id = nullptr;
    xQueueSend(_mediator_queue_handler, &transaction, portMAX_DELAY);
}

void X10_EffectMachine::build_packet(void)
{
    content_written = 0;
    buffer[content_written++] = '{';

    memcpy(buffer + content_written, "\"e\":\"", 5);
    content_written += 5;
    memcpy(buffer + content_written, EFFECT_EVENT_ID, EFFECT_EVENT_ID_LEN);
    content_written += EFFECT_EVENT_ID_LEN;
    buffer[content_written++] = '\"';
    buffer[content_written++] = ',';
    memcpy(buffer + content_written, "\"l\":{", 5);
    content_written += 5;

    for (uint8_t i = 0; i < _effect_count; i++)
    {
        buffer[content_written++] = '\"';
        const char *key = _effects[i]->id();
        size_t key_len = _effects[i]->id_size();
        memcpy(buffer + content_written, key, key_len);
        content_written += key_len;
        buffer[content_written++] = '\"';
        buffer[content_written++] = ':';

        char numbuf[12];
        int len = int_to_str(_effect_states[i], numbuf);
        memcpy(buffer + content_written, numbuf, len);
        content_written += len;

        if (i < _effect_count - 1)
        {
            buffer[content_written++] = ',';
        }
    }
    buffer[content_written++] = '}';
    buffer[content_written++] = '}';
    buffer[content_written++] = '\r';
    buffer[content_written++] = '\n';
    buffer[content_written++] = '\r';
    buffer[content_written++] = '\n';
    if (content_written < content_size)
    {
        buffer[content_written] = '\0';
    }
}

bool X10_EffectMachine::_initialize_query(void)
{
    _hook_receive_queue_handler = xQueueCreate(32, sizeof(EffectUpdate));
    _mediator_queue_handler = xQueueCreate(32, sizeof(MediatorTransaction));
    return NOT_NULL(_hook_receive_queue_handler) && NOT_NULL(_mediator_queue_handler);
}

bool X10_EffectMachine::_start_tasks(void)
{
    bool result = true;
    result &= xTaskCreate(__x10__effect_machine_hook_receive_task__, "x10_eff_hook_rx", 4096, this, 4, &_hook_receive_task_handler) == pdPASS;
    result &= xTaskCreate(__x10__effect_machine_mediator_task__, "x10_eff_center", 4096, this, 4, &_mediator_task_handler) == pdPASS;
    return result;
}

void X10_EffectMachine::_release_resources(void)
{
    DELETE_TASK(_hook_receive_task_handler);
    DELETE_TASK(_mediator_task_handler);
    DELETE_QUEUE(_hook_receive_queue_handler);
    DELETE_QUEUE(_hook_update_queue_handler);
    DELETE_QUEUE(_mediator_queue_handler);
    DELETE_MUTEXT(_buffer_lock);
    DELETE(_effect_states);
    DELETE(_effect_hashes);
    DELETE(_effects);
    _free_buffer();
}

size_t X10_EffectMachine::_accept_effects(EffectImpl **effects, size_t count)
{
    size_t accepted = 0;
    for (size_t i = 0; i < count; i++)
    {
        EffectImpl *effect = effects[i];
        int8_t result = _validate_effect(effect);
        accepted += result == 1 ? 1 : 0;
        if (result != 1)
        {
            ROXEL_LOGW("[EFFECT MACHINE] '%s' object is not passed in position %d ", NOT_NULL(effect) ? effect->id() : "NULL", i);
        }
    }
    return accepted;
}

int8_t X10_EffectMachine::_validate_effect(EffectImpl *effect)
{
    if (IS_NULL(effect))
    {
        return -1;
    }
    if (NOT_NULL(_effects))
    {
        for (size_t i = 0; i < _effect_count; i++)
        {
            EffectImpl *instance = _effects[i];
            if (NOT_NULL(instance) && instance->hash() == effect->hash())
            {
                return 0;
            }
        }
    }
    return 1;
}

bool X10_EffectMachine::_allocate_effects(size_t size)
{
    _effect_count = size;
    _effect_hashes = CREATE_ARRAY(uint32_t, size);
    _effect_states = CREATE_ARRAY(int, size);
    _effects = CREATE_ARRAY(EffectImpl *, size);
    return NOT_NULL(_effects) && NOT_NULL(_effect_states) && NOT_NULL(_effect_hashes);
}

bool X10_EffectMachine::_allocate_single_payload_cache(EffectLoader *loader)
{
    if (NOT_NULL(single_buffer))
    {
        vPortFree(single_buffer);
        single_buffer = nullptr;
    }
    size_t _size = loader->count;
    size_t max_id_size = 0;
    for (size_t i = 0; i < _size; i++)
    {
        size_t id_size = loader->effects[i]->id_size();
        if (id_size > max_id_size)
        {
            max_id_size = id_size;
        }
    }
    size_t max_num_len = 11;
    size_t end_divider_len = 4;
    size_t event_offset = 7 + EFFECT_EVENT_ID_LEN;
    size_t other_content = 7;
    single_content_size = (5 + max_id_size + max_num_len) + end_divider_len + event_offset + other_content + 1;
    ALLOCATE_BUFFER_MEMORY(single_buffer, single_content_size);
    return NOT_NULL(single_buffer);
}

bool X10_EffectMachine::_allocate_payload_cache(EffectLoader *loader)
{
    if (NOT_NULL(buffer))
    {
        vPortFree(buffer);
        buffer = nullptr;
    }
    size_t _size = loader->count;
    size_t all_name_len = 0;
    for (size_t i = 0; i < _size; i++)
    {
        all_name_len += loader->effects[i]->id_size();
    }
    size_t max_num_len = 11;
    size_t end_divider_len = 4;
    content_size = (_size * (2 + 1 + max_num_len)) + all_name_len + (_size > 1 ? (_size - 1) : 0) + 2 + end_divider_len + EFFECT_EVENT_ID_LEN + 11 + 1;
    ALLOCATE_BUFFER_MEMORY(buffer, content_size);
    return NOT_NULL(buffer);
}

void X10_EffectMachine::_load_effects(EffectLoader *loader)
{
    size_t shift = 0;
    for (size_t i = 0; i < loader->count; i++)
    {
        EffectImpl *effect = loader->effects[i];
        if (_validate_effect(effect) == 1)
        {
            effect->_update_queue = _hook_receive_queue_handler;
            effect->_position = shift;
            _effect_hashes[shift] = effect->hash();
            _effect_states[shift] = effect->as_int();
            _effects[shift++] = effect;
        }
    }
}

void X10_EffectMachine::_clean_trash(void)
{
    if (IS_NULL(_effects) || IS_NULL(_effect_hashes) || IS_NULL(_effect_states))
    {
        return;
    }
    for (size_t i = 0; i < _effect_count; i++)
    {
        _effect_states[i] = 0;
        _effect_hashes[i] = 0;
        _effects[i] = nullptr;
    }
}

void X10_EffectMachine::_free_buffer(void)
{
    if (NOT_NULL(single_buffer))
    {
        heap_caps_free(single_buffer);
        single_buffer = nullptr;
    }
    if (NOT_NULL(buffer))
    {
        heap_caps_free(buffer);
        buffer = nullptr;
    }
}