idf_component_register(
    SRCS "main.cc" "wifi.cc"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_wifi esp_netif nvs_flash esp_event lwip esp_http_client esp-tls esp_adc
)