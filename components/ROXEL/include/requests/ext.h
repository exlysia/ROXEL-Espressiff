#ifndef _ROXEL_REQUEST_EXT_H_
#define _ROXEL_REQUEST_EXT_H_

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "../utils/roxel_macro.h"
#include <functional>
#include <iostream>
#include <stdint.h>
#include <string.h>
#include <cJSON.h>

#define REQUESTS(...) make_requests(__VA_ARGS__)

class RequestImpl;

typedef enum
{
    FULL,
    FAST,
    NONE
} RequestType;

typedef struct
{
    const char *id;
    cJSON *respond;
    int client_id;
} RequestRespond;

typedef RequestImpl &Incoming;
typedef int ClientID;

class Payload
{
public:
    Payload(cJSON *data);
    ~Payload();
    const char *text(const char *key, const char *defaultValue = nullptr);
    int number(const char *key, int defaultValue = -1);
    bool boolean(const char *key, bool defaultValue = false);
    Payload &object(const char *key);

private:
    size_t _child_count = 0;
    Payload *_childs[16];

    cJSON *_data;
};

class RequestImpl
{
public:
    RequestImpl(const char *id, RequestType type);
    const char *id(void) const;
    uint32_t hash(void) const;
    size_t id_size(void) const;
    bool operator==(const RequestImpl &other) const;
    RequestType type(void);

    virtual void update_call(int client_id, cJSON *data) {}
    virtual void update_connect(int client_id) {}
    virtual void respond(int client_id, cJSON *data) {}

    QueueHandle_t _respond_queue = NULL;
    int _position = -1;

private:
    RequestType _type = RequestType::NONE;
    uint32_t _hash = 0;
    const char *_id;
    size_t _id_size;
};

class Request : public RequestImpl
{
public:
    Request(const char *id, std::function<void(Incoming, ClientID, Payload)> onCall, std::function<void(Incoming, ClientID)> onConnect);
    Request(const char *id, std::function<void(Incoming, ClientID, Payload)> onCall);
    void update_call(int client_id, cJSON *data) override;
    void update_connect(int client_id) override;
    void respond(int client_id, cJSON *data) override;

private:
    std::function<void(Incoming, ClientID, Payload)> _on_call;
    std::function<void(Incoming, ClientID)> _on_connect;
};

class FastRequest : public RequestImpl
{
public:
    FastRequest(const char *id, std::function<void(Incoming, ClientID, Payload)> onCall);
    void update_call(int client_id, cJSON *data) override;

private:
    std::function<void(Incoming, ClientID, Payload)> _on_call;
};

class Answer
{
public:
    Answer(void);
    Answer &number(const char *key, int value);
    Answer &text(const char *key, const char *value);
    Answer &boolean(const char *key, bool value);
    cJSON *detach();

    void respond(int client_id, RequestImpl &incomming);
    void broadcast(RequestImpl &incomming);

private:
    cJSON *_payload = NULL;
};

class RequestLoader
{
public:
    RequestImpl **requests = nullptr;
    int count = 0;

    RequestLoader() = default;

    RequestLoader(RequestImpl **list, int n)
        : requests(list), count(n) {}

    RequestLoader(const RequestLoader &) = delete;
    RequestLoader &operator=(const RequestLoader &) = delete;

    RequestLoader(RequestLoader &&other) noexcept
    {
        requests = other.requests;
        count = other.count;
        other.requests = nullptr;
        other.count = 0;
    }

    ~RequestLoader()
    {
        delete[] requests;
    }
};

template <typename T>
constexpr RequestImpl *as_request_ptr(T *ptr) noexcept { return ptr; }

template <typename T>
constexpr RequestImpl *as_request_ptr(T &ref) noexcept { return &ref; }

template <typename... Args>
inline RequestLoader *make_requests(Args &&...args)
{
    static RequestImpl *arr[sizeof...(Args)];
    size_t i = 0;
    ((arr[i++] = as_request_ptr(std::forward<Args>(args))), ...);

    return CREATE(RequestLoader, arr, sizeof...(Args));
}

#endif