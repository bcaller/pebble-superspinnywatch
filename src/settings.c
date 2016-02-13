#include <pebble.h>
#include "settings.h"

#define KEY_C1 110
#define KEY_C2 120
#define KEY_C3 130
#define KEY_C4 140
#define KEY_C5 160
#define KEY_FG 150
#define DEF_BG GColorFromHEX(0x000071)
#define DEF_C1 GColorYellow
#define DEF_C2 GColorFromHEX(0xff7c11)
#define DEF_C3 GColorBrilliantRose
#define DEF_C4 GColorCyan
#define DEF_C5 GColorRajah
#define DEF_FG GColorWhite
#define DEF_N_COLOURS 4

#define KEY_DISCO_VIBRATE 201
#define KEY_DATE 202
#define KEY_WEATHER 203
#define KEY_STORAGE_OK 444
#define KEY_FONT 205
#define KEY_TEMP_UNITS_F 208
#define KEY_N_COLOURS 209
#define KEY_DESATURATE 210

#define KEY_SIZE_WEATHER 206
#define WEATHER_SIZE_TINY true
#define WEATHER_SIZE_MED false
#define KEY_SIZE_DATE 207
#define DATE_SIZE_TINY false
#define DATE_SIZE_MED true

//Sometimes persistent storage gets messed up, especially strings when installing from CloudPebble
static bool storage_ok = false;

static uint32_t custom_fonts[] = {
    PBL_IF_ROUND_ELSE(RESOURCE_ID_NEPTUN_56, RESOURCE_ID_NEPTUN_48), PBL_IF_ROUND_ELSE(RESOURCE_ID_TIMEPIECE_50, RESOURCE_ID_TIMEPIECE_43),
    PBL_IF_ROUND_ELSE(RESOURCE_ID_ALANDEN_65, RESOURCE_ID_ALANDEN_56), RESOURCE_ID_KOMIKA_BOOGIE_48
};

//Singleton. Shoot me.
Settings settings;

static GColor set_colour(DictionaryIterator *iter, int key) {
    Tuple *colour = dict_find(iter, key);
    GColor result = GColorWhite;
    if(colour) {
        int colour_value = colour->value->int32;
        result = GColorFromHEX(colour_value);
        persist_write_int(key, colour_value);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting colour to %X", colour_value);
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Problem with colour");
    }
    return result;
}

static GColor persist_read_colour(int key, GColor def) {
    int colour_value = persist_read_int(key);
    if(!storage_ok || !persist_exists(key) || colour_value < 0 || colour_value > 16777215) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Cannot set colour to %X", colour_value);
        return def;
    }
    return GColorFromHEX(colour_value);
}

static bool persist_read_bool_def(int key, bool def) {
    if(!storage_ok || !persist_exists(key))
        return def;
    return persist_read_bool(key);
}

static int persist_read_int_def(int key, int def, int min, int max) {
    if(!storage_ok || !persist_exists(key))
        return def;
    int val = persist_read_int(key);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Val %d", val);
    if(val < min) return min;
    if(val > max) return max;
    return val;
}

static int8_t font_offset(int id) {
    if(id == 3) return 10;
    #ifdef PBL_ROUND
    if(id==1) return -5;
    if(id==2) return -10;
    #endif
    return 0;
}

