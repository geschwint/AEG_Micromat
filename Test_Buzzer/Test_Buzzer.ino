// Buzzer test on GPIO15 for the Waveshare ESP32-S3-Touch-LCD-2.
// Works out whether the buzzer is active (beeps on DC) or passive (needs a tone).
// Phases: 1) DC on 0.5 s x2  2) tones 1/2/3/4 kHz  3) sweep 500-5000 Hz  4) "done" beeps.
// Serial commands (115200): 'a' DC test, 't' tones, 's' sweep, 'd' done beeps,
// 'f<Hz>' play one frequency for 1 s (e.g. f2700).
#include <Arduino_GFX_Library.h>

#define LCD_SCLK 39
#define LCD_MOSI 38
#define LCD_MISO 40
#define LCD_DC   42
#define LCD_CS   45
#define LCD_RST  -1
#define LCD_BL   1

#define BUZZER_PIN 15
#define TONE_RES_BITS 10

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 1 /* landscape */, true /* IPS */, 240, 320);

void showPhase(const char *text) {
  Serial.println(text);
  gfx->fillRect(0, 60, 320, 120, RGB565_BLACK);
  gfx->setTextSize(3);
  gfx->setTextColor(RGB565_YELLOW);
  gfx->setCursor(10, 90);
  gfx->print(text);
}

void buzzerOff() {
  ledcDetach(BUZZER_PIN);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void playTone(uint32_t hz, uint32_t ms) {
  ledcAttach(BUZZER_PIN, hz, TONE_RES_BITS);
  ledcWrite(BUZZER_PIN, 1 << (TONE_RES_BITS - 1));  // 50 % duty square wave
  delay(ms);
  buzzerOff();
}

void testDc() {
  showPhase("1: DC on (active?)");
  for (int i = 0; i < 2; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
  }
}

void testTones() {
  const uint32_t freqs[] = { 1000, 2000, 3000, 4000 };
  for (uint32_t f : freqs) {
    char buf[32];
    snprintf(buf, sizeof(buf), "2: tone %lu Hz", (unsigned long)f);
    showPhase(buf);
    playTone(f, 600);
    delay(400);
  }
}

void testSweep() {
  showPhase("3: sweep 0.5-5 kHz");
  ledcAttach(BUZZER_PIN, 500, TONE_RES_BITS);
  ledcWrite(BUZZER_PIN, 1 << (TONE_RES_BITS - 1));
  for (uint32_t f = 500; f <= 5000; f += 50) {
    ledcChangeFrequency(BUZZER_PIN, f, TONE_RES_BITS);
    if (f % 500 == 0) Serial.printf("  %lu Hz\n", (unsigned long)f);
    delay(40);
  }
  buzzerOff();
}

void testDoneBeeps() {
  showPhase("4: done beeps");
  for (int i = 0; i < 3; i++) {
    playTone(2700, 200);
    delay(150);
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("Test_Buzzer starting");

  gfx->begin();
  gfx->fillScreen(RGB565_BLACK);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setCursor(10, 12);
  gfx->print("Buzzer test GPIO15");

  delay(1500);
  testDc();
  delay(800);
  testTones();
  delay(800);
  testSweep();
  delay(800);
  testDoneBeeps();
  showPhase("done - see serial");
  Serial.println("commands: a, t, s, d, f<Hz>");
}

void loop() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (line.length() < 16) line += c;
      continue;
    }
    line.trim();
    if (line == "a") testDc();
    else if (line == "t") testTones();
    else if (line == "s") testSweep();
    else if (line == "d") testDoneBeeps();
    else if (line.startsWith("f")) {
      uint32_t hz = line.substring(1).toInt();
      if (hz >= 100 && hz <= 10000) {
        char buf[32];
        snprintf(buf, sizeof(buf), "tone %lu Hz", (unsigned long)hz);
        showPhase(buf);
        playTone(hz, 1000);
      }
    }
    line = "";
  }
}
