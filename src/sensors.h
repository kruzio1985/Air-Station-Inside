#pragma once
// Orchestrator czujnikow: czyta sterowniki i aktualizuje tablice channels[].
// Ruch: lokalny PIR (przerwanie GPIO) + zdarzenia z MQTT (czujka Tuya w HA).
#include <Arduino.h>

void  sensors_begin();
void  sensors_loop();

bool  motion_active();          // ruch w ciagu ostatnich 60 s
bool  pir_raw_level();          // surowy stan pinu PIR (do diagnostyki na www)
void  note_motion_from_mqtt();  // zdarzenie ruchu z zewnatrz (MQTT)
void  note_motion_local();      // ruch z lokalnego PIR
