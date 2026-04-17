// TIG Rotator Controller - Cross-core shared atomics
//
// Single source of truth for every std::atomic flag that is touched by more
// than one FreeRTOS task / ISR. Putting them here makes ownership explicit
// and prevents drift between .h and .cpp declarations.
//
// Conventions:
//   - Writer uses memory_order_release (or acq_rel for exchange).
//   - Reader uses memory_order_acquire.
//   - "relaxed" is only acceptable for counters/values with no happens-before
//     relationship to other state.
//
// RISC-V SMP note: `volatile` alone is NOT sufficient for cross-core visibility.
// Always use std::atomic with explicit memory ordering.

#pragma once
#include <atomic>
#include <cstdint>

// ───────────────────────────────────────────────────────────────────────────────
// Safety (safetyTask on Core 0, ISR on Core 0, UI on Core 1)
// ───────────────────────────────────────────────────────────────────────────────
extern std::atomic<bool>     g_estopPending;     // ISR/boot set; safetyTask clears
extern std::atomic<uint32_t> g_estopTriggerMs;   // safetyTask sets debounce start; 0 = unset
extern std::atomic<bool>     g_uiResetPending;   // UI (Core 1) set; safetyTask consumes

// ───────────────────────────────────────────────────────────────────────────────
// Display / input wake (Core 0 sets on activity; Core 1 clears in dim_update)
// ───────────────────────────────────────────────────────────────────────────────
extern std::atomic<bool>     g_wakePending;

// ───────────────────────────────────────────────────────────────────────────────
// Storage / flash coordination (storageTask on Core 1 owns the writes)
// ───────────────────────────────────────────────────────────────────────────────
extern std::atomic<bool>     g_dir_switch_cache; // cached setting for DIR switch enable
extern std::atomic<bool>     g_flashWriting;     // storageTask holds true during flash I/O
extern std::atomic<bool>     g_screenRedraw;     // request full screen invalidate

// ───────────────────────────────────────────────────────────────────────────────
// UI → Core 0 requests
// ───────────────────────────────────────────────────────────────────────────────
extern std::atomic<bool>     motorConfigApplyPending; // UI touched Motor Config, motorTask re-applies

// ───────────────────────────────────────────────────────────────────────────────
// Fatal shutdown (use instead of raw ESP.restart() at init failures).
// Logs the reason with LOG_E (which is compiled into release builds),
// drains serial, and reboots. Never returns.
// ───────────────────────────────────────────────────────────────────────────────
[[noreturn]] void fatal_halt(const char* reason);
