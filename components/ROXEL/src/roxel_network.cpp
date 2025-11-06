#include "roxel_network.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>

inline static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    Network *instance = nullptr;
    if (arg != NULL && arg != nullptr)
    {
        instance = static_cast<Network *>(arg);
    }
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (instance != nullptr)
            {
                instance->__push_connection_state__(0);
            }
            esp_wifi_connect();
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            if (instance != nullptr)
            {
                instance->__push_connection_state__(1);
            }
        }
    }
}

roxel_network::roxel_network(const char *ssid, const char *passkey, const X10NET_Config &config)
{
    _passkey = passkey;
    _ssid = ssid;
    _dns = new roxel_dns(config);
}

roxel_network::~roxel_network()
{
    __sys__stop_network__();
    if (_dns != nullptr)
    {
        delete _dns;
        _dns = nullptr;
    }
}

void roxel_network::initialize(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    __sys__init_event_handlers__();

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, _ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, _passkey, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    if (_dns != nullptr)
    {
        _dns->launch();
    }
    esp_wifi_start();
}

bool roxel_network::await(void)
{
    if (_updates_available)
    {
        _updates_available = 0;
        return 1;
    }
    return 0;
}

bool roxel_network::state(void)
{
    return _sta_state;
}

void roxel_network::__push_connection_state__(bool state)
{
    _updates_available = _sta_state != state;
    _sta_state = state;
}

void roxel_network::__sys__init_event_handlers__(void)
{
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, this);
}

void roxel_network::__sys__stop_network__(void)
{
    esp_wifi_disconnect();
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        ROXEL_LOGE("[NETWORK] esp_wifi_stop failed: %d", err);
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK)
    {
        ROXEL_LOGE("[NETWORK] esp_wifi_deinit failed: %d", err);
    }
}