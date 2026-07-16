#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define NUM_STARS 60

struct Star {
  float x;
  float y;
  float z;
};

Star stars[NUM_STARS];
float speed = 2.5; // Star travel speed

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 20; // 20ms per frame (~50 FPS)

// Reset a star to a far distance with random center-offset coordinates
void initStar(int i) {
  stars[i].x = random(-64, 64);
  stars[i].y = random(-32, 32);
  stars[i].z = random(40, 200); // Start far away in the background
}

void setup() {
  Serial.begin(115200);

  // Initialize display with I2C address 0x3C
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Seed random generator using an unconnected analog pin
  randomSeed(analogRead(0));

  // Initialize all stars, spreading them out randomly along the depth axis (Z)
  for (int i = 0; i < NUM_STARS; i++) {
    initStar(i);
    stars[i].z = random(1, 200);
  }

  display.clearDisplay();
  display.display();
}

void loop() {
  unsigned long currentMillis = millis();

  // Run the animation updates on a non-blocking timer
  if (currentMillis - lastUpdate >= updateInterval) {
    lastUpdate = currentMillis;

    display.clearDisplay();

    for (int i = 0; i < NUM_STARS; i++) {
      // Bring the star closer by reducing its depth coordinate
      stars[i].z -= speed;

      // If the star has flown past the screen, reset it
      if (stars[i].z <= 0) {
        initStar(i);
      }

      // Convert 3D space (x, y, z) to 2D screen coordinates (sx, sy)
      // Screen center is (64, 32)
      int sx = (int)(stars[i].x * 128.0 / stars[i].z) + 64;
      int sy = (int)(stars[i].y * 64.0 / stars[i].z) + 32;

      // If star moves outside screen boundaries, reset it
      if (sx < 0 || sx >= SCREEN_WIDTH || sy < 0 || sy >= SCREEN_HEIGHT) {
        initStar(i);
      } else {
        // Draw the star. Size depends on closeness (z depth) to create a sense of depth
        if (stars[i].z < 60) {
          // Close star: draw a bright 2x2 square
          display.fillRect(sx, sy, 2, 2, SSD1306_WHITE);
        } else {
          // Far star: draw a single pixel
          display.drawPixel(sx, sy, SSD1306_WHITE);
        }
      }
    }

    display.display();
  }
}


