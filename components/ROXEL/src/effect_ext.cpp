#include "effect_ext.h"

/*
[EffectImpl-extension]
==============================================================================================================================
*/
EffectImpl::EffectImpl(const char *id)
{
    _id_size = strlen(id);
    _hash = CRC32(id);
    _id = id;
}

const char *EffectImpl::id(void) const
{
    return _id;
}

uint32_t EffectImpl::hash(void) const
{
    if (!_id)
    {
        return 0;
    }
    return _hash;
}

size_t EffectImpl::id_size(void) const
{
    return _id_size;
}

bool EffectImpl::operator==(const EffectImpl &other) const
{
    return _hash == other._hash;
}

/*
[Effect-extension]
==============================================================================================================================
*/
Effect::Effect(const char *id, std::function<void(Hook, State)> onUpdate, int initialState) : EffectImpl(id)
{
    _state = initialState;
    _on_update = onUpdate;
}

Effect::operator int() const
{
    return _state;
}

Effect &Effect::operator=(const int &other)
{
    int old = atomic_exchange(&_state, other);
    if (old != other && NOT_NULL(_update_queue))
    {
        EffectUpdate effect = {.position = _position, .value = other, .notify = true};
        xQueueSend(_update_queue, &effect, portMAX_DELAY);
    }
    return *this;
}

int Effect::as_int(void)
{
    return _state;
}

void Effect::update(int newState)
{
    int old = atomic_exchange(&_state, newState);
    if (old != newState && NOT_NULL(_on_update))
    {
        _on_update(*this, newState);
    }
}

/*
[StatesEffect-extension]
==============================================================================================================================
*/
StatesEffect::StatesEffect(const char *id, std::function<void(StatesHook, States)> onUpdate, uint8_t size) : EffectImpl(id)
{
    _on_update = onUpdate;
    if (size >= 32)
    {
        _mask = 0xFFFFFFFF;
        _size = 32;
        return;
    };
    _mask = (1UL << size) - 1;
    _size = size;
}

bool StatesEffect::operator[](uint8_t index) const
{
    if (index >= _size)
        return 0;
    return (_states.load() >> index) & 1U;
}

StatesEffect::operator bool() const
{
    uint32_t mask = _mask.load(std::memory_order_relaxed);
    return (_states.load(std::memory_order_relaxed) & mask) == mask;
}

bool StatesEffect::any(void) const
{
    return (_states.load(std::memory_order_relaxed) & _mask.load(std::memory_order_relaxed)) != 0;
}

void StatesEffect::all(bool state)
{
    uint32_t new_value = state ? _mask.load(std::memory_order_relaxed) : 0;
    uint32_t old = atomic_exchange(&_states, state ? _mask.load(std::memory_order_relaxed) : 0);
    if (old != new_value)
    {
        _notify_changes(new_value);
    }
}

void StatesEffect::set(uint8_t index, bool state)
{
    if (index >= _size)
        return;
    uint32_t old = 0;
    if (state)
        old = _states.fetch_or(1UL << index, std::memory_order_relaxed);
    else
        old = _states.fetch_and(~(1UL << index), std::memory_order_relaxed);

    uint32_t new_value = _states.load(std::memory_order_relaxed);
    if (old != new_value)
    {
        _notify_changes(new_value);
    }
}

void StatesEffect::clear(void)
{
    uint32_t old = atomic_exchange(&_states, 0);
    if (old != 0)
    {
        _notify_changes(0);
    }
}

uint8_t StatesEffect::size(void) const
{
    return _size;
}

int StatesEffect::as_int(void)
{
    return _states.load(std::memory_order_relaxed);
}

void StatesEffect::update(int newState)
{
    int old = atomic_exchange(&_states, (uint32_t)newState);
    if (old != newState && NOT_NULL(_on_update))
    {
        _on_update(*this, newState);
    }
}

void StatesEffect::_notify_changes(uint32_t new_value)
{
    if (NOT_NULL(_update_queue))
    {
        EffectUpdate effect = {.position = _position, .value = (int)new_value, .notify = true};
        xQueueSend(_update_queue, &effect, portMAX_DELAY);
    }
}