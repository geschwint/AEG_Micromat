// LVGL user interface: seven views (splash, main, dimmed, menu, editor, service, hotspot), see the GUI
// requirements in PLAN.md. It only renders; App decides what inputs do.
// Refreshes every 33 ms, and only touches an LVGL object when its value really
// changes: any setter (even with the same value) makes LVGL redraw, and a
// redraw blocks loop() for 13-80 ms.
#pragma once
#include <Arduino_GFX_Library.h>
#include "app.h"
#include "oven.h"
#include "door.h"
#include "clock.h"
#include "outputs.h"
#include "inputs.h"
#include "wifi_time.h"

void uiBegin(Arduino_GFX *gfx, App &app, Oven &oven, Door &door, Clock &clock, Outputs &outputs, Inputs &inputs,
             WifiTime &wifi);
void uiUpdate();  // call every loop(); runs LVGL timers, never blocks
// Service (console `texts N`): logs the width in px of every UI text of
// language N in the 14/20/28 px fonts and the big digits, to check translations fit.
void uiReportTexts(uint8_t language);
