#ifndef _ROXEL_MACRO_H_
#define _ROXEL_MACRO_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "esp_rom_crc.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>

#define NOT_NULL(x) (x != nullptr && x != NULL)
#define IS_NULL(x) (x == nullptr || x == NULL)
#define NOT_EMPTY(x) (strlen(x) > 0)
#define EQUALS(a, b) (strcmp(a, b) == 0)
#define N_EQUALS(a, b, c) (strncmp(a, b, c) == 0)
#define CRC32(x) esp_rom_crc32_le(0, reinterpret_cast<const uint8_t *>(x), strlen(x))
#define DELETE(x)        \
    do                   \
    {                    \
        if (NOT_NULL(x)) \
            delete x;    \
        x = nullptr;     \
    } while (0)
#define DELETE_QUEUE(x)      \
    do                       \
    {                        \
        if (NOT_NULL(x))     \
            vQueueDelete(x); \
        x = NULL;            \
    } while (0)
#define DELETE_TASK(x)      \
    do                      \
    {                       \
        if (NOT_NULL(x))    \
            vTaskDelete(x); \
        x = NULL;           \
    } while (0)
#define DELETE_MUTEXT(x)         \
    do                           \
    {                            \
        if (NOT_NULL(x))         \
            vSemaphoreDelete(x); \
        x = NULL;                \
    } while (0)
#define CREATE(x, ...) new x(__VA_ARGS__)
#define CREATE_ARRAY(x, size) new x[size]
#define CREATE_ARRAY_WITH_ITEMS(x, size, ...) \
    new x[size] { __VA_ARGS__ }
#define FUNCTION inline static void
#define MAIN extern "C" void app_main(void)

#define ALLOCATE_BUFFER_MEMORY(ptr, size)                                            \
    ptr = (char *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);       \
    if (IS_NULL(ptr))                                                                \
    {                                                                                \
        ptr = (char *)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); \
    }

#define ROXEL_LOG_TAG "ROXEL.SYSTEM"
#define ROXEL_LOG(level, format, ...)                                                 \
    do                                                                                \
    {                                                                                 \
        ESP_LOG_LEVEL_LOCAL(level, ROXEL_LOG_TAG, format __VA_OPT__(, ) __VA_ARGS__); \
    } while (0)
#define ROXEL_LOGI(format, ...)                                                              \
    do                                                                                       \
    {                                                                                        \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, ROXEL_LOG_TAG, format __VA_OPT__(, ) __VA_ARGS__); \
    } while (0)
#define ROXEL_LOGW(format, ...)                                                              \
    do                                                                                       \
    {                                                                                        \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN, ROXEL_LOG_TAG, format __VA_OPT__(, ) __VA_ARGS__); \
    } while (0)
#define ROXEL_LOGE(format, ...)                                                               \
    do                                                                                        \
    {                                                                                         \
        ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, ROXEL_LOG_TAG, format __VA_OPT__(, ) __VA_ARGS__); \
    } while (0)

#define PP_ARG_N(                                     \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10,          \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
    _61, _62, _63, N, ...) N
#define PP_RSEQ_N()                             \
    63, 62, 61, 60,                             \
        59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
        49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
        39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
        29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
        19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
        9, 8, 7, 6, 5, 4, 3, 2, 1, 0
#define PP_NARG_(...) PP_ARG_N(__VA_ARGS__)
#define PP_NARG(...) PP_NARG_(__VA_ARGS__, PP_RSEQ_N())

#define IS(a, b) as_instance(a) == as_instance(b)
#define WHEN(a, b) if (as_instance(a) == as_instance(b))

static inline int int_to_str(uint32_t value, char *buf)
{
    char tmp[12];
    int i = 0;
    if (value == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (value > 0)
    {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }
    int pos = 0;
    for (int j = i - 1; j >= 0; --j)
        buf[pos++] = tmp[j];
    buf[pos] = '\0';
    return pos;
}

template <typename T>
constexpr T &as_instance(T &ref) noexcept
{
    return ref;
}

template <typename T>
constexpr T &as_instance(T *ptr) noexcept
{
    return *ptr;
}

template <typename T>
constexpr T *as_ptr(T &ref) noexcept { return &ref; }

template <typename T>
constexpr T *as_ptr(T *ptr) noexcept { return ptr; }

#endif
