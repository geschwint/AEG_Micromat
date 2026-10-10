// Serial command console (115200) for testing without the panel hardware.
// Reads characters without blocking; a command runs when its line ends.
#pragma once
#include "oven.h"
#include "door.h"
#include "clock.h"
#include "outputs.h"
#include "inputs.h"
#include "wifi_time.h"

void consoleUpdate(Oven &oven, Door &door, Clock &clock, Outputs &outputs, Inputs &inputs, WifiTime &wifi);
