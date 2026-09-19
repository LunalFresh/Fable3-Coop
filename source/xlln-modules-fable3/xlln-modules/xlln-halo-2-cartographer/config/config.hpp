#pragma once
#include <stdint.h>

extern bool H2Portable;
extern int H2Config_language_code_main;
extern int H2Config_language_code_variant;
extern bool H2Config_custom_labels_capture_missing;
extern int H2Config_hotkeyIdGuide;
extern bool H2Config_hide_ingame_chat;
extern bool H2Config_hide_first_person;
extern bool H2Config_hide_HUD;
extern int32_t H2Config_field_of_view;
extern int32_t H2Config_field_of_view_vehicle;
extern float H2Config_crosshair_offset;
extern float H2Config_sensitivity_native_controller;
extern float H2Config_sensitivity_native_mouse;
extern uint32_t H2Config_sensitivity_raw_mouse;
extern int8_t H2Config_sensitivity_xy_axis_ratio_controller;
extern int8_t H2Config_sensitivity_xy_axis_ratio_mouse;
extern float& H2Config_sensitivity_vehicle_ratio_controller;
extern float& H2Config_sensitivity_vehicle_ratio_mouse;
extern bool H2Config_xDelay_enabled;
extern bool H2Config_warp_fix_enabled;
extern bool H2Config_vehicle_flip_eject_enabled;
extern bool H2Config_kill_volumes_enabled;

HRESULT InitConfig();
HRESULT UninitConfig();
