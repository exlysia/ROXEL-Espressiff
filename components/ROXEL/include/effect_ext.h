#ifndef _ROXEL_EFFECT_EXT_H_
#define _ROXEL_EFFECT_EXT_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdatomic.h>
#include <functional>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include "roxel_macro.h"

#define EFFECTS(...) make_effects(__VA_ARGS__)

class StatesEffect;
class Effect;

typedef StatesEffect &StatesHook;
typedef Effect &Hook;

typedef int State;
typedef uint32_t States;

typedef struct
{
    int position;
    int value;
    bool notify;
} EffectUpdate;

class EffectImpl
{
public:
    EffectImpl(const char *id);
    const char *id(void) const;
    uint32_t hash(void) const;
    size_t id_size(void) const;

    bool operator==(const EffectImpl &other) const;
    virtual int as_int(void) { return 0; }
    virtual void update(int newState) {};

    QueueHandle_t _update_queue = NULL;
    int _position = -1;

private:
    uint32_t _hash = 0;
    const char *_id;
    size_t _id_size;
};

class Effect : public EffectImpl
{
public:
    Effect(const char *id, std::function<void(Hook, State)> onUpdate = NULL, int initialState = 0);
    explicit operator int() const;
    Effect &operator=(const int &other);

    int as_int(void) override;
    void update(int newState) override;

private:
    std::function<void(Hook, State)> _on_update;
    std::atomic_int _state = 0;
};

class StatesEffect : public EffectImpl
{
public:
    StatesEffect(const char *id, std::function<void(StatesHook, States)> onUpdate = NULL, uint8_t size = 32);
    bool operator[](uint8_t index) const;
    explicit operator bool() const;
    bool any(void) const;
    void all(bool state);
    void set(uint8_t index, bool state);
    void clear(void);
    uint8_t size(void) const;

    int as_int(void) override;
    void update(int newState) override;

private:
    std::function<void(StatesHook, States)> _on_update;
    std::atomic_uint32_t _states = 0;
    std::atomic_uint32_t _mask;
    uint8_t _size;

    void _notify_changes(uint32_t new_value);
};

class EffectLoader
{
public:
    EffectImpl **effects = nullptr;
    int count = 0;

    EffectLoader() = default;

    EffectLoader(EffectImpl **list, int n)
        : effects(list), count(n) {}

    EffectLoader(const EffectLoader &) = delete;
    EffectLoader &operator=(const EffectLoader &) = delete;

    EffectLoader(EffectLoader &&other) noexcept
    {
        effects = other.effects;
        count = other.count;
        other.effects = nullptr;
        other.count = 0;
    }

    ~EffectLoader()
    {
        delete[] effects;
    }
};

template <typename T>
constexpr EffectImpl *as_effect_ptr(T *ptr) noexcept { return ptr; }

template <typename T>
constexpr EffectImpl *as_effect_ptr(T &ref) noexcept { return &ref; }

template <typename... Args>
inline EffectLoader *make_effects(Args &&...args)
{
    static EffectImpl *arr[sizeof...(Args)];
    size_t i = 0;
    ((arr[i++] = as_effect_ptr(std::forward<Args>(args))), ...);

    return CREATE(EffectLoader, arr, sizeof...(Args));
}

#endif