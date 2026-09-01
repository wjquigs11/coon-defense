#include <Arduino.h>
#include "esp_system.h"
#include "esp_debug_helpers.h"
#include "esp32-hal-log.h"
#include "esp_attr.h"
#include "esp_err.h"

// Custom abort handler function
void IRAM_ATTR custom_abort_handler(void) {
  log_printf("Custom abort handler called!\n");

  // Print the backtrace
  log_printf("Backtrace:\n");
  esp_backtrace_print(100);

  // Print free heap
  log_printf("Free heap: %u\n", esp_get_free_heap_size());

  // Print minimum free heap
  log_printf("Minimum free heap: %u\n", esp_get_minimum_free_heap_size());

  // Print stack pointer and other registers
  uint32_t sp;
  __asm__ __volatile__("mov %0, sp" : "=r"(sp));
  log_printf("Stack pointer: 0x%08x\n", sp);

  // Give UART time to flush then restart
  for (volatile int i = 0; i < 1000000; i++) {}
  esp_restart();
  // Should not reach here, but satisfy noreturn
  while (true) {}
}

// Override the abort function which gets called by panic handler
extern "C" void IRAM_ATTR abort(void) {
  custom_abort_handler();
}

// Setup function to initialize custom panic handling
void setup_custom_panic_handler() {
  // The abort handler is automatically used by defining abort() above
  log_printf("Custom panic handler initialized\n");
}
