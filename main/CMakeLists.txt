idf_component_register(SRCS "medidor_fluxo.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES "nvs_flash;esp_netif;esp_event;log;freertos;driver;mqtt;protocol_examples_common;json"
                       )