void update_settings(DictionaryIterator *iterator) {
    //perhaps settings
    storage_ok = true;
    persist_write_int(KEY_STORAGE_OK, KEY_STORAGE_OK);

    settings.bg = set_colour(iterator, KEY_BG);
    settings.colours[0] = set_colour(iterator, KEY_C1);
    settings.colours[1] = set_colour(iterator, KEY_C2);
    settings.colours[2] = set_colour(iterator, KEY_C3);
    settings.colours[3] = set_colour(iterator, KEY_C4);
    settings.colours[4] = set_colour(iterator, KEY_C5);
    settings.fg = set_colour(iterator, KEY_FG);
    
    fonts_unload_custom_font(settings.font_main_big);
    int font_id = dict_find(iterator, KEY_FONT)->value->int8;
    settings.font_main_big = fonts_load_custom_font(resource_get_handle(custom_fonts[font_id]));
    persist_write_int(KEY_FONT, font_id);
    settings.font_big_y_offset = font_offset(font_id);

    bool size = dict_find(iterator, KEY_SIZE_DATE)->value->int8 == 1;
    persist_write_bool(KEY_SIZE_DATE, size);
    settings.font_date = fonts_get_system_font(size ? FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_24_BOLD);
    
    size = dict_find(iterator, KEY_SIZE_WEATHER)->value->int8 == 1;
    persist_write_bool(KEY_SIZE_WEATHER, size);
    settings.font_temp = fonts_get_system_font(size ? FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_14);

    settings.disco_vibrate = dict_find(iterator, KEY_DISCO_VIBRATE)->value->int8 == 1;
    persist_write_bool(KEY_DISCO_VIBRATE, settings.disco_vibrate);
    
    settings.disco_desaturate = dict_find(iterator, KEY_DESATURATE)->value->int8 == 1;
    persist_write_bool(KEY_DESATURATE, settings.disco_desaturate);

    settings.show_weather = dict_find(iterator, KEY_WEATHER)->value->int8 == 1;
    persist_write_bool(KEY_WEATHER, settings.show_weather);

    settings.show_date = dict_find(iterator, KEY_DATE)->value->int8 == 1;
    persist_write_bool(KEY_DATE, settings.show_date);
    
    settings.fahrenheit = dict_find(iterator, KEY_TEMP_UNITS_F)->value->int8 == 1;
    persist_write_bool(KEY_TEMP_UNITS_F, settings.fahrenheit);
    
    settings.num_colours = dict_find(iterator, KEY_N_COLOURS)->value->int8;
    persist_write_int(KEY_N_COLOURS, settings.num_colours);
}

void init_settings() {
    int font_id = 0;
    if(persist_exists(KEY_STORAGE_OK) && persist_read_int(KEY_STORAGE_OK) == KEY_STORAGE_OK) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading from storage");
        storage_ok = true; //else storage empty or corrupt
        font_id = persist_read_int(KEY_FONT);
    } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "No stored settings");
    }
    settings = (Settings) {
        .bg = persist_read_colour(KEY_BG, DEF_BG),
        .fg = persist_read_colour(KEY_FG, DEF_FG),
        //Settings
        .disco_vibrate = persist_read_bool_def(KEY_DISCO_VIBRATE, true),
        .disco_desaturate = persist_read_bool_def(KEY_DESATURATE, true),
        .show_weather = persist_read_bool_def(KEY_WEATHER, true),
        .show_date = persist_read_bool_def(KEY_DATE, true),
        .fahrenheit = persist_read_bool_def(KEY_TEMP_UNITS_F, false),
        //Fonts
        .font_main_big = fonts_load_custom_font(resource_get_handle(custom_fonts[font_id])),
        .font_big_y_offset = font_offset(font_id),
        .font_date = fonts_get_system_font(persist_read_bool_def(KEY_SIZE_DATE, false) ?
                                           FONT_KEY_GOTHIC_14 : FONT_KEY_GOTHIC_24_BOLD),
        .font_temp = fonts_get_system_font(persist_read_bool_def(KEY_SIZE_WEATHER, false) ?
                                           FONT_KEY_GOTHIC_24_BOLD : FONT_KEY_GOTHIC_14),
        .num_colours = persist_read_int_def(KEY_N_COLOURS, DEF_N_COLOURS, 3, 5)
    };
    //Ribbon Colours
    settings.colours[0] = persist_read_colour(KEY_C1, DEF_C1);
    settings.colours[1] = persist_read_colour(KEY_C2, DEF_C2);
    settings.colours[2] = persist_read_colour(KEY_C3, DEF_C3);
    settings.colours[3] = persist_read_colour(KEY_C4, DEF_C4);
    settings.colours[4] = persist_read_colour(KEY_C5, DEF_C5);
}

void destroy_settings() {
    fonts_unload_custom_font(settings.font_main_big);
}