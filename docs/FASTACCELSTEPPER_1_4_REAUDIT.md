# FastAccelStepper 1.4.0 re-audit — 3 October 2026

The installed repository HEAD and upstream `refs/tags/1.4.0` both resolve to `f24a65996bdc501a0497cdca6a636706003333e7`. The previous conclusions were challenged against this code, not a generic documentation summary or the different registry artifact.

| Question | Verified behavior and project action |
|---|---|
| Does 1.4.0 make `forceStop()` synchronous? | No. It sets `ignore_commands` and requests the ramp stop; queued output is processed. Keep ENA inhibition immediate and wait for `isRunning()` before permitting reset. The header's approximate 20 ms is not a measured bound for this machine. |
| Can `forceStopAndNewPosition()` replace it? | It stops the RMT channel and clears the queue, but explicitly discards the actual stop position and requires a replacement position. It is not a transparent substitute for the current stop path. |
| Does `isRunning()` include queued work? | Yes: driver running, ramp active, or queue nonempty. The project correctly waits on this condition, including the RMT transaction completion callback. |
| Is position physical feedback? | No. RMT's performed-pulse helper returns zero; queued/encoded position can run ahead. Estimated UI progress and measured calibration remain distinct. |
| Are return types new in 1.4.0? | `MoveResultCode` already existed in 0.33.14. Explicit enum comparisons strengthen the adapter; speed/acceleration setters still use `int8_t`. |
| What are actual DIR budgets? | IDF5 RMT uses a 24,000-tick before-change pause (1.5 ms at 16 MHz). Public pause-count getter reports one. DM542T requests another 200 µs after the change; standard mode requests zero after-change delay but still retains RMT's before-change drain. This is a scheduling budget, not a physical stop-time specification. |
| Does `engine.init(0)` put the RMT IRQ on Core 0? | No. It pins `StepperTask`; IDF installs the interrupt inside `rmt_new_tx_channel()` on the allocation caller. The old source comment was wrong. Motor initialization now runs through a temporary Core 0 task when setup runs elsewhere. Boot waits at most 5 seconds, with ENA inhibited, and stops on task allocation/timeout failure. |
| Are high-level moves affected by `moveTimed()` retry codes? | The application uses ramped `move()`/run methods; internal queue retries belong to the library. Adding an application retry loop would risk duplicate motion. |
| Do experimental planners improve one axis? | `FasNAxis`, `FasTimed` and synchronized starts are separate, experimental APIs. The existing single RMT axis uses none of them. |
| Does a non-null allocation prove all RMT driver operations succeeded? | No. Upstream logs several IDF errors with `ESP_ERROR_CHECK_WITHOUT_ABORT`; the public pointer check does not prove peripheral health. Host compilation/tests and a successful allocation are not substitutes for device commissioning. No private-library patch is introduced. |

The Core 0 allocation change is compiled in all three real firmware variants. Native/SDL adapters do not execute ESP32 task creation or establish interrupt affinity; actual boot logs and pulse measurements are still required on the board. Encoder regression compiles the actual pinned upstream core and IDF5/6 fill encoder with both buffer geometries.

Sources: [tag 1.4.0](https://github.com/gin66/FastAccelStepper/releases/tag/1.4.0), [public methods and stop implementation](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/src/FastAccelStepper.cpp), [RMT driver](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/src/pd_esp32/StepperISR_idf5_esp32_rmt.cpp), [DIR scheduling](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/src/pd_esp32/esp32_queue.h), [ESP-IDF interrupt allocation](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_driver_rmt/src/rmt_tx.c).
