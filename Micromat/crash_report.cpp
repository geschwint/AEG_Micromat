#include "crash_report.h"
#include "debug_log.h"
#include <esp_attr.h>
#include <esp_core_dump.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <esp_system.h>

// Survives a software restart or crash (not a power cut).
struct AllocFail {
  uint32_t magic;
  uint32_t count;
  uint32_t size, caps;
  const char *func;  // a string in flash: valid after the restart with the same firmware
  uint32_t freeInternal, largestInternal;
};
static constexpr uint32_t ALLOC_MAGIC = 0xA110CFA1;
RTC_NOINIT_ATTR static AllocFail lastFail;
static AllocFail prevFail;  // from before this start
static bool havePrev = false;

// Runs in the task whose allocation failed, possibly with locks held: no Log
// (it takes a mutex), only the ROM printf and plain stores.
static void IRAM_ATTR onAllocFailed(size_t size, uint32_t caps, const char *func) {
  if (lastFail.magic != ALLOC_MAGIC) lastFail.count = 0;
  lastFail.magic = ALLOC_MAGIC;
  lastFail.count++;
  lastFail.size = size;
  lastFail.caps = caps;
  lastFail.func = func;
  lastFail.freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  lastFail.largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  esp_rom_printf("ALLOC FAILED: %u bytes, caps 0x%x, in %s; internal free %u, largest %u\n", (unsigned)size,
                 (unsigned)caps, func ? func : "?", (unsigned)lastFail.freeInternal, (unsigned)lastFail.largestInternal);
}

void crashReportBegin() {
  // malloc() puts blocks under 4 KB in internal RAM first. LVGL allocates
  // with malloc, and its font caches (4 sizes x glyphs + kerning) can then use
  // up all ~150 KB of it: once `texts 0..4` left 504 bytes. FreeRTOS objects
  // must be in internal RAM, so the next lock created on first use (eFuse,
  // console UART) got NULL and the firmware aborted (2026-10-11, twice). Make
  // every malloc() try PSRAM (8 MB) first; code that needs internal RAM (Wi-Fi,
  // DMA, FreeRTOS) asks for it explicitly with heap_caps_malloc().
  heap_caps_malloc_extmem_enable(0);
  if (esp_reset_reason() != ESP_RST_POWERON && lastFail.magic == ALLOC_MAGIC) {
    prevFail = lastFail;
    havePrev = true;
  }
  lastFail.magic = 0;
  heap_caps_register_failed_alloc_callback(onAllocFailed);
}

static const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "power on";
    case ESP_RST_SW:       return "restart";
    case ESP_RST_PANIC:    return "CRASH";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT:      return "watchdog";
    case ESP_RST_BROWNOUT: return "brown-out";
    case ESP_RST_USB:      return "USB";
    case ESP_RST_EXT:      return "reset pin";
    default:               return "other";
  }
}

static void logFail(const char *when, const AllocFail &f) {
  Log.printf("alloc: %s %lu failed allocation(s), last %lu bytes, caps 0x%lx, in %s; internal free %lu, largest %lu\n",
             when, (unsigned long)f.count, (unsigned long)f.size, (unsigned long)f.caps, f.func ? f.func : "?",
             (unsigned long)f.freeInternal, (unsigned long)f.largestInternal);
}

void crashReportLog(bool always) {
  if (always || esp_reset_reason() != ESP_RST_POWERON) Log.printf("reset reason: %s\n", resetReason());
  if (havePrev) logFail("before the restart:", prevFail);
  if (lastFail.magic == ALLOC_MAGIC) logFail("since start-up:", lastFail);
  else if (always) Log.println("alloc: no failed allocations since start-up");
  if (esp_core_dump_image_check() != ESP_OK) {
    if (always) Log.println("crash: no core dump saved");
    return;
  }
  esp_core_dump_summary_t s;
  if (esp_core_dump_get_summary(&s) != ESP_OK) {
    Log.println("crash: core dump saved but unreadable");
    return;
  }
  Log.printf("crash: last saved crash, build %.8s, task \"%s\", PC 0x%08lx, cause %lu, vaddr 0x%08lx\n",
             (const char *)s.app_elf_sha256, s.exc_task, (unsigned long)s.exc_pc,
             (unsigned long)s.ex_info.exc_cause, (unsigned long)s.ex_info.exc_vaddr);
  Log.print("crash: backtrace");
  for (uint32_t i = 0; i < s.exc_bt_info.depth && i < 16; i++) Log.printf(" 0x%08lx", (unsigned long)s.exc_bt_info.bt[i]);
  Log.println(s.exc_bt_info.corrupted ? " (corrupted)" : "");
  if (!always) Log.println("crash: `crash` shows it again, `crash clear` erases it");
}

void crashReportClear() {
  Log.println(esp_core_dump_image_erase() == ESP_OK ? "crash: core dump erased" : "crash: erase FAILED");
}
