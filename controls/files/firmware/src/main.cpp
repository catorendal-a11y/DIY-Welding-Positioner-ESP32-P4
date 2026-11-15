/*
 * TIG Rotator Controls: behaviour from project.json
 *
 * Bring-up only for this test. The real firmware is the repository's src/ tree (FreeRTOS, LVGL 9, FastAccelStepper on RMT, E-STOP ISR on PIN_ESTOP FALLING). ENA must stay HIGH (inhibit) at boot; never pulse STEP from a bring-up test.
 *
 * Pin numbers come only from pins.h. Run the bring-up environment first:
 *   pio run -e bringup -t upload
 */
#include <Arduino.h>
#include "pins.h"

void setup() {
  // Outputs to their idle level first: until here they float, and a driver input may read that as on.
  gpio_set_level((gpio_num_t)PIN_STEP, 0);
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  gpio_set_level((gpio_num_t)PIN_DIR, 0);
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);
  gpio_set_level((gpio_num_t)PIN_ENA, 1);
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);
  Serial.begin(115200);
}

void loop() {
}
