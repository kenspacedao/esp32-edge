#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected to I2C (SDA=21, SCL=22)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// DHT11 Sensor Settings
#define DHTPIN 15     // OUT/DAT pin connected to GPIO 15
#define DHTTYPE DHT11 // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// Asynchronous timing variables
unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 2000; // Read every 2000 ms (2 seconds)

// Store latest readings
float temperature = 0.0;
float humidity = 0.0;

void updateDisplay(float temp, float hum) {
  display.clearDisplay();

  // Draw header box
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  display.fillRect(0, 0, SCREEN_WIDTH, 14, SSD1306_WHITE);

  // Header text (inverted colors)
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(10, 3);
  display.print(F("ENVIRONMENT MONITOR"));

  display.setTextColor(SSD1306_WHITE);

  // Print Temperature
  display.setCursor(10, 24);
  display.print(F("Temp: "));
  if (isnan(temp)) {
    display.print(F("--"));
  } else {
    display.print(temp, 1); // 1 decimal place
    display.print(F(" C"));
  }

  // Print Humidity
  display.setCursor(10, 42);
  display.print(F("Humid: "));
  if (isnan(hum)) {
    display.print(F("--"));
  } else {
    display.print(hum, 1); // 1 decimal place
    display.print(F(" %"));
  }

  // Display update
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(500); // Stabilize serial connection
  Serial.println(F("--- Environmental Monitor Initializing ---"));

  // Initialize I2C with default SDA=21, SCL=22 pins
  Wire.begin(21, 22);

  // Initialize DHT11 sensor
  dht.begin();

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

  Serial.println(F("Initialization complete."));
}

void loop() {
  unsigned long currentMillis = millis();

  // Non-blocking timer to read the DHT11 sensor every 2 seconds
  if (currentMillis - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = currentMillis;

    // Reading temperature or humidity takes about 250 milliseconds!
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    // Check if any reads failed and exit early (to try again).
    if (isnan(t) || isnan(h)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      updateDisplay(NAN, NAN); // Show "--" on the display to indicate error
    } else {
      temperature = t;
      humidity = h;

      // Print to Serial Monitor for debugging
      Serial.print(F("Humidity: "));
      Serial.print(humidity);
      Serial.print(F("%  Temperature: "));
      Serial.print(temperature);
      Serial.println(F("°C"));

      // Refresh the OLED screen with the latest values
      updateDisplay(temperature, humidity);
    }
  }
}
