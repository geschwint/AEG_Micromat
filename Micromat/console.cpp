#include "console.h"
#include "crash_report.h"
#include "debug_log.h"
#include "perf.h"
#include "settings.h"
#include "wifi_creds.h"
#include "eggs.h"
#include "ui.h"
#include <esp_heap_caps.h>

static const char *HELP =
  "commands: +N / -N (seconds) | t N (set seconds) | p N (power, watts) | start | stop | quick\n"
  "          rot | open | close | s (status) | in (raw inputs) | perf (loop timing) | mem | crash (last crash) | crash clear | T YYYY-MM-DD HH:MM:SS (set RTC, UTC)\n"
  "          ssid NAME | pass PASSWORD (Wi-Fi network, password saved encrypted) | forget | wifi (status, sync now)\n"
  "          log (debug log status) | log on | log off (debug log on the SD card)\n"
  "          egg (as the long Rotate press; door open) | egg melody on | egg melody off\n"
  "          keygen (create the eFuse encryption key, once per chip)";

static void printStatus(Oven &oven, Door &door, Clock &clock, Outputs &outputs) {
  char clk[24] = "--:--:--";
  if (clock.valid()) strftime(clk, sizeof(clk), "%H:%M:%S", &clock.local());
  Log.printf("[%7lu] STATUS %s | %lu s | %u W | rotate %s | door %s | clock %s\n", millis(),
                Oven::stateName(oven.state()), (unsigned long)oven.remainingSeconds(), oven.power(),
                oven.rotate() ? "on" : "off", door.closed() ? "closed" : "OPEN", clk);
  Log.printf("          outputs: magnetron %s (req %s), fan %s, turntable %s, lamp %s%s\n",
                outputs.isOn(OUT_MAGNETRON) ? "ON" : "off", outputs.isRequested(OUT_MAGNETRON) ? "on" : "off",
                outputs.isOn(OUT_FAN) ? "ON" : "off", outputs.isOn(OUT_TURNTABLE) ? "ON" : "off",
                outputs.isOn(OUT_LAMP) ? "ON" : "off", oven.fanRunningOn() ? " (fan run-on)" : "");
}

static void run(const String &cmd, Oven &oven, Door &door, Clock &clock, Outputs &outputs, Inputs &inputs,
                WifiTime &wifi) {
  if (cmd.startsWith("+") || cmd.startsWith("-")) oven.addSeconds(cmd.toInt());
  else if (cmd.startsWith("t ")) oven.setSeconds(cmd.substring(2).toInt());
  else if (cmd.startsWith("p ")) oven.setPower(cmd.substring(2).toInt());
  else if (cmd == "start") oven.start();
  else if (cmd == "stop") oven.stop();
  else if (cmd == "quick") oven.quickStart();
  else if (cmd == "rot") oven.toggleRotate();
  else if ((cmd == "open" || cmd == "close") && !door.simulated()) Log.println("door is a real sensor (SIMULATE_DOOR = false)");
  else if (cmd == "open") door.simulate(false);
  else if (cmd == "close") door.simulate(true);
  else if (cmd == "s") printStatus(oven, door, clock, outputs);
  else if (cmd == "in") inputs.printRaw();
  else if (cmd == "perf") loopStats.report();
  else if (cmd == "crash") crashReportLog(true);
  else if (cmd == "crash clear") crashReportClear();
  else if (cmd == "mem")
    Log.printf("mem: internal %u free, largest %u, min ever %u | PSRAM %u free\n",
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
               (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  else if (cmd.startsWith("T ")) Log.println(clock.setUtc(cmd.substring(2).c_str()) ? "SET OK" : "SET FAILED");
  // Wi-Fi network: `ssid` first (stored as an open network), then `pass`.
  else if (cmd.startsWith("ssid ")) {
    if (credsSave(cmd.c_str() + 5, "")) wifi.syncNow();
  } else if (cmd.startsWith("pass ")) {
    if (!credsHaveNetwork()) Log.println("set the network first: ssid NAME");
    else if (credsSave(credsSsid(), cmd.c_str() + 5)) wifi.syncNow();
  } else if (cmd == "forget") {
    wifi.cancel();
    credsForget();
  } else if (cmd == "wifi") {
    char pass[WIFI_PASS_MAX + 1];
    bool passOk = credsPassword(pass, sizeof(pass));  // checks that it decrypts; never printed
    memset(pass, 0, sizeof(pass));
    char ip[16] = "-";
    credsLastIp(ip, sizeof(ip));
    Log.printf("wifi: %s, network \"%s\"%s, last IP %s, auto time %s, encryption key %s, %s\n",
                  settings.wifiOn ? "on" : "OFF", credsSsid(),
                  !credsHaveNetwork() ? "" : passOk ? " (password ok)" : " (password UNREADABLE)", ip,
                  settings.autoTime ? "on" : "off", credsKeyReady() ? "ok" : "MISSING", wifi.status());
    wifi.syncNow();
  } else if (cmd == "log on" || cmd == "log off") {
    settings.debugLog = cmd == "log on";
    saveSettings();
    if (!Log.setEnabled(settings.debugLog)) Log.println("log: no SD card (stays on: starts when a card is there at start-up)");
  } else if (cmd == "log") {
    bool card = Log.cardOk();
    Log.printf("log: %s, %s, card %s", settings.debugLog ? "on" : "off", Log.active() ? "writing" : "not writing",
               card ? "ok" : "MISSING");
    for (uint8_t i = 0; card && i < DebugLog::FILES; i++) Log.printf(", %s %u bytes", DebugLog::fileName(i), (unsigned)Log.fileSize(i));
    Log.println();
  } else if (cmd == "egg") {  // as the long Rotate press (still needs the door open)
    inputs.inject(InputEvent::RotateLong);
  } else if (cmd == "egg melody on" || cmd == "egg melody off") {
    eggs.melody = cmd.endsWith("on");
    saveEggs();
    Log.printf("eggs: end melody %s\n", eggs.melody ? "on" : "off");
  } else if (cmd.startsWith("texts ")) {
    uiReportTexts(cmd.substring(6).toInt());
  } else if (cmd == "keygen") {
    Log.println(credsKeyReady() ? "keygen: the encryption key already exists"
                                   : "keygen: burns a random key into an eFuse block. PERMANENT (one of six key "
                                     "blocks, nothing else changes). Settings -> Wi-Fi -> Network creates it too. "
                                     "To go ahead: keygen YES");
  } else if (cmd == "keygen YES") {
    Log.println(credsCreateKey() ? "keygen: OK, Wi-Fi passwords can now be saved" : "keygen: FAILED");
  }
  else Log.println(HELP);
}

void consoleUpdate(Oven &oven, Door &door, Clock &clock, Outputs &outputs, Inputs &inputs, WifiTime &wifi) {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (line.length() < 100) line += c;
      continue;
    }
    line.trim();
    if (line.length()) run(line, oven, door, clock, outputs, inputs, wifi);
    line = "";
  }
}
