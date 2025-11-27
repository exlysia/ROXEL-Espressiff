#include "request_ext.h"

Payload::Payload(cJSON *data)
{
    _data = data;
}

Payload::~Payload()
{
    for (size_t i = 0; i < _child_count; i++)
    {
        DELETE(_childs[i]);
    }
}

const char *Payload::text(const char *key, const char *defaultValue)
{
    if (NOT_NULL(_data))
    {
        const char *value = cJSON_GetStringValue(cJSON_GetObjectItem(_data, key));
        return IS_NULL(value) ? defaultValue : value;
    }
    return defaultValue;
}

int Payload::number(const char *key, int defaultValue)
{
    if (NOT_NULL(_data))
    {
        cJSON *item = cJSON_GetObjectItem(_data, key);
        if (cJSON_IsNumber(item))
        {
            return cJSON_GetNumberValue(item);
        }
    }
    return defaultValue;
}

bool Payload::boolean(const char *key, bool defaultValue)
{
    if (NOT_NULL(_data))
    {
        cJSON *item = cJSON_GetObjectItem(_data, key);
        if (cJSON_IsBool(item))
        {
            return cJSON_IsTrue(item);
        }
    }
    return defaultValue;
}

Payload &Payload::object(const char *key)
{
    if (_child_count < 16)
    {
        Payload *payload = CREATE(Payload, cJSON_GetObjectItem(_data, key));
        if (NOT_NULL(payload))
        {
            _childs[_child_count++] = payload;
            return *payload;
        }
    }
    return *this;
}

RequestImpl::RequestImpl(const char *id)
{
    _id_size = strlen(id);
    _hash = CRC32(id);
    _id = id;
}

const char *RequestImpl::id(void) const
{
    return _id;
}

uint32_t RequestImpl::hash(void) const
{
    if (!_id)
    {
        return 0;
    }
    return _hash;
}

size_t RequestImpl::id_size(void) const
{
    return _id_size;
}

bool RequestImpl::operator==(const RequestImpl &other) const
{
    return _hash == other._hash;
}

Request::Request(const char *id, std::function<void(Incoming, ClientID, Payload)> onCall, std::function<void(Incoming, ClientID)> onConnect) : RequestImpl(id)
{
    _on_connect = onConnect;
    _on_call = onCall;
}

Request::Request(const char *id, std::function<void(Incoming, ClientID, Payload)> onCall) : RequestImpl(id)
{
    _on_call = onCall;
}

void Request::respond(int client_id, cJSON *data)
{
    if (NOT_NULL(data) && NOT_NULL(_respond_queue))
    {
        RequestRespond answer;
        answer.client_id = client_id;
        answer.respond = data;
        answer.id = id();
        if (xQueueSend(_respond_queue, &answer, portMAX_DELAY) != pdPASS)
        {
            cJSON_Delete(data);
        }
    }
    else
    {
        if (NOT_NULL(data))
        {
            cJSON_Delete(data);
        }
    }
}

void Request::update_call(int client_id, cJSON *data)
{
    if (_on_call)
    {
        Payload *payload = CREATE(Payload, data);
        if (NOT_NULL(payload))
        {
            _on_call(*this, client_id, *payload);
            DELETE(payload);
        }
    }
}

void Request::update_connect(int client_id)
{
    if (_on_connect)
    {
        _on_connect(*this, client_id);
    }
}

FastRequest::FastRequest(const char *id, std::function<void(Incoming, ClientID, Payload)> onCall) : RequestImpl(id)
{
    _on_call = onCall;
}

void FastRequest::update_call(int client_id, cJSON *data)
{
    if (_on_call)
    {
        Payload *payload = CREATE(Payload, data);
        if (NOT_NULL(payload))
        {
            _on_call(*this, client_id, *payload);
            DELETE(payload);
        }
    }
}

Answer::Answer(void)
{
    if (NOT_NULL(_payload))
    {
        cJSON_Delete(_payload);
    }
    _payload = cJSON_CreateObject();
}

Answer &Answer::number(const char *key, int value)
{
    if (NOT_NULL(_payload))
    {
        cJSON_AddNumberToObject(_payload, key, value);
    }
    return *this;
}

Answer &Answer::text(const char *key, const char *value)
{
    if (NOT_NULL(_payload))
    {
        cJSON_AddStringToObject(_payload, key, value);
    }
    return *this;
}

Answer &Answer::boolean(const char *key, bool value)
{
    if (NOT_NULL(_payload))
    {
        cJSON_AddBoolToObject(_payload, key, value);
    }
    return *this;
}

cJSON *Answer::detach(void)
{
    cJSON *tmp = _payload;
    _payload = NULL;
    return tmp;
}

void Answer::respond(int client_id, RequestImpl &incomming)
{
    incomming.respond(client_id, detach());
}

void Answer::broadcast(RequestImpl &incomming)
{
    incomming.respond(-1, detach());
}