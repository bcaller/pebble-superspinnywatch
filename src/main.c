#include <pebble.h>
#include "OutlinedTextLayer.h"

#define TICKS 40
#define FRAME_DURATION 175
#define MIN_RIBBONS 5
#define MAX_EXTRA_RIBBONS 7

#define KEY_TEMPERATURE 23
#define KEY_CONDITIONS 24
#define KEY_LOC 18

#define KEY_BG 100
#define KEY_C1 110
#define KEY_C2 120
#define KEY_C3 130
#define KEY_C4 140
#define KEY_FG 150
#define DEF_BG GColorFromHEX(0x000071)
#define DEF_C1 GColorYellow
#define DEF_C2 GColorFromHEX(0xff7c11)
#define DEF_C3 GColorBrilliantRose
#define DEF_C4 GColorCyan
#define DEF_FG GColorWhite

#define KEY_DISCO_VIBRATE 201
bool disco_vibrate;
#define KEY_DATE 202
#define KEY_WEATHER 203
bool storage_ok = false;
#define KEY_STORAGE_OK 444
#define KEY_FONT 205

static Window *main_window;
static Layer *time_layer;
static Layer *spinny_layer;
static OutlinedTextLayer *date_layer;
static OutlinedTextLayer *weather_layer;
static GFont font_main_big;
static int8_t font_big_y_offset;
static GFont font_date_small;
static GFont font_temp_vsmall;
static char weather_layer_buffer[32];
static GColor colours[4];

uint8_t offset = 0;
uint8_t anim_ticks = TICKS; //Start with spinning animation
uint8_t num_ribbons = 22;
bool bluetooth = true;
bool charging = false;
static char date_buffer[12];

static GColor bg;
static GColor fg;

static const GPathInfo RIBBON_PATHINFO = {
        10,
        (GPoint[]) {{-3,  0},
                    {-4, -35},
                    {-10, -120},
                    {10,  -120},
                    {4, -35},
                    {3,   0},
                    {4, 35},
                    {10,  120},
                    {-10, 120},
                    {-4, 35}}
};
static GPath *ribbon_path;

static uint32_t custom_fonts[] = {
    RESOURCE_ID_NEPTUN_48, RESOURCE_ID_TIMEPIECE_43, RESOURCE_ID_ALANDEN_56, RESOURCE_ID_KOMIKA_BOOGIE_48
};

