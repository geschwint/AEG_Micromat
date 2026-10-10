// Relay output test for the VMA400 4-channel board (active-high inputs) on the
// Waveshare ESP32-S3-Touch-LCD-2. Run with NOTHING connected to the relay contacts.
// At boot: all relays OFF first, then one cycle (each relay ON 1 s in turn).
// Serial commands (115200): '1'..'4' toggle a relay, '0' all off, 'c' run the cycle.
#include <Arduino_GFX_Library.h>

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

struct Relay {
  uint8_t pin;
  const char *name;
  bool on;
};

// Pin map from HARDWARE.md (K1 magnetron on GPIO17 is not part of this test).
Relay relays[] = {
  { 18, "K2 fan", false },
  { 8,  "K3 turntable", false },
  { 9,  "K4 lamp", false },
  { 10, "K5 spare", false },
};
const int NUM_RELAYS = sizeof(relays) / sizeof(relays[0]);

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1 /* landscape */, true /* IPS */, 240, 320);

void drawStatus() {
  gfx->setTextSize(3);
  for (int i = 0; i < NUM_RELAYS; i++) {
    int y = 50 + i * 45;
    gfx->fillRect(0, y, 320, 40, RGB565_BLACK);
    gfx->setCursor(10, y + 8);
    gfx->setTextColor(relays[i].on ? RGB565_GREEN : RGB565_DARKGREY);
    gfx->printf("%d %-12s %s", i + 1, relays[i].name, relays[i].on ? "ON" : "off");
  }
}

void setRelay(int i, bool on) {
  relays[i].on = on;
  digitalWrite(relays[i].pin, on ? HIGH : LOW);
  Serial.printf("%-13s GPIO%-2d -> %s\n", relays[i].name, relays[i].pin, on ? "ON" : "off");
  drawStatus();
}

void allOff() {
  for (int i = 0; i < NUM_RELAYS; i++) setRelay(i, false);
}

void runCycle() {
  Serial.println("cycle: each relay ON for 1 s");
  for (int i = 0; i < NUM_RELAYS; i++) {
    setRelay(i, true);
    delay(1000);
    setRelay(i, false);
    delay(500);
  }
  Serial.println("cycle done");
}

void setup() {
  // Safety first: drive every relay input LOW before anything else.
  for (int i = 0; i < NUM_RELAYS; i++) {
    digitalWrite(relays[i].pin, LOW);
    pinMode(relays[i].pin, OUTPUT);
  }

  Serial.begin(115200);
  delay(500);
  Serial.println("Test_Relays starting, all relays OFF");

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(10, 12);
  gfx->print("Relay test (no loads!)");
  drawStatus();

  delay(2000);  // time to watch that nothing clicks after boot
  runCycle();
  Serial.println("commands: 1-4 toggle, 0 all off, c cycle");
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c >= '1' && c <= '0' + NUM_RELAYS) {
    int i = c - '1';
    setRelay(i, !relays[i].on);
  } else if (c == '0') {
    allOff();
  } else if (c == 'c') {
    runCycle();
  }
}
