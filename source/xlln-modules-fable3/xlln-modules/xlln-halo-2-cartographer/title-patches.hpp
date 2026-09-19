#pragma once
#include <stdint.h>

extern float player_controls_sensitivity_vehicle_ratios_controller[4];
extern float player_controls_sensitivity_vehicle_ratios_mouse[4];

bool InitTitlePatches();
bool UninitTitlePatches();

char* __stdcall H2GetLabel(int a1, int label_id, int a3, int a4);
bool ToggleSkull(int skullLabelIndex);
bool LoadSkullStatus(int skullLabelIndex);
void SetFOV(int field_of_view_degrees);
void SetFOVVehicle(int field_of_view_degrees);
void SetCrosshairVerticalOffset(float crosshair_offset);
void UpdateLocalPlayerControlsSensitivity(uint32_t local_player_index, float sensitivity_player, int8_t sensitivity_xy_axis_ratio_player, float sensitivity_vehicle, int8_t sensitivity_xy_axis_ratio_vehicle);
void RefreshToggleWarpFix();
void RefreshToggleXDelay();
void RefreshToggleVehicleFlipEject();
void RefreshToggleKillVolumes();
