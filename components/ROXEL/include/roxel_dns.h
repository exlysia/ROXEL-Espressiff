#ifndef _ROXEL_DNS_H_
#define _ROXEL_DNS_H_

struct X10NET_Config
{
    const char *ip = "192.168.88.110";
    const char *gateway = "192.168.88.1";
    const char *netmask = "255.255.255.0";
};

class roxel_dns
{
public:
    roxel_dns(const X10NET_Config &config = X10NET_Config());
    void launch(void);
private:
    X10NET_Config _config;
};

#endif