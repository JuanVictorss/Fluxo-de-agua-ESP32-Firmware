# 💧 Medidor de Fluxo de Água com ESP32 e MQTT

![ESP32](https://img.shields.io/badge/ESP32-Framework-00979D?logo=espressif&logoColor=white)
![C](https://img.shields.io/badge/Language-C-blue)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![MQTT](https://img.shields.io/badge/MQTT-Enabled-purple)
![License](https://img.shields.io/badge/License-MIT-yellow)

---

## 📜 Visão Geral

Este projeto utiliza um **ESP32** para medir a **vazão (L/min)** e o **volume total (L)** de água que passa por um **sensor YF-S201**.  
Os dados são enviados para um **broker MQTT** em **formato JSON**, permitindo **monitoramento remoto em tempo real**.

✅ **Objetivo**: Criar uma solução de **baixo custo** para monitorar o consumo de água.

---

## ✨ Funcionalidades

- 📈 **Medição em tempo real**: Vazão calculada a cada segundo.
- 🔢 **Volume acumulado**: Contabiliza o consumo total de água.(valor é zerado ao reiniciar ESP32)
- 🌐 **Conectividade Wi-Fi**: Usando `example_connect()` do ESP-IDF.
- 📡 **Publicação MQTT**: Envia dados em JSON para o tópico `ifpe/ads/esp32/fluxoagua`.
- 💡 **Feedback visual**: LED indica fluxo ativo.
- ⚙️ **Baseado em FreeRTOS**: Tarefa dedicada para cálculos e envio.

---

## 🛠️ Hardware Necessário

- Placa **ESP32** (qualquer versão).
- Sensor de fluxo de água **YF-S201**.
- Fios Jumper para conexão.

## 🛠️ Software Necessário

- Framework ESP-IDF (v5.0 ou superior) instalado e configurado.
- Um Cliente MQTT para visualizar os dados.

---

## 🔌 Esquema de Conexão

| Fio do Sensor(Cor) | Conectar em    | Pino ESP32 | Descrição                     |
| ------------------ | -------------- | ---------- | ----------------------------- |
| **🔴 Vermelho**    | Alimentação 5V | VIN        | Alimenta o sensor             |
| **⚫ Preto**       | Terra (Ground) | GND        | Referência de 0V              |
| **🟡 Amarelo**     | Sinal de Dados | D13        | Envia os pulsos para o GPIO13 |

---

## 📂 Estrutura do Projeto

```
fluxo-de-agua/
├── main/
│   ├── medidor_fluxo.c
│   └── CMakeLists.txt
├── components/
├── CMakeLists.txt
├── sdkconfig
└── .gitignore
```

---

## 🚀 Como Compilar e Gravar

### ✅ 1. Pré-requisitos

- **ESP-IDF instalado e configurado**  
  👉 [Guia oficial](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

## !!! PARA COMANDOS "idf.py" VOCÊ PRECISARÁ UTILIZAR O ESP-IDF CMD !!!
### ✅ 2. Clonar o repositório
Dica: O sistema de compilação do ESP-IDF tem problemas com caracteres especiais, espaços e acentos nos caminhos (nomes de pastas).
Então evite clonar em uma pasta ou subpastas com caracteres especiais no nome.

```bash
git clone https://github.com/JuanVictorss/Fluxo-de-agua-ESP32-Firmware.git
cd Fluxo-de-agua-ESP32-Firmware
```

### ✅ 3. Configurar Wi-Fi

Abra:

```bash
idf.py menuconfig
```

Vá em: **Example Connection Configuration**  
Insira WiFi SSID(nome) e WiFi Password(senha) do Wi-Fi.  
Salve(S) e saia(Q).

Broker MQTT padrão:

```
mqtt://broker.hivemq.com:1883
```

Para usar outro broker, altere no código medidor_fluxo.c a linha 127:

```c
    esp_mqtt_client_config_t config_mqtt = {
        .broker.address.uri = "mqtt://broker.hivemq.com:1883",
    };
```

### ✅ 4. Compilar

```bash
idf.py build
```

### ✅ 5. Conectando o esp32

```bash
Para conectar no modo flash segure o botão boot da placa enquanto conecta o cabo USB nela.
```

### ✅ 6. Verificando a porta que conectou

```bash
python -m serial.tools.list_ports
```

Vai aparecer a PORTA_SERIAL que está sendo utilizada.

O nome da porta (ex: COM3) pode mudar se você conectar a placa em uma entrada USB diferente no seu computador.

### ✅ 7. Gravar no ESP32
COM4 é a PORTA_SERIAL, pode mudar de acordo com a entrada USB que você utilizar.
```bash
idf.py -p COM4 flash
```
Ao finalizar você verá algo como:
```bash
Leaving...
Hard resetting via RTS pin...
Done
```

---

## 📊 Monitoramento

### 🔍 Monitor Serial
COM4 é a PORTA_SERIAL, pode mudar de acordo com a entrada USB que você utilizar.
```bash
idf.py -p COM4 monitor
```
Aqui você vai conseguir monitorar os logs pelo terminal dessa forma abaixo:
```bash
I (85716) FLUXO_AGUA_MQTT: Vazão: 0.00 L/min (Pulsos: 0) | Total Acumulado: 0.00 L
I (86716) FLUXO_AGUA_MQTT: Vazão: 0.00 L/min (Pulsos: 0) | Total Acumulado: 0.00 L
I (87716) FLUXO_AGUA_MQTT: Vazão: 0.00 L/min (Pulsos: 0) | Total Acumulado: 0.00 L
I (88716) FLUXO_AGUA_MQTT: Fluxo iniciado!
I (88716) FLUXO_AGUA_MQTT: Vazão: 5.33 L/min (Pulsos: 40) | Total Acumulado: 0.09 L
I (89716) FLUXO_AGUA_MQTT: Vazão: 4.53 L/min (Pulsos: 34) | Total Acumulado: 0.16 L
I (90716) FLUXO_AGUA_MQTT: Vazão: 1.47 L/min (Pulsos: 11) | Total Acumulado: 0.19 L
I (91716) FLUXO_AGUA_MQTT: Fluxo parado!
I (91716) FLUXO_AGUA_MQTT: Vazão: 0.00 L/min (Pulsos: 0) | Total Acumulado: 0.19 L
```

Exemplo de saída:

```
I (FLUXO_AGUA_MQTT): Conectado ao broker MQTT
I (FLUXO_AGUA_MQTT): Vazão: 4.53 L/min (Pulsos: 34) | Total: 0.08 L
I (FLUXO_AGUA_MQTT): Fluxo iniciado!
I (FLUXO_AGUA_MQTT): Fluxo parado!
```

### 📡 Monitor MQTT

- **Broker**: `broker.hivemq.com`
- **Porta**: `1883`
- **Tópico**: `ifpe/ads/esp32/fluxoagua`

Formato JSON:

```json
{
  "flow_rate_lpm": 4.53,
  "total_liters": 0.08
}
```

---

## ⚙️ Detalhes do Código

**Arquivo principal**: `main/medidor_fluxo.c`

✔ **Interrupção (ISR)**:  
`isr_sensor_fluxo()` → Incrementa `contador_pulsos` a cada pulso.

✔ **Tarefa principal** (`tarefa_fluxo_agua`):

- Executa a cada 1 segundo.
- Calcula vazão:
  ```c
  taxa_fluxo = pulsos / FATOR_CALIBRACAO;
  ```
- Atualiza volume:
  ```c
  litros_totais += taxa_fluxo / 60;
  ```
- Controla LED de status.
- Publica via MQTT em JSON.

✔ **Fator de calibração**:  
`FATOR_CALIBRACAO = 7.5` (padrão YF-S201).

✔ **app_main()**:

- Inicializa Wi-Fi.
- Configura GPIO.
- Cria ISR para o sensor.
- Inicia MQTT e a tarefa FreeRTOS.

---


## 🔗 Repositórios Relacionados

- [📂 Frontend](https://github.com/JuanVictorss/Fluxo-de-agua-Frontend)
- [📂 Backend](https://github.com/JuanVictorss/Fluxo-de-agua-Backend)

---
## 📜 Licença

Este projeto está sob licença **MIT**.  
Você é livre para usar, modificar e distribuir, desde que mantenha os créditos.

---

## 🖼️ Demonstração

---

## 👨‍💻 Autor

**Juan Victor Souza Silva**.
Projeto para fins acadêmicos na disciplina de **Sistemas Embarcados**.