void draw_bordered(GContext *ctx, const char *s_buffer, uint8_t w, uint8_t h, FontInfo *fo, int16_t x, int16_t y) {
    graphics_draw_text(ctx, s_buffer, fo, GRect(x, y, w, h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static GColor invert(GColor c) {
    return (GColor8) {
            .a = c.a,
            .r = 0b11 ^ c.r,
            .g = 0b11 ^ c.g,
            .b = 0b11 ^ c.b
    };
}

static GColor desaturate(GColor c) {
    return (GColor8) {
            .a = c.a,
            .r = (c.r + c.g + c.b) / 3,
            .g = (c.r + c.g + c.b) / 3,
            .b = (c.r + c.g + c.b) / 3
    };
}

static GColor invertIfDisconnected(GColor c) {
    return bluetooth ? c : desaturate(c);
}

void surround_text(GContext *ctx, const char *s_buffer, uint8_t w, FontInfo *fo, int16_t y, uint8_t h) {
    draw_bordered(ctx, s_buffer, w, h, fo, 2, y);
    draw_bordered(ctx, s_buffer, w, h, fo, -2, y);
    draw_bordered(ctx, s_buffer, w, h, fo, 0, y - 2);
    draw_bordered(ctx, s_buffer, w, h, fo, 0, y + 2);
    draw_bordered(ctx, s_buffer, w, h, fo, -1, y + 1);
    draw_bordered(ctx, s_buffer, w, h, fo, -1, y - 1);
    draw_bordered(ctx, s_buffer, w, h, fo, 1, y - 1);
    draw_bordered(ctx, s_buffer, w, h, fo, 1, y + 1);
}

static void time_draw(Layer *layer, GContext *ctx) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Time draw");
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Write the current hours and minutes into a buffer
    static char time_buffer[8];
    strftime(time_buffer, sizeof(time_buffer), clock_is_24h_style() ?
                                               "%H:%M" : "%l:%M", tick_time);

    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_text_color(ctx, invertIfDisconnected(bg));
    uint8_t y = PBL_IF_ROUND_ELSE(55, 49) + font_big_y_offset;
    //Border
    surround_text(ctx, time_buffer, bounds.size.w, font_main_big, y, 50);
    //Drop Shadow
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 2, y + 2);
    //draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 3, y+3);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 4, y + 4);
    //draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 5, y+5);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 6, y + 6);
    //Text
    graphics_context_set_text_color(ctx, invertIfDisconnected(charging ? GColorGreen : fg));
    graphics_draw_text(ctx, time_buffer, font_main_big, GRect(0, y, bounds.size.w, 50), GTextOverflowModeFill,
                       GTextAlignmentCenter,
                       NULL);
}

static void spinny_layer_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_fill_color(ctx, invertIfDisconnected(bg));
    graphics_fill_rect(ctx, bounds, 0, GCornersAll);

    GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    gpath_move_to(ribbon_path, center);

    for (uint8_t i = 0; i < num_ribbons; i++) {
        graphics_context_set_fill_color(ctx, invertIfDisconnected(colours[i % 4]));
        gpath_rotate_to(ribbon_path, TRIG_MAX_ANGLE / num_ribbons / 2 * i - TRIG_MAX_ANGLE * offset / 360);
        gpath_draw_filled(ctx, ribbon_path);
    }
    graphics_context_set_fill_color(ctx, invertIfDisconnected(GColorWhite));
    graphics_fill_circle(ctx, GPoint(bounds.size.w / 2, bounds.size.h / 2), 3);
}

static void animation_timer_callback(void *d) {
    if (--anim_ticks) {
        app_timer_register(FRAME_DURATION, animation_timer_callback, NULL);
    }
    offset = (offset + 5) % 180;
    layer_mark_dirty(spinny_layer);
}

static void tick_minute_handler(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(time_layer);
    if(units_changed & DAY_UNIT) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "New day");
        strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
        outlined_text_layer_set_text(date_layer, date_buffer);
    }
    
    bool batteryOK = num_ribbons > MIN_RIBBONS + MAX_EXTRA_RIBBONS / 2;
    bool newHour = units_changed & HOUR_UNIT;

    if (!anim_ticks) {
        anim_ticks = tick_time->tm_min ? TICKS / 4 : TICKS * 3 / 4; // Massive hour
        if(newHour || batteryOK) //Don't do every minute if battery is low
            app_timer_register(FRAME_DURATION, animation_timer_callback, NULL);
    }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (!anim_ticks) {
        app_timer_register(FRAME_DURATION, animation_timer_callback, NULL);
    }
    anim_ticks = TICKS;
}

static void battery_callback(BatteryChargeState state) {
    // Record the new battery level
    int r = state.charge_percent * MAX_EXTRA_RIBBONS;
    num_ribbons = r / 100 + MIN_RIBBONS;
    if (r % 100) {
        num_ribbons++;
    }
    charging = state.is_charging;
    layer_mark_dirty(spinny_layer);
}

static void bluetooth_callback(bool connected) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, connected ? "bluetooth" : "no bluetooth");
    if(bluetooth && !connected && disco_vibrate) {
        // Notify that phone has disconnected
        vibes_short_pulse();
    }
    bluetooth = connected;
    outlined_text_layer_set_colors(date_layer, invertIfDisconnected(bg), invertIfDisconnected(fg));
    outlined_text_layer_set_colors(weather_layer, invertIfDisconnected(bg), invertIfDisconnected(fg));
    layer_mark_dirty(spinny_layer);
}

static bool persist_read_bool_def_true(int key) {
    if(!storage_ok || !persist_exists(key))
        return true;
    return persist_read_bool(key);
}

