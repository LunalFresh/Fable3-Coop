#pragma once
#include <stdint.h>

extern HWND xlln_hwnd_connections;

uint32_t InitXllnWndConnections();
uint32_t UninitXllnWndConnections();

void XllnWndConnectionsInvalidateConnections();
