#include <pebble.h>

#define TICKS 40
#define FRAME_DURATION 175
#define MIN_RIBBONS 5
#define MAX_EXTRA_RIBBONS 7

#define KEY_TEMPERATURE 23
#define KEY_CONDITIONS 24
#define KEY_LOC 18

static Window *main_window;
static Layer *time_layer;
static Layer *spinny_layer;
static Layer *date_layer;
static Layer *weather_layer;
static GFont font_main_big;
static GFont font_date_small;
static GFont font_temp_vsmall;
static char weather_layer_buffer[32];
static GColor colours[4];
static AppTimer *timer = NULL;

uint8_t offset = 0;
uint8_t anim_ticks = TICKS; //Start with spinning animation
uint8_t num_ribbons = 22;
bool bluetooth = true;
bool charging = false;

GColor bg;

static const GPathInfo RIBBON_PATHINFO = {
        6,
        (GPoint[]) {{-10, -120},
                    {10,  -120},
                    {3,   0},
                    {10, 120},
                    {-10, 120},
                    {-3,  0}}
};
static GPath *ribbon_path;

void draw_bordered(GContext *ctx, const char *s_buffer, uint8_t w, uint8_t h, FontInfo *fo, uint8_t x, uint8_t y) {
    graphics_draw_text(ctx, s_buffer, fo, GRect(x, y, w, h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static GColor invert(GColor c) {
   return (GColor8){ 
     .a = c.a,
     .r = 0b11 ^ c.r, 
     .g = 0b11 ^ c.g, 
     .b = 0b11 ^ c.b 
   };
}

static GColor invertIfDisconnected(GColor c) {
   return bluetooth ? c : invert(c);
}

void surround_text(GContext *ctx, const char *s_buffer, uint8_t w, FontInfo *fo, uint8_t y, uint8_t h) {
    draw_bordered(ctx, s_buffer, w, h, fo, 2, y);
    draw_bordered(ctx, s_buffer, w, h, fo, -2, y);
    draw_bordered(ctx, s_buffer, w, h, fo, 0, y-2);
    draw_bordered(ctx, s_buffer, w, h, fo, 0, y+2);
    draw_bordered(ctx, s_buffer, w, h, fo, -1, y+1);
    draw_bordered(ctx, s_buffer, w, h, fo, -1, y-1);
    draw_bordered(ctx, s_buffer, w, h, fo, 1, y-1);
    draw_bordered(ctx, s_buffer, w, h, fo, 1, y+1);
}

static void time_draw(Layer *layer, GContext *ctx) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "Time draw");
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Write the current hours and minutes into a buffer
    static char time_buffer[8];
    strftime(time_buffer, sizeof(time_buffer), clock_is_24h_style() ?
                                         "%H:%M" : "%I:%M", tick_time);

    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_text_color(ctx, invertIfDisconnected(bg));
    uint8_t y = PBL_IF_ROUND_ELSE(55, 49);
    //Border
    surround_text(ctx, time_buffer, bounds.size.w, font_main_big, y, 50);
    //Drop Shadow
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 2, y+2);
    //draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 3, y+3);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 4, y+4);
    //draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 5, y+5);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 6, y+6);
    //Text
    graphics_context_set_text_color(ctx, invertIfDisconnected(charging ? GColorGreen : GColorWhite));
    graphics_draw_text(ctx, time_buffer, font_main_big, GRect(0, y, bounds.size.w, 50), GTextOverflowModeFill, GTextAlignmentCenter,
                       NULL);
}

static void date_draw(Layer *layer, GContext *ctx) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "Date draw");
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    // Copy date into buffer from tm structure
    static char date_buffer[16];
    strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
    GRect bounds = layer_get_bounds(layer);
   graphics_context_set_text_color(ctx, invertIfDisconnected(bg));
    surround_text(ctx, date_buffer, bounds.size.w, font_date_small, 0, 30);
       graphics_context_set_text_color(ctx, invertIfDisconnected(GColorWhite));
    graphics_draw_text(ctx, date_buffer, font_date_small, GRect(0, 0, bounds.size.w, 30), GTextOverflowModeFill, GTextAlignmentCenter,
                       NULL);
}

static void weather_draw(Layer *layer, GContext *ctx) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "Weather draw");
   GRect bounds = layer_get_bounds(layer);
   graphics_context_set_text_color(ctx, invertIfDisconnected(bg));
   surround_text(ctx, weather_layer_buffer, bounds.size.w, font_temp_vsmall, 0, 30);
   graphics_context_set_text_color(ctx, invertIfDisconnected(GColorWhite));
   graphics_draw_text(ctx, weather_layer_buffer, font_temp_vsmall, GRect(0, 0, bounds.size.w, 30), GTextOverflowModeFill, GTextAlignmentCenter,
                       NULL);
}

