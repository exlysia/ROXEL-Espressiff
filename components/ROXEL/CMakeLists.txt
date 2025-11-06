idf_component_register(
    SRCS "src/ROXEL.cpp" "src/effect_ext.cpp" "src/request_ext.cpp" "src/roxel_udp.cpp" "src/roxel_tcp.cpp" "src/roxel_network.cpp" "src/roxel_dns.cpp" "src/roxel_encode.cpp" "src/cli_manager.cpp" "src/router.cpp" "src/effect_machine.cpp" "src/request_machine.cpp" "src/roxel_ota.cpp"
    INCLUDE_DIRS "include"
    REQUIRES lwip esp_wifi esp_event nvs_flash esp_netif json app_update mbedtls esp_timer
)