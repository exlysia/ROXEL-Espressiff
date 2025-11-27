idf_component_register(
    SRCS "src/ROXEL.cpp" "src/effects/ext.cpp" "src/effects/machine.cpp" "src/requests/ext.cpp" "src/requests/full_machine.cpp" "src/requests/fast_machine.cpp" "src/net/network.cpp" "src/net/dns.cpp" "src/net/udp/udp.cpp" "src/net/tcp/tcp.cpp" "src/update/ota.cpp" "src/router/router.cpp" "src/utils/cli_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lwip esp_wifi esp_event nvs_flash esp_netif json app_update mbedtls esp_timer
)