static void spinny_layer_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_fill_color(ctx, invertIfDisconnected(bg));
    graphics_fill_rect(ctx, bounds, 0, GCornersAll);

    ribbon_path = gpath_create(&RIBBON_PATHINFO);

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
        app_timer_reschedule(timer, FRAME_DURATION);
    }
    offset = (offset + 5) % 180;
    layer_mark_dirty(spinny_layer);
}

static void tick_minute_handler(struct tm *tick_time, TimeUnits units_changed) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "Minute tick");
    layer_mark_dirty(time_layer);
    layer_mark_dirty(date_layer);

    if (!anim_ticks) {
        anim_ticks = tick_time->tm_min ? TICKS / 4 : TICKS * 3 / 4; // Massive hour
        app_timer_reschedule(timer, FRAME_DURATION);
    }
    //Update weather
   if(tick_time->tm_min % 30 == 0) {
     // Begin dictionary
     DictionaryIterator *iter;
     app_message_outbox_begin(&iter);
   
     // Add a key-value pair
     dict_write_uint8(iter, 0, 0);
   
     // Send the message!
     app_message_outbox_send();
   }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (!anim_ticks) {
        app_timer_reschedule(timer, FRAME_DURATION);
    }
    anim_ticks = TICKS;
}

static void battery_callback(BatteryChargeState state) {
  // Record the new battery level
   int r = state.charge_percent * MAX_EXTRA_RIBBONS;
   num_ribbons = r / 100 + MIN_RIBBONS;
   if(r % 100) {
      num_ribbons++;
   }
   charging = state.is_charging;
   layer_mark_dirty(spinny_layer);
}

static void bluetooth_callback(bool connected) {
   APP_LOG(APP_LOG_LEVEL_DEBUG, "bluetooth");
   bluetooth = connected;
   layer_mark_dirty(spinny_layer);
}

static void main_window_load(Window *window) {
    font_main_big = fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
   //font_main_big = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_COOL_46)); 
   font_date_small = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
   font_temp_vsmall = fonts_get_system_font(FONT_KEY_GOTHIC_14);
    colours[0] = GColorYellow;
    colours[1] = GColorFromHEX(0xff7c11);
    colours[2] = GColorBrilliantRose;
    colours[3] = GColorCyan;
    // Get information about the Window
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    time_layer = layer_create(bounds);
    layer_set_update_proc(time_layer, time_draw);
    
    spinny_layer = layer_create(bounds);
    layer_set_update_proc(spinny_layer, spinny_layer_draw);
    
    date_layer = layer_create(GRect(0, bounds.size.h-35, bounds.size.w, 35));
    layer_set_update_proc(date_layer, date_draw);
    
    weather_layer = layer_create(GRect(0, 0, bounds.size.w, 35));
    layer_set_update_proc(weather_layer, weather_draw);
    
    layer_add_child(window_layer, spinny_layer);
    layer_add_child(window_layer, date_layer);
    layer_add_child(window_layer, weather_layer);
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
   app_timer_register(10, animation_timer_callback, NULL);
}

static void main_window_unload(Window *w) {
    layer_destroy(time_layer);
    layer_destroy(spinny_layer);
    layer_destroy(date_layer);
    layer_destroy(weather_layer);
    gpath_destroy(ribbon_path);
    connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    accel_tap_service_unsubscribe();
    tick_timer_service_unsubscribe();
}


static void deinit() {
    app_message_deregister_callbacks();
    window_destroy(main_window);
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
  if(temp_tuple && conditions_tuple) {
    snprintf(temp_buffer, sizeof(temp_buffer), "%d°C", (int)temp_tuple->value->int32);
    snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
     //snprintf(loc_buffer, sizeof(loc_buffer), "%s", loc_tuple->value->cstring);

    // Assemble full string and display
    snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temp_buffer, conditions_buffer);
     APP_LOG(APP_LOG_LEVEL_DEBUG, weather_layer_buffer);
     layer_mark_dirty(weather_layer);
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
    bg = GColorFromHEX(0x000071);
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
   
   time_t temp = time(NULL);
   tick_minute_handler(localtime(&temp), MINUTE_UNIT);
   
   // Register callbacks
   app_message_register_inbox_received(inbox_received_callback);
   app_message_register_inbox_dropped(inbox_dropped_callback);
   app_message_register_outbox_failed(outbox_failed_callback);
   app_message_register_outbox_sent(outbox_sent_callback);
   // Open AppMessage
   app_message_open(128, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}