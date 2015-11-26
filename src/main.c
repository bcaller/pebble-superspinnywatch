#include <pebble.h>

static Window *main_window;
static Layer *time_layer;
static Layer *spinny_layer;
static Layer *date_layer;
static GFont *f;
static GFont font_main_big;
static GFont font_date_small;

int32_t offset = 0;
int anim_ticks = 0;

GColor bg;

static const GPathInfo RIBBON_PATHINFO = {
        4,
        (GPoint[]) {{3,   0},
                    {-3,  0},
                    {-10, -120},
                    {10,  -120}}
};
static GPath *ribbon_path;

void draw_bordered(GContext *ctx, const char *s_buffer, int16_t w, int16_t h, FontInfo *fo, int16_t x, int16_t y) {
    graphics_draw_text(ctx, s_buffer, fo, GRect(x, y, w, h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

void surround_text(GContext *ctx, const char *s_buffer, int w, const FontInfo *fo, int16_t y, int h) {
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
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Write the current hours and minutes into a buffer
    static char time_buffer[8];
    strftime(time_buffer, sizeof(time_buffer), clock_is_24h_style() ?
                                         "%H:%M" : "%I:%M", tick_time);

    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_text_color(ctx, bg);
    int16_t y = PBL_IF_ROUND_ELSE(55, 49);
    //Border
    surround_text(ctx, time_buffer, bounds.size.w, font_main_big, y, 50);
    //Drop Shadow
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 2, y+2);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 3, y+3);
    draw_bordered(ctx, time_buffer, bounds.size.w, 50, font_main_big, 4, y+4);
    //Text
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, time_buffer, font_main_big, GRect(0, y, bounds.size.w, 50), GTextOverflowModeFill, GTextAlignmentCenter,
                       NULL);
}

static void date_draw(Layer *layer, GContext *ctx) {
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);
    // Copy date into buffer from tm structure
    static char date_buffer[16];
    strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
    GRect bounds = layer_get_bounds(layer);
    graphics_context_set_text_color(ctx, GColorWhite);
    surround_text(ctx, date_buffer, bounds.size.w, font_date_small, 0, 30);
    graphics_context_set_text_color(ctx, bg);
    graphics_draw_text(ctx, date_buffer, font_main_big, GRect(0, 0, bounds.size.w, 30), GTextOverflowModeFill, GTextAlignmentCenter,
                       NULL);
}

static void spinny_layer_draw(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);

    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, bounds, 0, GCornersAll);


    ribbon_path = gpath_create(&RIBBON_PATHINFO);

    GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    gpath_move_to(ribbon_path, center);

    int num = 22;
    GColor colours[4] = {GColorYellow, GColorFromHEX(0xff7c11), GColorBrilliantRose, GColorCyan};
    //Draw each ribbon
    for (int i = 0; i < num; i++) {
        graphics_context_set_fill_color(ctx, colours[i % 4]);
        gpath_rotate_to(ribbon_path, TRIG_MAX_ANGLE / num * i + TRIG_MAX_ANGLE / 360 * offset);
        gpath_draw_filled(ctx, ribbon_path);
    }
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, GPoint(bounds.size.w / 2, bounds.size.h / 2), 5);
}

static void animation_timer_callback(void *d) {
    if (--anim_ticks) {
        app_timer_register(200, animation_timer_callback, NULL);
    }
    offset = (offset + 5) % 360;
    layer_mark_dirty(spinny_layer);
}

static void tick_minute_handler(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(time_layer);

    if (!anim_ticks) {
        anim_ticks = 20;
        app_timer_register(200, animation_timer_callback, NULL);
    }
}

static void tick_day_handler(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(date_layer);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    if (!anim_ticks) {
        app_timer_register(200, animation_timer_callback, NULL);
    }
    anim_ticks = 20;
}

static void main_window_load(Window *window) {
    font_main_big = fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
    font_date_small = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
    // Get information about the Window
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    // Create the TextLayer with specific bounds
    time_layer = layer_create(bounds);

    // Add it as a child layer to the Window's root layer

    spinny_layer = layer_create(bounds);
    layer_set_update_proc(spinny_layer, spinny_layer_draw);
    layer_add_child(window_layer, spinny_layer);
    date_layer = layer_create(GRect(bounds.size.h - 30, 0, bounds.size.w, 30));

    layer_set_update_proc(time_layer, time_draw);
    layer_set_update_proc(date_layer, date_draw);
    layer_add_child(window_layer, time_layer);

    accel_tap_service_subscribe(tap_handler);
}

static void main_window_unload(Window *w) {
    // Destroy TextLayer
    layer_destroy(time_layer);
    layer_destroy(spinny_layer);
    layer_destroy(date_layer);
    gpath_destroy(ribbon_path);
}


static void deinit() {
    // Destroy Window
    window_destroy(main_window);
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
    tick_timer_service_subscribe(DAY_UNIT, tick_day_handler);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}