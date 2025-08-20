#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_timer.h"
#include "cJSON.h"

#define PINO_SENSOR_FLUXO GPIO_NUM_13
#define PINO_LED GPIO_NUM_2
#define DEVICE_ID "esp32-juanvictor"

static const char *TAG = "FLUXO_AGUA_MQTT";
const char* NVS_NAMESPACE = "config_fluxo";

char TOPICO_DADOS[100];
char TOPICO_STATUS[100];
char TOPICO_CONFIG_SET[100];
char TOPICO_CONFIG_GET[100];

volatile uint64_t contador_pulsos = 0;
static double litros_totais = 0.0;
esp_mqtt_client_handle_t cliente_mqtt = NULL;

static double fator_calibracao = 7.5;
static int32_t intervalo_telemetria_s = 1;

void sanitizar_config();

//salva as configurações na memória
void salvar_configuracoes_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_blob(nvs_handle, "fator_calib", &fator_calibracao, sizeof(fator_calibracao));
        nvs_set_i32(nvs_handle, "intervalo_s", intervalo_telemetria_s);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Novas configurações salvas na NVS!");
    } else {
        ESP_LOGE(TAG, "Erro ao abrir NVS para salvar configurações: %s", esp_err_to_name(err));
    }
}

//carrega as configurações da NVS ao iniciar
void carregar_configuracoes_nvs() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t size_fator = sizeof(fator_calibracao);
        if (nvs_get_blob(nvs_handle, "fator_calib", &fator_calibracao, &size_fator) != ESP_OK) {
            ESP_LOGW(TAG, "Fator de calibração não encontrado na NVS, usando padrão: %.2f", fator_calibracao);
        }
        if (nvs_get_i32(nvs_handle, "intervalo_s", &intervalo_telemetria_s) != ESP_OK) {
            ESP_LOGW(TAG, "Intervalo de telemetria não encontrado, usando padrão: %d", (int)intervalo_telemetria_s);
        }
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Configurações carregadas: Fator=%.2f, Intervalo=%ds", fator_calibracao, (int)intervalo_telemetria_s);
    } else {
        ESP_LOGW(TAG, "Nenhuma configuração encontrada na NVS, usando valores padrão.");
    }
    sanitizar_config();
}

// segurança de calibração
void sanitizar_config() {
    if (fator_calibracao <= 0.0) {
        fator_calibracao = 7.5;
    }
    if (intervalo_telemetria_s <= 0) {
        ESP_LOGW(TAG, "Intervalo de telemetria inválido (%d). Resetando para o padrão 1s.", (int)intervalo_telemetria_s);
        intervalo_telemetria_s = 1;
    }
}


// Rotina de Serviço de Interrupção para o sensor de fluxo
static void IRAM_ATTR isr_sensor_fluxo(void *arg) {
    contador_pulsos++;
}

// manipula eventos do cliente MQTT
static void manipulador_eventos_mqtt(void *args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado ao broker MQTT");
            esp_mqtt_client_publish(cliente_mqtt, TOPICO_STATUS, "{\"status\":\"online\"}", 0, 1, 1); // Retain status
            esp_mqtt_client_subscribe(cliente_mqtt, TOPICO_CONFIG_SET, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Desconectado do broker MQTT");
            break;

        case MQTT_EVENT_DATA:
            if (event->topic_len == strlen(TOPICO_CONFIG_SET) &&
                strncmp(event->topic, TOPICO_CONFIG_SET, event->topic_len) == 0) {
                
                ESP_LOGI(TAG, "Mensagem de configuração recebida no tópico: %.*s", event->topic_len, event->topic);
                
                cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
                if (root) {
                    if (cJSON_HasObjectItem(root, "fator_calibracao")) {
                        fator_calibracao = cJSON_GetObjectItem(root, "fator_calibracao")->valuedouble;
                    }
                    if (cJSON_HasObjectItem(root, "intervalo_telemetria_s")) {
                        intervalo_telemetria_s = cJSON_GetObjectItem(root, "intervalo_telemetria_s")->valueint;
                    }
                    
                    sanitizar_config(); 
                    salvar_configuracoes_nvs();
                    
                    ESP_LOGI(TAG, "Configurações atualizadas: Fator=%.2f, Intervalo=%ds", fator_calibracao, (int)intervalo_telemetria_s);
                    
                    cJSON_Delete(root);
                } else {
                    ESP_LOGE(TAG, "Erro ao parsear JSON de configuração");
                }
            }
            break;
        
        default:
            break;
    }
}

