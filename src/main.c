#include <pebble.h>
#include "OutlinedTextLayer.h"
#include "settings.h"

#define TICKS 40
#define FRAME_DURATION 175
#define MIN_RIBBONS 5
#define MAX_EXTRA_RIBBONS 7

#define KEY_TEMPERATURE 23
#define KEY_CONDITIONS 24

static Window *main_window;
static Layer *time_layer;
static Layer *spinny_layer;
static OutlinedTextLayer *date_layer;
static OutlinedTextLayer *weather_layer;
static char weather_layer_buffer[32];

uint8_t offset = 0;
uint8_t anim_ticks = TICKS; //Start with spinning animation
uint8_t num_ribbons = 22;
bool bluetooth = true;
bool charging = false;
static char date_buffer[12];
static AppTimer* anim_timer;

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

static GColor alterIfDisconnected(GColor c) {
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
    if(!clock_is_24h_style() && time_buffer[0] == ' ') {
        //Yuk! Spaces
        for(int i=0; i<7; i++) {
            time_buffer[i] = time_buffer[i+1];
        }
    }

    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_text_color(ctx, alterIfDisconnected(settings.bg));
    uint8_t y = PBL_IF_ROUND_ELSE(55, 49) + settings.font_big_y_offset;
    //Border
    surround_text(ctx, time_buffer, bounds.size.w, settings.font_main_big, y, 50);
    //Drop Shadow
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, settings.font_main_big, 2, y + 2);
    //draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 3, y+3);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, settings.font_main_big, 4, y + 4);
    //draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 5, y+5);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, settings.font_main_big, 6, y + 6);
    //Text
    graphics_context_set_text_color(ctx, alterIfDisconnected(charging ? GColorGreen : settings.fg));
    graphics_draw_text(ctx, time_buffer, settings.font_main_big, GRect(0, y, bounds.size.w, 50), GTextOverflowModeFill,
                       GTextAlignmentCenter,
                       NULL);
}

static void spinny_layer_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_fill_color(ctx, alterIfDisconnected(settings.bg));
    graphics_fill_rect(ctx, bounds, 0, GCornersAll);

    GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    gpath_move_to(ribbon_path, center);

    for (int i = 0; i < num_ribbons; i++) {
        graphics_context_set_fill_color(ctx, alterIfDisconnected(settings.colours[i % settings.num_colours]));
        gpath_rotate_to(ribbon_path, TRIG_MAX_ANGLE / num_ribbons / 2 * i - TRIG_MAX_ANGLE * offset / 360);
        gpath_draw_filled(ctx, ribbon_path);
    }
    graphics_context_set_fill_color(ctx, alterIfDisconnected(GColorWhite));
    graphics_fill_circle(ctx, GPoint(bounds.size.w / 2, bounds.size.h / 2), 3);
}

static void spin();
static void animation_timer_callback(void *d) {
    anim_timer = NULL;
    if (--anim_ticks) {
        spin();
    }
    offset = (offset + 5) % 180;
    layer_mark_dirty(spinny_layer);
}
static void spin() {
    if(anim_timer==NULL) {
        anim_timer = app_timer_register(FRAME_DURATION, animation_timer_callback, NULL);
    }
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
            spin();
    }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    anim_ticks = TICKS;
    spin();
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
    if(bluetooth && !connected && settings.disco_vibrate) {
        // Notify that phone has disconnected
        vibes_short_pulse();
    }
    bluetooth = connected;
    outlined_text_layer_set_colors(date_layer, alterIfDisconnected(settings.bg), alterIfDisconnected(settings.fg));
    outlined_text_layer_set_colors(weather_layer, alterIfDisconnected(settings.bg), alterIfDisconnected(settings.fg));
    layer_mark_dirty(spinny_layer);
}

static void apply_settings() {
    outlined_text_layer_set_font(date_layer, settings.font_date);
    outlined_text_layer_set_colors(date_layer, alterIfDisconnected(settings.bg), alterIfDisconnected(settings.fg));
    layer_set_hidden(outlined_text_layer_get_layer(date_layer), !settings.show_date);
    
    outlined_text_layer_set_font(weather_layer, settings.font_temp);
    outlined_text_layer_set_colors(weather_layer, alterIfDisconnected(settings.bg), alterIfDisconnected(settings.fg));
    layer_set_hidden(outlined_text_layer_get_layer(weather_layer), !settings.show_weather);
    
    anim_ticks = TICKS;
    spin();
}

static void main_window_load(Window *window) {  
    // Get information about the Window
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    time_layer = layer_create(bounds);
    layer_set_update_proc(time_layer, time_draw);

    spinny_layer = layer_create(bounds);
    ribbon_path = gpath_create(&RIBBON_PATHINFO);
    layer_set_update_proc(spinny_layer, spinny_layer_draw);

    date_layer = outlined_text_layer_create(GRect(0, bounds.size.h - PBL_IF_ROUND_ELSE(40, 35), bounds.size.w, 35));

    weather_layer = outlined_text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(23, 0), bounds.size.w, 35));
    
    apply_settings();

    layer_add_child(window_layer, spinny_layer);
    layer_add_child(window_layer, outlined_text_layer_get_layer(date_layer));
    layer_add_child(window_layer, outlined_text_layer_get_layer(weather_layer));
    layer_add_child(window_layer, time_layer);

    accel_tap_service_subscribe(tap_handler);
    battery_state_service_subscribe(battery_callback);
    // Ensure battery level is displayed from the start
    battery_callback(battery_state_service_peek());

    // Register for Bluetooth connection updates
    connection_service_subscribe((ConnectionHandlers) {
      .pebble_app_connection_handler = bluetooth_callback
    });
    bluetooth_callback(connection_service_peek_pebble_app_connection());
    
    //kick off
    time_t temp = time(NULL);
    tick_minute_handler(localtime(&temp), SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT | DAY_UNIT);
    spin();
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
    destroy_settings();
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
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Problem with colour");
    }
    return result;
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
    // Read tuples for weather data
    Tuple *temp_tuple = dict_find(iterator, KEY_TEMPERATURE);
    Tuple *conditions_tuple = dict_find(iterator, KEY_CONDITIONS);

    if (temp_tuple && conditions_tuple) {
        static char conditions_buffer[32];
        static char temp_buffer[8];
        snprintf(temp_buffer, sizeof(temp_buffer), settings.fahrenheit ? "%d°F" : "%d°C", (int) temp_tuple->value->int32);
        snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);

        // Assemble full string and display
        snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temp_buffer, conditions_buffer);
        APP_LOG(APP_LOG_LEVEL_DEBUG, weather_layer_buffer);
        outlined_text_layer_set_text(weather_layer, weather_layer_buffer);
    } else if(dict_find(iterator, KEY_BG)) {
        //perhaps settings
        update_settings(iterator);
        apply_settings();
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

static void init() {
    // Create main Window element and assign to pointer
    main_window = window_create();
    
    init_settings();

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