#include "net/dns.h"

#include "esp_netif_ip_addr.h"
#include "esp_netif.h"

roxel_dns::roxel_dns(const X10NET_Config &config)
{
    _config = config;
}

void roxel_dns::launch(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_dhcpc_stop(netif);
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = esp_ip4addr_aton(_config.ip);
    ip_info.gw.addr = esp_ip4addr_aton(_config.gateway);
    ip_info.netmask.addr = esp_ip4addr_aton(_config.netmask);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
}