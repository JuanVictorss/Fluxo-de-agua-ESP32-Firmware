#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_timer.h"

// pinos
#define PINO_SENSOR_FLUXO   GPIO_NUM_13
#define PINO_LED            GPIO_NUM_2
#define FATOR_CALIBRACAO    7.5f

static const char *TAG = "FLUXO_AGUA_MQTT";

// variaveis
volatile uint32_t contador_pulsos = 0;
static float litros_totais = 0.0f;
static bool esta_fluindo = false;

esp_mqtt_client_handle_t cliente_mqtt = NULL;

// Rotina de Interrupção do Sensor (ISR)
static void IRAM_ATTR isr_sensor_fluxo(void *arg) {
    contador_pulsos++;
}

// eventos do MQTT
static void manipulador_eventos_mqtt(void *args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao broker MQTT");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado do broker MQTT");
            break;
        default:
            break;
    }
}

// publicar a mensagem MQTT
static void publicar_mensagem_mqtt(float taxa_fluxo) {
    if (cliente_mqtt) {
        char payload[100];
        snprintf(payload, sizeof(payload), "{\"flow_rate_lpm\": %.2f, \"total_liters\": %.2f}", taxa_fluxo, litros_totais);
        
        int msg_id = esp_mqtt_client_publish(cliente_mqtt, "ifpe/ads/esp32/fluxoagua", payload, 0, 1, 0);
        
        if (msg_id < 0) {
            ESP_LOGW(TAG, "Falha ao enviar mensagem MQTT");
        }
    }
}

static void tarefa_fluxo_agua(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        gpio_intr_disable(PINO_SENSOR_FLUXO);
        uint32_t pulsos_no_intervalo = contador_pulsos;
        contador_pulsos = 0;
        gpio_intr_enable(PINO_SENSOR_FLUXO);

        float taxa_fluxo = (float)pulsos_no_intervalo / FATOR_CALIBRACAO;

        bool ultimo_estado_fluindo = esta_fluindo;
        esta_fluindo = (taxa_fluxo > 0.0);

        if (esta_fluindo) {
            litros_totais += (taxa_fluxo / 60.0f);
            gpio_set_level(PINO_LED, 1);
        } else {
            gpio_set_level(PINO_LED, 0);
        }
        
        // fluxo começou
        if (esta_fluindo && !ultimo_estado_fluindo) {
            ESP_LOGI(TAG, "Fluxo iniciado!");
        }

        // para o fluxo
        if (!esta_fluindo && ultimo_estado_fluindo) {
            ESP_LOGI(TAG, "Fluxo parado!");
            publicar_mensagem_mqtt(0.0f); 
        }

        // fluxo ativo
        if (esta_fluindo) {
            publicar_mensagem_mqtt(taxa_fluxo);
        }

        ESP_LOGI(TAG, "Vazão: %.2f L/min (Pulsos: %lu) | Total Acumulado: %.2f L", taxa_fluxo, pulsos_no_intervalo, litros_totais);
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    gpio_config_t config_led = {
        .pin_bit_mask = (1ULL << PINO_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&config_led);
    gpio_set_level(PINO_LED, 0);

    gpio_config_t config_sensor = {
        .pin_bit_mask = (1ULL << PINO_SENSOR_FLUXO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&config_sensor);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PINO_SENSOR_FLUXO, isr_sensor_fluxo, NULL);

    esp_mqtt_client_config_t config_mqtt = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
    };
    cliente_mqtt = esp_mqtt_client_init(&config_mqtt);
    esp_mqtt_client_register_event(cliente_mqtt, ESP_EVENT_ANY_ID, manipulador_eventos_mqtt, NULL);
    esp_mqtt_client_start(cliente_mqtt);

    xTaskCreate(tarefa_fluxo_agua, "tarefa_fluxo_agua", 4096, NULL, 5, NULL);
}