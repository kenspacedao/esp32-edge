# AGENTS.md

## 1. Mission & Context
Build an ESP32-based edge-to-cloud environment that samples ambient temperature, humidity, and particulate matter. The local values must display in real time on a physical 128x64 OLED screen before being packaged as JSON and dispatched over TLS/MQTT to AWS IoT Core. From there, serverless workflows handle data ingestion (AWS Lambda) and persistent storage (DynamoDB), with execution monitoring inside CloudWatch.

---

## 2. Hardware BOM (Bill of Materials)
Ensure all physical components below are sourced and wired before running local device testing:

*   **Microcontroller:** ESP32 Development Board (e.g., ESP32-WROOM-32 or NodeMCU ESP32)
*   **Particulate Sensor:** PMS5003 Laser Dust Sensor (UART Serial)
*   **Climate Sensor:** DHT11 Temperature & Humidity Sensor
*   **OLED Display:** GME12864 / SSD1306 0.96" OLED (I2C, 128x64 pixels)
*   **Passives:** 
    *   1x 10k Ohm resistor (Pull-up for DHT11 Data line, if not using a pre-assembled module)
    *   1x 10µF - 100µF electrolytic capacitor (Decoupling capacitor across the main 5V/GND breadboard rails)
*   **Prototyping:** Half/full-size solderless breadboard, Male-to-Male & Male-to-Female jumper wires, and a reliable micro-USB/USB-C data cable

---

## 3. Toolchain & Environment Registry
*   **Firmware Env:** VS Code with PlatformIO (C++ framework)
*   **Cloud Env:** Python 3.12 (AWS Lambda local staging)
*   **Target Core Packages:**
    *   Firmware: `WiFiClientSecure.h`, `PubSubClient`, `ArduinoJson`, `Adafruit_SSD1306`, `Adafruit_GFX`, `DHT sensor library`
    *   Backend: Python `boto3`, `pytest` (local validation)

---

## 4. Judgment Boundaries

### 🚫 NEVER
*   Never hardcode active AWS credentials, Wi-Fi credentials, or private device certificates in plain source files. 
*   Never use blocking code blocks (`delay()`) inside the ESP32 main loop; utilize non-blocking timing structures (`millis()`) to avoid dropping incoming UART data from the PMS5003.
*   Never leave the CloudWatch Log Group retention set to "Never Expire".

### ⚠️ ASK
*   Ask before generating AWS infrastructure via AWS CLI or Terraform if a local manual configuration sequence is preferred.
*   Ask to verify the hardware pinout if modifying default I2C (SDA=21, SCL=22) or UART2 (RX2=16, TX2=17) interfaces.

### ✅ ALWAYS
*   Always structure the local sensor reads asynchronously, ensuring the GME12864 display refreshes independently of cloud network latency.
*   Always use a strict, structured JSON payload format when packaging metrics to dispatch over MQTT.
*   Always format Python AWS Lambda functions with explicit try-except error handling and structured CloudWatch log messages.

---

## 5. Directory Structure
```text
├── .agents/                 # Antigravity operational configs
├── firmware/                # PlatformIO C++ Project Workspace
│   ├── include/
│   │   └── secrets.h        # Declared credentials template (Git ignored)
│   ├── src/
│   │   └── main.cpp         # Master firmware source
│   └── platformio.ini       # Hardware/library manifest
├── aws/                     # AWS Serverless & Cloud Infrastructure
│   ├── lambda/
│   │   └── lambda_function.py  # Python ingestion handler
│   └── policy_templates/    # IoT Core & IAM policies
└── README.md                # Human documentation