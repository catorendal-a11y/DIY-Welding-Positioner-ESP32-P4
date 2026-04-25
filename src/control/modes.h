#pragma once
#include <Arduino.h>
#include "control.h"
#include "../motor/motor.h"
#include "../motor/speed.h"

void continuous_start(bool soft_start, uint32_t auto_stop_ms);
void continuous_stop();
void continuous_update();

void pulse_start(uint32_t on_ms, uint32_t off_ms, uint16_t cycles);
void pulse_stop();
void pulse_update();
uint32_t pulse_get_on_ms();
uint32_t pulse_get_off_ms();
bool pulse_is_on_phase();
uint16_t pulse_get_cycle_count();
uint16_t pulse_get_cycle_limit();

void step_execute(float angle_deg);
void step_execute_sequence(float angle_deg, uint16_t repeats, float dwell_sec);
void step_update();
void step_reset_accumulator();
float step_get_accumulated();
float step_get_current_angle();
long step_get_count();

void jog_start(Direction dir);
void jog_stop();
void jog_update();
void jog_set_speed(float rpm);
float jog_get_speed();
