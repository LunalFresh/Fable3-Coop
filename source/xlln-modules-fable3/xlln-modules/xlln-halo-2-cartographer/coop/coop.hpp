#pragma once
#include <stdint.h>

extern bool coop_mode_fix_active;
extern uint8_t coop_mode_fix_setting_state;

void CoopInitializeGameEngineSystems(uint8_t *a1);
void CoopSpawnPlayer(int32_t player_index);
void CoopSetPlayerUnitDatumAndModel(int32_t player_index, int32_t *unit_datum, int32_t *player_model);

void ToggleCoopModeFix(uint8_t setting_state);
void LoopCoop();
bool InitCoop();
bool UninitCoop();
