#include "net/network.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>

static bool wifi_connected = false;
static bool ip_received = false;

inline static void network_task(void *arg)
{
    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!wifi_connected)
        {
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_wifi_connect();
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!ip_received)
        {
            esp_wifi_disconnect();
        }
    }
}

inline static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data)
{
    Network *instance = static_cast<Network *>(arg);
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
            return;
        }
        if (event_id == WIFI_EVENT_STA_CONNECTED)
        {
            wifi_connected = true;
            ip_received = false;
            if (instance->_task_is_running)
                xTaskNotifyGive(instance->_task_handler);
            return;
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            wifi_connected = false;
            instance->__push_connection_state__(0);
            if (instance->_task_is_running)
                xTaskNotifyGive(instance->_task_handler);
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_received = true;
            instance->__push_connection_state__(1);
        }
    }
}

roxel_network::roxel_network(const char *ssid, const char *passkey)
{
    _passkey = passkey;
    _ssid = ssid;
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

void roxel_network::useStaticConfiguration(const X10NET_Config &config)
{
    _dns = new roxel_dns(config);
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
    __start_task__();
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

void roxel_network::release(void)
{
    if (_task_is_running)
    {
        DELETE_TASK(_task_handler);
        _task_is_running = false;
    }
}

void roxel_network::__push_connection_state__(bool state)
{
    _updates_available = 1;
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

void roxel_network::__start_task__(void)
{
    _task_is_running = xTaskCreate(network_task, "roxel_network", 2048, nullptr, 3, &_task_handler);
}