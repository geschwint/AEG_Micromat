// Display bring-up test for Waveshare ESP32-S3-Touch-LCD-2 (ST7789T3, 240x320, SPI)
// Pins from Waveshare's demo package (02_gfx_helloworld).
#include <Arduino_GFX_Library.h>

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1  // not connected
#define LCD_BL   1   // backlight, active high

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1 /* rotation: landscape */, true /* IPS */, 240, 320);

unsigned long lastUpdate = 0;

void drawColorBars() {
  const uint16_t colors[] = { RGB565_RED, RGB565_GREEN, RGB565_BLUE, RGB565_YELLOW, RGB565_CYAN, RGB565_MAGENTA, RGB565_WHITE };
  const int n = sizeof(colors) / sizeof(colors[0]);
  int w = gfx->width() / n;
  for (int i = 0; i < n; i++) {
    gfx->fillRect(i * w, gfx->height() - 40, w, 40, colors[i]);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Test_Display starting");

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(10, 10);
  gfx->println("AEG Micromat");
  gfx->setTextSize(2);
  gfx->setCursor(10, 50);
  gfx->printf("%d x %d\n", gfx->width(), gfx->height());

  drawColorBars();
  Serial.printf("Display %d x %d ready\n", gfx->width(), gfx->height());
}

void loop() {
  // Uptime counter proves the loop is alive; redraw only the number area.
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    unsigned long s = millis() / 1000;
    gfx->fillRect(10, 90, 300, 40, RGB565_BLACK);
    gfx->setTextSize(4);
    gfx->setTextColor(RGB565_ORANGE);
    gfx->setCursor(10, 90);
    gfx->printf("%02lu:%02lu", s / 60, s % 60);
    Serial.printf("uptime %lu s\n", s);
  }
}
