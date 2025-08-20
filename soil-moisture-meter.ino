#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <DHT.h>

// Pin definitions (change if you use different pins)
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8

#define DHTPIN 7       // DHT11 data pin connected to D7
#define DHTTYPE DHT11  // DHT 11 (AM2302)

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  tft.begin();
  tft.setRotation(0);  
  tft.fillScreen(GC9A01A_BLACK);
  dht.begin();
}

void drawArc(int16_t cx, int16_t cy, int16_t r, float start_angle, float end_angle, uint16_t color, uint8_t thickness) {
  if (end_angle < start_angle) {
    // Handle counterclockwise drawing
    for (float angle = start_angle; angle >= end_angle; angle -= 0.5) {
      float rad = angle * DEG_TO_RAD;
      for (int t = 0; t < thickness; t++) {
        int16_t x = cx + (r - t) * cos(rad);
        int16_t y = cy + (r - t) * sin(rad);
        tft.drawPixel(x, y, color);
      }
    }
  } else {
    // Handle clockwise drawing
    for (float angle = start_angle; angle <= end_angle; angle += 0.5) {
      float rad = angle * DEG_TO_RAD;
      for (int t = 0; t < thickness; t++) {
        int16_t x = cx + (r - t) * cos(rad);
        int16_t y = cy + (r - t) * sin(rad);
        tft.drawPixel(x, y, color);
      }
    }
  }
}

// Add this new function before loop()
void drawScaleText(int16_t cx, int16_t cy, int16_t r, float angle, const char* text, uint16_t color) {
  float rad = angle * DEG_TO_RAD;
  int16_t x1, y1;
  uint16_t w, h;
  
  tft.cp437(true);     // Use the smallest possible font
  tft.setTextSize(0);  // Smallest size
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  
  // Position text slightly inside the arc
  int16_t x = cx + (r - 20) * cos(rad) - w/2;  // Moved even closer to arc
  int16_t y = cy + (r - 20) * sin(rad) - h/2;
  
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // Clear screen
  tft.fillScreen(GC9A01A_BLACK);

  // Debug: Try reading multiple times if error
  if (isnan(h) || isnan(t)) {
    delay(100);
    h = dht.readHumidity();
    t = dht.readTemperature();
  }

  int r = (tft.width() < tft.height() ? tft.width() : tft.height()) / 2 - 4;

  // Draw temperature meter on left side
  if (!isnan(t)) {
    float temp = constrain(t, 0, 60);
    float temp_angle = map(temp, 0, 60, 90, 270);
    drawArc(tft.width()/2, tft.height()/2, r, 90, temp_angle, GC9A01A_RED, 20);
    
    // Draw temperature scale (skip 0 and 60)
    for(int i = 5; i <= 55; i += 5) {
      float scale_angle = map(i, 0, 60, 90, 270);
      char tempStr[4];
      sprintf(tempStr, "%d", i);
      drawScaleText(tft.width()/2, tft.height()/2, r, scale_angle, tempStr, GC9A01A_WHITE);
    }
  }

  // Draw humidity meter on right side
  if (!isnan(h)) {
    float hum = constrain(h, 0, 100);
    float hum_angle = map(hum, 0, 100, 90, -90); // 90° through 0° to -90° (same as 270°)
    drawArc(tft.width()/2, tft.height()/2, r, 90, hum_angle, GC9A01A_GREEN, 20);
    
    // Draw humidity scale (skip 0 and 100)
    for(int i = 10; i <= 90; i += 10) {
      float scale_angle = map(i, 0, 100, 90, -90);
      char humStr[4];
      sprintf(humStr, "%d", i);
      drawScaleText(tft.width()/2, tft.height()/2, r, scale_angle, humStr, GC9A01A_WHITE);
    }
  }

  // Show temperature and humidity in white in the center with circle
  if (!isnan(h) && !isnan(t)) {
    tft.setTextColor(GC9A01A_WHITE);
    tft.setTextSize(3);

    char tempStr[16];
    char humStr[16];
    sprintf(tempStr, "T:%dC", (int)t);
    sprintf(humStr, "H:%d%%", (int)h);

    // Calculate text bounds for centering
    int16_t x1, y1;
    uint16_t w, htxt;
    tft.getTextBounds(tempStr, 0, 0, &x1, &y1, &w, &htxt);

    int16_t x = (tft.width() - w) / 2;
    int16_t y = (tft.height() - (htxt * 2 + 8)) / 2;

    // Draw white circle around text area
    int16_t circle_radius = (htxt * 2 + 16);
    tft.drawCircle(tft.width()/2, tft.height()/2, circle_radius, GC9A01A_WHITE);
    tft.drawCircle(tft.width()/2, tft.height()/2, circle_radius-1, GC9A01A_WHITE);

    tft.setCursor(x, y);
    tft.print(tempStr);
    tft.setCursor(x, y + htxt + 8);
    tft.print(humStr);
  } else {
    // Show error message if sensor not found
    tft.setTextColor(GC9A01A_RED);
    tft.setTextSize(2);
    const char* err = "Sensor Error";
    int16_t x1, y1;
    uint16_t w, htxt;
    tft.getTextBounds(err, 0, 0, &x1, &y1, &w, &htxt);
    int16_t x = (tft.width() - w) / 2;
    int16_t y = (tft.height() - htxt) / 2;
    tft.setCursor(x, y);
    tft.print(err);
  }

  delay(5000);
}