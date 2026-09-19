#pragma once
#include <stdint.h>

uint32_t InitXllnConfig();
uint32_t UninitXllnConfig();

extern wchar_t* xlln_file_config_path;
extern uint32_t XllnConfig(bool save_config);