// status "online"
static void tarefa_status(void *pvParameters) {
    const int intervalo_status_segundos = 30; // Aumentado para não poluir o broker
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(intervalo_status_segundos * 1000));
        if (cliente_mqtt) {
            esp_mqtt_client_publish(cliente_mqtt, TOPICO_STATUS, "{\"status\":\"online\"}", 0, 1, 1);
        }
    }
}

// lê o sensor e envia os dados
static void tarefa_fluxo_agua(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(intervalo_telemetria_s * 1000));
        gpio_intr_disable(PINO_SENSOR_FLUXO);
        uint64_t pulsos_no_intervalo = contador_pulsos;
        contador_pulsos = 0;
        gpio_intr_enable(PINO_SENSOR_FLUXO);

        // vazão l/minutos
        double taxa_fluxo_lpm = (double)pulsos_no_intervalo / (fator_calibracao * (double)intervalo_telemetria_s);
        
        // vazão l/segundos
        double taxa_fluxo_lps = taxa_fluxo_lpm / 60.0;

        // volume em litros no intervalo
        double volume_no_intervalo = taxa_fluxo_lps * (double)intervalo_telemetria_s;
        
        litros_totais += volume_no_intervalo;
        
        // acende led quando tem fluxo
        if (taxa_fluxo_lpm > 0.0) {
            gpio_set_level(PINO_LED, 1);
        } else {
            gpio_set_level(PINO_LED, 0);
        }

        // online se conectar
        if (cliente_mqtt) {
            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"flow_rate_lpm\":%.6f,\"total_liters\":%.6f}",
                     taxa_fluxo_lpm, litros_totais);
            
            esp_mqtt_client_publish(cliente_mqtt, TOPICO_DADOS, payload, 0, 1, 0);
        }
    }
}

void app_main(void) {
    // tópicos MQTT
    sprintf(TOPICO_DADOS, "ifpe/ads/dispositivo/%s/dados", DEVICE_ID);
    sprintf(TOPICO_STATUS, "ifpe/ads/dispositivo/%s/status", DEVICE_ID);
    sprintf(TOPICO_CONFIG_SET, "ifpe/ads/dispositivo/%s/config/set", DEVICE_ID);
    sprintf(TOPICO_CONFIG_GET, "ifpe/ads/dispositivo/%s/config/get", DEVICE_ID);

    // NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    carregar_configuracoes_nvs();

    // wifi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // GPIOLED
    gpio_config_t config_led = {
        .pin_bit_mask = (1ULL << PINO_LED),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&config_led);

    // GPIO sensor de Fluxo
    gpio_config_t config_sensor = {
        .pin_bit_mask = (1ULL << PINO_SENSOR_FLUXO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&config_sensor);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PINO_SENSOR_FLUXO, isr_sensor_fluxo, NULL);

    //cliente MQTT
    esp_mqtt_client_config_t config_mqtt = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
        .session.last_will = {
            .topic = TOPICO_STATUS,
            .msg = "{\"status\":\"offline\"}",
            .qos = 1,
            .retain = 1
        }
    };

    cliente_mqtt = esp_mqtt_client_init(&config_mqtt);
    esp_mqtt_client_register_event(cliente_mqtt, ESP_EVENT_ANY_ID, manipulador_eventos_mqtt, NULL);
    esp_mqtt_client_start(cliente_mqtt);

    xTaskCreate(tarefa_fluxo_agua, "tarefa_fluxo_agua", 4096, NULL, 5, NULL);
    xTaskCreate(tarefa_status, "tarefa_status", 2048, NULL, 3, NULL);
}