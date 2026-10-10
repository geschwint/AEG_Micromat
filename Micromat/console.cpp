#include "console.h"
#include "perf.h"
#include "settings.h"
#include "wifi_creds.h"

static const char *HELP =
  "commands: +N / -N (seconds) | t N (set seconds) | p N (power %) | start | stop | quick\n"
  "          rot | open | close | s (status) | in (raw inputs) | perf (loop timing) | T YYYY-MM-DD HH:MM:SS (set RTC, UTC)\n"
  "          ssid NAME | pass PASSWORD (Wi-Fi network, password saved encrypted) | forget | wifi (status, sync now)\n"
  "          keygen (create the eFuse encryption key, once per chip)";

static void printStatus(Oven &oven, Door &door, Clock &clock, Outputs &outputs) {
  char clk[24] = "--:--:--";
  if (clock.valid()) strftime(clk, sizeof(clk), "%H:%M:%S", &clock.local());
  Serial.printf("[%7lu] STATUS %s | %lu s | %u %% | rotate %s | door %s | clock %s\n", millis(),
                Oven::stateName(oven.state()), (unsigned long)oven.remainingSeconds(), oven.power(),
                oven.rotate() ? "on" : "off", door.closed() ? "closed" : "OPEN", clk);
  Serial.printf("          outputs: magnetron %s (req %s), fan %s, turntable %s, lamp %s%s\n",
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
  else if ((cmd == "open" || cmd == "close") && !door.simulated()) Serial.println("door is a real sensor (SIMULATE_DOOR = false)");
  else if (cmd == "open") door.simulate(false);
  else if (cmd == "close") door.simulate(true);
  else if (cmd == "s") printStatus(oven, door, clock, outputs);
  else if (cmd == "in") inputs.printRaw();
  else if (cmd == "perf") loopStats.report();
  else if (cmd.startsWith("T ")) Serial.println(clock.setUtc(cmd.substring(2).c_str()) ? "SET OK" : "SET FAILED");
  // Wi-Fi network: `ssid` first (stored as an open network), then `pass`.
  else if (cmd.startsWith("ssid ")) {
    if (credsSave(cmd.c_str() + 5, "")) wifi.syncNow();
  } else if (cmd.startsWith("pass ")) {
    if (!credsHaveNetwork()) Serial.println("set the network first: ssid NAME");
    else if (credsSave(credsSsid(), cmd.c_str() + 5)) wifi.syncNow();
  } else if (cmd == "forget") {
    wifi.cancel();
    credsForget();
  } else if (cmd == "wifi") {
    Serial.printf("wifi: %s, network \"%s\", auto time %s, encryption key %s, %s\n", settings.wifiOn ? "on" : "OFF",
                  credsSsid(), settings.autoTime ? "on" : "off", credsKeyReady() ? "ok" : "MISSING", wifi.status());
    wifi.syncNow();
  } else if (cmd == "keygen") {
    Serial.println(credsKeyReady() ? "keygen: the encryption key already exists"
                                   : "keygen: burns a random key into an eFuse block. PERMANENT (one of six key "
                                     "blocks, nothing else changes). To go ahead: keygen YES");
  } else if (cmd == "keygen YES") {
    Serial.println(credsCreateKey() ? "keygen: OK, Wi-Fi passwords can now be saved" : "keygen: FAILED");
  }
  else Serial.println(HELP);
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
