#ifndef _ROXEL_NETWORK_H_
#define _ROXEL_NETWORK_H_

#include "roxel_dns.h"
#include "roxel_network.h"
#include "roxel_macro.h"

class roxel_network;

typedef roxel_network Network;

class roxel_network
{
public:
    roxel_network(const char *ssid, const char *passkey, const X10NET_Config &config = X10NET_Config());
    ~roxel_network();
    void initialize(void);
    bool await(void);
    bool state(void);

    void __push_connection_state__(bool state);
private:
    bool _updates_available = false;
    bool _sta_state = false;
    const char *_passkey;
    const char *_ssid;

    roxel_dns* _dns = nullptr;

    void __sys__init_event_handlers__(void);
    void __sys__stop_network__(void);
};

#endif