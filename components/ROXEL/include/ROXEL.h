#ifndef _ROXEL_H_
#define _ROXEL_H_

#include "roxel_network.h"
#include "roxel_macro.h"
#include "roxel_udp.h"
#include "roxel_tcp.h"
#include "router.h"
#include "effect_machine.h"

class ROXEL;

#define LOAD_REQUESTS(sdk, ...)                                      \
    do                                                               \
    {                                                                \
        if (NOT_NULL(as_ptr(sdk)))                                   \
        {                                                            \
            as_ptr(sdk)->__upload_requests__(REQUESTS(__VA_ARGS__)); \
        }                                                            \
    } while (0);

#define LOAD_EFFECTS(sdk, ...)                                     \
    do                                                             \
    {                                                              \
        if (NOT_NULL(as_ptr(sdk)))                                 \
        {                                                          \
            as_ptr(sdk)->__upload_effects__(EFFECTS(__VA_ARGS__)); \
        }                                                          \
    } while (0);

#define INITIALIZE_OTA(sdk, port)                \
    do                                           \
    {                                            \
        if (NOT_NULL(as_ptr(sdk)))               \
        {                                        \
            as_ptr(sdk)->createOverTheAir(port); \
        }                                        \
    } while (0);

#define SETUP FUNCTION setup(ROXEL &self)
#define LOOP FUNCTION loop(ROXEL &self, bool isConnected)

typedef ROXEL &Instance;

class ROXEL
{
public:
    ROXEL(Network &network, const char *token);
    ~ROXEL();
    bool initialize(std::function<void(Instance)> onInitialize = NULL);
    void launch(std::function<void(Instance, bool)> onLoopback = NULL);
    void release(void);
    void createOverTheAir(const uint16_t port);

    void __upload_requests__(RequestLoader *loader);
    void __upload_effects__(EffectLoader *loader);

private:
    Network *_network = nullptr;
    x10_cli_manager *_cli_manager = nullptr;
    roxel_udp *_udp_server = nullptr;
    roxel_tcp *_tcp_server = nullptr;
    X10_router *_router = nullptr;
    OTA *_ota_instance = nullptr;

    bool _do_verify_modules(bool with_logs);
    void _uninitialize_shutdown(void);
    void _initialize_shutdown(void);
    void _initialize_network(void);
    void _initialize_nvs(void);

    void _start_services(void);
    void _stop_services(void);

    bool _handle_over_the_air(void);
    void _release_resources(void);
};

#endif