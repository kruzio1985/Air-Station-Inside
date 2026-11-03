#include "syslog.h"
#include <stdarg.h>
#include <stdio.h>
#include <esp_heap_caps.h>

#define SYSLOG_CAP 65536    // 64 kB bufor w PSRAM

static char*    s_buf   = nullptr;
static size_t   s_len   = 0;
static bool     s_psram = false;

void syslog_init() {
    if (s_buf) return;
    // PSRAM preferowany (stacja N16R8 ma 8 MB); bez niego zwykly heap.
    s_buf = (char*)heap_caps_malloc(SYSLOG_CAP, MALLOC_CAP_SPIRAM);
    if (s_buf) { s_psram = true; }
    else       { s_buf = (char*)malloc(SYSLOG_CAP); s_psram = false; }
    if (s_buf) s_buf[0] = 0;
}

static void log_write(char lvl, const char* fmt, va_list va) {
    if (!s_buf) syslog_init();
    if (!s_buf) return;

    char line[256];
    vsnprintf(line, sizeof(line), fmt, va);

    uint32_t t = millis();
    char stamp[32];
    snprintf(stamp, sizeof(stamp), "[%7lu.%03lu] %c ", (unsigned long)(t/1000), (unsigned long)(t%1000), lvl);
    size_t sl = strlen(stamp);
    size_t ll = strlen(line);
    size_t need = sl + ll + 1; // + '\n'

    // jesli brakuje miejsca, usun najstarsze linie
    while (s_len + need >= SYSLOG_CAP) {
        char* nl = strchr(s_buf, '\n');
        if (!nl) { s_len = 0; s_buf[0] = 0; break; }
        size_t cut = (nl - s_buf) + 1;
        memmove(s_buf, s_buf + cut, s_len - cut);
        s_len -= cut;
    }
    memcpy(s_buf + s_len, stamp, sl); s_len += sl;
    memcpy(s_buf + s_len, line, ll);  s_len += ll;
    s_buf[s_len++] = '\n';
    s_buf[s_len] = 0;

    // konsola
    Serial.print(stamp);
    Serial.println(line);
}

void LOG_I(const char* fmt, ...) { va_list va; va_start(va, fmt); log_write('I', fmt, va); va_end(va); }
void LOG_W(const char* fmt, ...) { va_list va; va_start(va, fmt); log_write('W', fmt, va); va_end(va); }
void LOG_E(const char* fmt, ...) { va_list va; va_start(va, fmt); log_write('E', fmt, va); va_end(va); }

String syslog_dump(bool clear) {
    if (!s_buf) return String();
    String out = String(s_buf);
    if (clear) { s_len = 0; s_buf[0] = 0; }
    return out;
}

size_t syslog_size() { return s_len; }
