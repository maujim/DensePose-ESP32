# Main component CMakeLists.txt
# This tells ESP-IDF which source files to compile

idf_component_register(
    SRCS
        "main.c"
        "wifi_csi.c"
        "pose_inference.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_wifi
        esp_event
        nvs_flash
        esp_netif
    PRIV_REQUIRES
        esp_timer
)
