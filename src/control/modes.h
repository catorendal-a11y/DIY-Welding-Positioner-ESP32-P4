#pragma once
#include <Arduino.h>
#include "control.h"
#include "../motor/motor.h"
#include "../motor/speed.h"

void continuous_start();
void continuous_stop();
void continuous_update();

void pulse_start(uint32_t on_ms, uint32_t off_ms);
void pulse_stop();
void pulse_update();
uint32_t pulse_get_on_ms();
uint32_t pulse_get_off_ms();
bool pulse_is_on_phase();

void step_execute(float angle_deg);
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

void timer_start(uint32_t duration_sec);
void timer_stop();
void timer_update();
uint32_t timer_get_remaining_sec();
uint32_t timer_get_duration();
