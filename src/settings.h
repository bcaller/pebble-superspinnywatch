#pragma once
#include <pebble.h>

#define KEY_BG 100

typedef struct Settings {
    bool disco_vibrate;
    bool storage_ok;
    bool show_weather;
    bool show_date;
    bool fahrenheit;
    int8_t font_big_y_offset;
    int8_t num_colours;
    GColor bg;
    GColor fg;
    GFont font_main_big;
    GFont font_date;
    GFont font_temp;
    GColor colours[5];
} Settings;

extern Settings settings;

void init_settings();
void update_settings(DictionaryIterator* iterator);
void destroy_settings();