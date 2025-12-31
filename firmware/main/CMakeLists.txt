# Main component CMakeLists.txt
# This tells ESP-IDF which source files to compile

idf_component_register(
    SRCS
        "main.c"
        "wifi_csi.c"
        "led_status.c"
        "http_server.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_wifi
        esp_event
        nvs_flash
        esp_netif
        esp_http_server
        driver
    PRIV_REQUIRES
        esp_timer
        esp_driver_rmt
)
