#pragma once
// Wi-Fi + punkt dostepowy awaryjny + NTP + mDNS.
#include <Arduino.h>

void wifi_begin();
void wifi_loop();
bool wifi_connected();
String wifi_mode_label();   // "STA" / "AP" / "brak"
