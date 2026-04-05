// On-chip die temperature (ESP32-P4) — shared by System Info and ESTOP overlay

#pragma once

void onchip_temp_init();
bool onchip_temp_get_celsius(float* outC);
