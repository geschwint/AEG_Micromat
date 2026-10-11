// The last crash, from the core dump ESP-IDF saves in the `coredump` flash
// partition when the firmware panics (it restarts right after, so the serial
// output is usually lost). Logged at start-up and with the console command
// `crash`; `crash clear` erases it. Decode the addresses on the PC with
// xtensa-esp32s3-elf-addr2line and the ELF of the build named in the report.
//
// Also records failed memory allocations (size, memory kind, function, free
// internal RAM at the time): logged when they happen, and the last one is kept
// in RTC memory across a restart, so it's reported after a crash too.
#pragma once

void crashReportBegin();           // first thing in setup(): registers the allocation hook
void crashReportLog(bool always);  // always = false: only if there is something to report (start-up)
void crashReportClear();
