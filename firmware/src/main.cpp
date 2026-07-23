#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PM25AQI.h>
#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Define MQTT Publish topic
#define AWS_IOT_PUBLISH_TOPIC "esp32/telemetry"

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Asynchronous timing for cloud publishing
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 10000; // Publish telemetry every 10 seconds

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected to I2C (SDA=21, SCL=22)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// DHT11 Sensor Settings
#define DHTPIN 15     // OUT/DAT pin connected to GPIO 15
#define DHTTYPE DHT11 // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// PMS5003 PM2.5 Sensor Settings
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

// Asynchronous timing variables
unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 2000; // Read every 2000 ms (2 seconds)
unsigned long lastPMRead = 0;
const unsigned long pmTimeoutInterval = 10000; // 10 seconds timeout

// Store latest readings
float temperature = 0.0;
float humidity = 0.0;
int pm25 = 0;
int pm10 = 0;
bool pmValid = false;

void updateDisplay(float temp, float hum, int pm25_val, int pm10_val) {
  display.clearDisplay();

  // Draw border and header box
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  display.fillRect(0, 0, SCREEN_WIDTH, 14, SSD1306_WHITE);

  // Header text (inverted colors)
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(30, 3);
  display.print(F("ENV MONITOR"));

  display.setTextColor(SSD1306_WHITE);

  // Draw grid lines
  display.drawFastVLine(64, 14, 50, SSD1306_WHITE); // Vertical separator
  display.drawFastHLine(0, 39, 128, SSD1306_WHITE); // Horizontal separator

  // 1. Temperature (Top-Left)
  display.setCursor(4, 18);
  display.print(F("Temp:"));
  display.setCursor(4, 28);
  if (isnan(temp)) {
    display.print(F("--"));
  } else {
    display.print(temp, 1);
    display.print(F(" F"));
  }

  // 2. Humidity (Top-Right)
  display.setCursor(68, 18);
  display.print(F("Humid:"));
  display.setCursor(68, 28);
  if (isnan(hum)) {
    display.print(F("--"));
  } else {
    display.print(hum, 1);
    display.print(F(" %"));
  }

  // 3. PM2.5 (Bottom-Left)
  display.setCursor(4, 43);
  display.print(F("PM2.5:"));
  display.setCursor(4, 53);
  if (pm25_val < 0) {
    display.print(F("--"));
  } else {
    display.print(pm25_val);
    display.print(F(" ug"));
  }

  // 4. Air Quality (Bottom-Right)
  display.setCursor(68, 43);
  display.print(F("Air Q:"));
  display.setCursor(68, 53);
  if (pm25_val < 0 || pm10_val < 0) {
    display.print(F("--"));
  } else if (pm25_val <= 12 && pm10_val <= 54) {
    display.print(F("Good"));
  } else if (pm25_val <= 35 && pm10_val <= 154) {
    display.print(F("Moderate"));
  } else {
    display.print(F("Bad"));
  }

  display.display();
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println(F("Connecting to Wi-Fi..."));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(F("\nWi-Fi Connected!"));

  // Configure WiFiClientSecure to use the AWS IoT certificates
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Initialize the MQTT client
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  Serial.println(F("Connecting to AWS IoT Core..."));
  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println(F("\nAWS IoT Connection Failed!"));
    return;
  }

  Serial.println(F("\nAWS IoT Connected!"));
}

void reconnectAWS() {
  // Non-blocking reconnect: attempt once per loop call if disconnected
  if (!client.connected()) {
    Serial.println(F("MQTT disconnected. Reconnecting..."));
    if (client.connect(THINGNAME)) {
      Serial.println(F("MQTT reconnected."));
    }
  }
}

void publishTelemetry() {
  JsonDocument doc;

  // Populate data
  doc["device_id"] = THINGNAME;
  doc["timestamp"] = millis(); // Local uptime relative offset
  
  if (isnan(temperature)) {
    doc["temperature"] = nullptr; // JSON null if sensor fails
  } else {
    doc["temperature"] = temperature;
  }
  
  if (isnan(humidity)) {
    doc["humidity"] = nullptr;
  } else {
    doc["humidity"] = humidity;
  }

  doc["pm25"] = pmValid ? pm25 : -1;
  doc["pm10"] = pmValid ? pm10 : -1;
  doc["status"] = (isnan(temperature) || !pmValid) ? "warning" : "ok";

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer);

  Serial.print(F("Publishing: "));
  Serial.println(jsonBuffer);
  
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup() {
  Serial.begin(115200);
  delay(500); // Stabilize serial connection
  Serial.println(F("--- Environmental Monitor Initializing ---"));

  // Initialize I2C with default SDA=21, SCL=22 pins
  Wire.begin(21, 22);

  // Initialize DHT11 sensor
  dht.begin();

  // Initialize Serial2 for PMS5003 (Baud: 9600, RX: 16, TX: 17)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  // Initialize PMS5003 sensor using UART
  if (!aqi.begin_UART(&Serial2)) {
    Serial.println(F("PMS5003 sensor not found!"));
  } else {
    Serial.println(F("PMS5003 sensor initialized successfully."));
  }

  // Initialize OLED display (checks 0x3C first, then falls back to 0x3D)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED not found at 0x3C, trying 0x3D..."));
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("OLED initialization failed! SSD1306 allocation failed."));
      for (;;)
        ; // Don't proceed, loop forever
    }
  }

  display.clearDisplay();
  display.display();

  connectAWS(); // Connect to Wi-Fi and AWS IoT Core

  Serial.println(F("Initialization complete."));
}

void loop() {
  unsigned long currentMillis = millis();

  // Maintain the MQTT connection and handle reconnects
  reconnectAWS();
  client.loop();

  // Asynchronously read the PMS5003 PM2.5 sensor
  PM25_AQI_Data data;
  if (aqi.read(&data)) {
    pm25 = data.pm25_standard;
    pm10 = data.pm100_standard;
    lastPMRead = currentMillis;
    pmValid = true;

    // Log to Serial Monitor for debugging
    Serial.print(F("PM 2.5: "));
    Serial.print(pm25);
    Serial.print(F(" ug/m3  PM 10: "));
    Serial.print(pm10);
    Serial.println(F(" ug/m3"));
  }

  // Check if we haven't received any PM data for more than the timeout interval (10 seconds)
  if (pmValid && (currentMillis - lastPMRead >= pmTimeoutInterval)) {
    pmValid = false;
    Serial.println(F("PMS5003 reading timeout - sensor disconnected?"));
  }

  // Non-blocking timer to read the DHT11 sensor every 2 seconds
  if (currentMillis - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = currentMillis;

    // Reading temperature or humidity takes about 250 milliseconds!
    float t = dht.readTemperature(true); // Read in Fahrenheit
    float h = dht.readHumidity();

    // Check if any reads failed and exit early (to try again).
    if (isnan(t) || isnan(h)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      updateDisplay(NAN, NAN, pmValid ? pm25 : -1, pmValid ? pm10 : -1);
    } else {
      temperature = t;
      humidity = h;

      // Print to Serial Monitor for debugging
      Serial.print(F("Humidity: "));
      Serial.print(humidity);
      Serial.print(F("%  Temperature: "));
      Serial.print(temperature);
      Serial.println(F("°F"));

      // Refresh the OLED screen with the latest values
      updateDisplay(temperature, humidity, pmValid ? pm25 : -1, pmValid ? pm10 : -1);
    }
  }

  // Non-blocking cloud publish timer (every 10 seconds)
  if (client.connected() && (currentMillis - lastPublishTime >= publishInterval)) {
    lastPublishTime = currentMillis;
    publishTelemetry();
  }
}