static void main_window_load(Window *window) {
    int font_id = persist_read_int(KEY_FONT);
    font_main_big = fonts_load_custom_font(resource_get_handle(custom_fonts[font_id]));
    font_big_y_offset = font_id == 3 ? 10 : 0;
    font_date_small = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    font_temp_vsmall = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    // Get information about the Window
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    time_layer = layer_create(bounds);
    layer_set_update_proc(time_layer, time_draw);

    spinny_layer = layer_create(bounds);
    ribbon_path = gpath_create(&RIBBON_PATHINFO);
    layer_set_update_proc(spinny_layer, spinny_layer_draw);

    date_layer = outlined_text_layer_create(GRect(0, bounds.size.h - 35, bounds.size.w, 35));
    outlined_text_layer_set_font(date_layer, font_date_small);
    outlined_text_layer_set_colors(date_layer, invertIfDisconnected(bg), invertIfDisconnected(fg));
    layer_set_hidden(outlined_text_layer_get_layer(date_layer), !persist_read_bool_def_true(KEY_DATE));

    weather_layer = outlined_text_layer_create(GRect(0, 0, bounds.size.w, 35));
    outlined_text_layer_set_font(weather_layer, font_temp_vsmall);
    outlined_text_layer_set_colors(weather_layer, invertIfDisconnected(bg), invertIfDisconnected(fg));
    layer_set_hidden(outlined_text_layer_get_layer(weather_layer), !persist_read_bool_def_true(KEY_WEATHER));

    layer_add_child(window_layer, spinny_layer);
    layer_add_child(window_layer, outlined_text_layer_get_layer(date_layer));
    layer_add_child(window_layer, outlined_text_layer_get_layer(weather_layer));
    layer_add_child(window_layer, time_layer);

    accel_tap_service_subscribe(tap_handler);
    battery_state_service_subscribe(battery_callback);
    // Ensure battery level is displayed from the start
    battery_callback(battery_state_service_peek());

    // Register for Bluetooth connection updates
#ifdef PBL_SDK_2
    bluetooth_connection_service_subscribe(bluetooth_callback);
#elif PBL_SDK_3
    connection_service_subscribe((ConnectionHandlers) {
      .pebble_app_connection_handler = bluetooth_callback
    });
#endif
#ifdef PBL_SDK_2
    bluetooth_callback(bluetooth_connection_service_peek());
#elif PBL_SDK_3
    bluetooth_callback(connection_service_peek_pebble_app_connection());
#endif
    
    //kick off
    time_t temp = time(NULL);
    tick_minute_handler(localtime(&temp), SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT);
    animation_timer_callback(NULL);
}

static void main_window_unload(Window *w) {
    layer_destroy(time_layer);
    layer_destroy(spinny_layer);
    outlined_text_layer_destroy(date_layer);
    outlined_text_layer_destroy(weather_layer);
    gpath_destroy(ribbon_path);
    connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    accel_tap_service_unsubscribe();
    tick_timer_service_unsubscribe();
    fonts_unload_custom_font(font_main_big);
}


static void deinit() {
    app_message_deregister_callbacks();
    window_destroy(main_window);
}

