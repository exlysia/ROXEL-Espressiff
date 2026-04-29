#include "ROXEL.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_err.h"

inline static ROXEL *_roxel_sdk_instance = nullptr;

inline static void __roxel_shutdown_handler__(void)
{
    if (NOT_NULL(_roxel_sdk_instance))
    {
        _roxel_sdk_instance->release();
        _roxel_sdk_instance = nullptr;
    }
}

ROXEL::ROXEL(Network &network, const char *token)
{
    _cli_manager = CREATE(x10_cli_manager, token);
    _udp_server = CREATE(roxel_udp, 5001, _cli_manager);
    _tcp_server = CREATE(roxel_tcp, 5002, _cli_manager);
    _router = CREATE(X10_router, _cli_manager, 5003, &_tcp_server->_tcp_on_connect_queue_handler, &_tcp_server->_tcp_queue_handler);
    _network = &network;
}

ROXEL::~ROXEL()
{
    _release_resources();
}

bool ROXEL::initialize(std::function<void(Instance)> onInitialize)
{
    _initialize_nvs();
    _initialize_shutdown();
    if (!_do_verify_modules(1))
    {
        _release_resources();
        return 0;
    }
    _initialize_network();
    if (NOT_NULL(onInitialize))
    {
        onInitialize(*this);
    }
    return 1;
}

void ROXEL::launch(std::function<void(Instance, bool)> onLoopback)
{
    while (1)
    {
        if (!_do_verify_modules(0))
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (_handle_over_the_air())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // network state change detection
        if (_network->await())
        {
            if (_network->state())
            {
                _start_services();
            }
            else
            {
                _stop_services();
            }
        }

        // do loopback to main code
        if (NOT_NULL(onLoopback))
        {
            onLoopback(*this, _network->state());
        }

        // standart delay for application loop
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void ROXEL::release(void)
{
    ROXEL_LOGW("[MAIN] ============================================================");
    ROXEL_LOGW("[MAIN] WARNING: Stopping...");
    ROXEL_LOGW("[MAIN] ============================================================");
    _stop_services();
    ROXEL_LOGW("[MAIN] ============================================================");
    ROXEL_LOGW("[MAIN] WARNING: System has been stopped.");
    ROXEL_LOGW("[MAIN] ============================================================");
    _release_resources();
}

void ROXEL::createOverTheAir(const uint16_t port)
{
    if (IS_NULL(_ota_instance))
    {
        _ota_instance = CREATE(OTA, port);
        bool success = NOT_NULL(_ota_instance);
        ROXEL_LOG(success ? ESP_LOG_INFO : ESP_LOG_WARN, "[MAIN] Over-The-Air (OTA) Interface creation is %s", success ? "successful" : "failure");
        if (success && NOT_NULL(_router))
        {
            _router->attachOverTheAir(_ota_instance);
        }
    }
}

void ROXEL::__upload_requests__(RequestLoader *loader)
{
    if (IS_NULL(loader))
    {
        return;
    }
    X10_RequestMachine *full_machine = NOT_NULL(_router) ? _router->request_full_machine() : nullptr;
    if (NOT_NULL(full_machine))
    {
        full_machine->load(loader);
    }
    FastRequestMachine *fast_machine = NOT_NULL(_router) ? _router->request_fast_machine() : nullptr;
    if (NOT_NULL(fast_machine))
    {
        fast_machine->load(loader);
    }
}

void ROXEL::__upload_effects__(EffectLoader *loader)
{
    if (IS_NULL(loader))
    {
        return;
    }
    X10_EffectMachine *machine = NOT_NULL(_router) ? _router->effect_machine() : nullptr;
    if (NOT_NULL(machine))
    {
        machine->load(loader);
    }
}

bool ROXEL::_do_verify_modules(bool with_logs)
{
    bool client_manager = NOT_NULL(_cli_manager);
    bool network = NOT_NULL(_network);
    bool router = NOT_NULL(_router);
    bool tcp = NOT_NULL(_tcp_server);
    bool udp = NOT_NULL(_udp_server);
    if (with_logs)
    {
        ROXEL_LOG(network ? ESP_LOG_INFO : ESP_LOG_ERROR, "[MAIN] Verification: Network module = %s", network ? "ACCEPTED" : "FAILURE");
        ROXEL_LOG(client_manager ? ESP_LOG_INFO : ESP_LOG_ERROR, "[MAIN] Verification: Client-Manager module = %s", client_manager ? "ACCEPTED" : "FAILURE");
        ROXEL_LOG(router ? ESP_LOG_INFO : ESP_LOG_ERROR, "[MAIN] Verification: Router module = %s", router ? "ACCEPTED" : "FAILURE");
        ROXEL_LOG(tcp ? ESP_LOG_INFO : ESP_LOG_ERROR, "[MAIN] Verification: TCP-server module = %s", tcp ? "ACCEPTED" : "FAILURE");
        ROXEL_LOG(udp ? ESP_LOG_INFO : ESP_LOG_ERROR, "[MAIN] Verification: UDP-server module = %s", udp ? "ACCEPTED" : "FAILURE");
    }
    return network && client_manager && router && tcp && udp;
}

void ROXEL::_uninitialize_shutdown(void)
{
    esp_unregister_shutdown_handler(__roxel_shutdown_handler__);
}

void ROXEL::_initialize_shutdown(void)
{
    _roxel_sdk_instance = this;
    esp_register_shutdown_handler(__roxel_shutdown_handler__);
}

void ROXEL::_initialize_network(void)
{
    ROXEL_LOGI("[MAIN] Network initialization started");
    _network->initialize();
}

void ROXEL::_initialize_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ROXEL_LOGI("[MAIN] NVS Initialized");
}

void ROXEL::_start_services(void)
{
    if (NOT_NULL(_cli_manager))
    {
        _cli_manager->shedule_clients();
    }
    if (NOT_NULL(_udp_server))
    {
        _udp_server->launch();
    }
    if (NOT_NULL(_tcp_server))
    {
        _tcp_server->launch();
    }
    if (NOT_NULL(_router))
    {
        _router->launch();
    }
}

void ROXEL::_stop_services(void)
{
    if (NOT_NULL(_cli_manager))
    {
        _cli_manager->stop_sheduler();
    }
    if (NOT_NULL(_router))
    {
        _router->stop();
    }
    if (NOT_NULL(_udp_server))
    {
        _udp_server->stop();
    }
    if (NOT_NULL(_tcp_server))
    {
        _tcp_server->stop();
    }
}

void ROXEL::_release_resources(void)
{
    DELETE(_router);
    DELETE(_udp_server);
    DELETE(_tcp_server);
    DELETE(_cli_manager);
    _network->release();
}

bool ROXEL::_handle_over_the_air(void)
{
    if (NOT_NULL(_ota_instance) && _ota_instance->isRunning())
    {
        _uninitialize_shutdown();
        release();
        if (!_ota_instance->isExecuted())
        {
            _ota_instance->execute();
        }
        return 1;
    }
    return 0;
}