static GColor set_colour(DictionaryIterator *iter, int key) {
    Tuple *colour = dict_find(iter, key);
    GColor result = GColorWhite;
    if(colour) {
        int colour_value = colour->value->int32;
        result = GColorFromHEX(colour_value);
        persist_write_int(key, colour_value);
        APP_LOG(APP_LOG_LEVEL_ERROR, "Setting colour to %X", colour_value);
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Problem with colour");
    }
    return result;
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Store incoming information
    static char conditions_buffer[32];
    static char temp_buffer[32];

    // Read tuples for data
    Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
    Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);
    //Tuple *loc_tuple = dict_find(iterator, KEY_LOC);

    // If all data is available, use it
    if (temp_tuple && conditions_tuple) {
        snprintf(temp_buffer, sizeof(temp_buffer), "%d°C", (int) temp_tuple->value->int32);
        snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
        //snprintf(loc_buffer, sizeof(loc_buffer), "%s", loc_tuple->value->cstring);

        // Assemble full string and display
        snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temp_buffer, conditions_buffer);
        APP_LOG(APP_LOG_LEVEL_DEBUG, weather_layer_buffer);
        outlined_text_layer_set_text(weather_layer, weather_layer_buffer);
    } else if(dict_find(iterator, KEY_BG)) {
        //perhaps settings
        storage_ok = true;
        persist_write_int(KEY_STORAGE_OK, KEY_STORAGE_OK);
        
        bg = set_colour(iterator, KEY_BG);
        colours[0] = set_colour(iterator, KEY_C1);
        colours[1] = set_colour(iterator, KEY_C2);
        colours[2] = set_colour(iterator, KEY_C3);
        colours[3] = set_colour(iterator, KEY_C4);
        fg = set_colour(iterator, KEY_FG);
        outlined_text_layer_set_colors(date_layer, invertIfDisconnected(bg), invertIfDisconnected(fg));
        outlined_text_layer_set_colors(weather_layer, invertIfDisconnected(bg), invertIfDisconnected(fg));
        
        fonts_unload_custom_font(font_main_big);
        int font_id = dict_find(iterator, KEY_FONT)->value->int8;
        font_main_big = fonts_load_custom_font(resource_get_handle(custom_fonts[font_id]));
        persist_write_int(KEY_FONT, font_id);
        font_big_y_offset = font_id == 3 ? 10 : 0;
        
        disco_vibrate = dict_find(iterator, KEY_DISCO_VIBRATE)->value->int8 == 1;
        persist_write_bool(KEY_DISCO_VIBRATE, disco_vibrate);
        
        bool show_weather = dict_find(iterator, KEY_WEATHER)->value->int8 == 1;
        layer_set_hidden(outlined_text_layer_get_layer(weather_layer), !show_weather);
        persist_write_bool(KEY_DISCO_VIBRATE, show_weather);
        
        bool show_date = dict_find(iterator, KEY_DATE)->value->int8 == 1;
        layer_set_hidden(outlined_text_layer_get_layer(date_layer), !show_date);
        persist_write_bool(KEY_DATE, show_date);
        
        layer_mark_dirty(time_layer);
        layer_mark_dirty(spinny_layer);
    }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static GColor persist_read_colour(int key, GColor def) {
    int colour_value = persist_read_int(key);
    if(!storage_ok || !persist_exists(key) || colour_value < 0 || colour_value > 16777215)
        return def;
    return GColorFromHEX(colour_value);
}

static void init() {
    if(persist_exists(KEY_STORAGE_OK) && persist_read_int(KEY_STORAGE_OK) == KEY_STORAGE_OK) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading from storage");
        storage_ok = true; //else storage empty or corrupt
    } else {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "No stored settings");
    }
    bg = persist_read_colour(KEY_BG, DEF_BG);
    colours[0] = persist_read_colour(KEY_C1, DEF_C1);
    colours[1] = persist_read_colour(KEY_C2, DEF_C2);
    colours[2] = persist_read_colour(KEY_C3, DEF_C3);
    colours[3] = persist_read_colour(KEY_C4, DEF_C4);
    fg = persist_read_colour(KEY_FG, DEF_FG);
    
    disco_vibrate = persist_read_bool_def_true(KEY_DISCO_VIBRATE);
    // Create main Window element and assign to pointer
    main_window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(main_window, (WindowHandlers) {
            .load = main_window_load,
            .unload = main_window_unload
    });

    // Show the Window on the watch, with animated=true
    window_stack_push(main_window, true);

    // Register with TickTimerService
    tick_timer_service_subscribe(MINUTE_UNIT, tick_minute_handler);

    // Register callbacks
    app_message_register_inbox_received(inbox_received_callback);
    app_message_register_inbox_dropped(inbox_dropped_callback);
    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    // Open AppMessage
    app_message_open(2048, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}