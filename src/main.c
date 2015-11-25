#include <pebble.h>

static Window *s_main_window;
static Layer *s_time_layer;
static Layer *super_layer;
static GFont *f;

int32_t offset = 0;
int timer_scheduled = 0;

GColor bg;

static const GPathInfo NEEDLE_NORTH_POINTS = { 
  4,
  (GPoint[]) { { 3, 0 }, { -3, 0 }, { -10, -120 }, { 10, -120 } }
};
static GPath *s_needle_north;

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  layer_mark_dirty(s_time_layer);
}

static void super_layer_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // Draw a black filled rectangle with sharp corners
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornersAll);

  // Draw a white filled circle a radius of half the layer height
  
  s_needle_north = gpath_create(&NEEDLE_NORTH_POINTS);
  // Move the needles to the center of the screen.
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  gpath_move_to(s_needle_north, center);
  int num = 22;
  GColor colours[4] = {GColorYellow, GColorFromHEX(0xff7c11), GColorBrilliantRose, GColorCyan};
  for(int i = 0; i < num; i++) {
    graphics_context_set_fill_color(ctx, colours[i%4]);
    gpath_rotate_to(s_needle_north, TRIG_MAX_ANGLE / num * i + TRIG_MAX_ANGLE / 360 * offset);
    gpath_draw_filled(ctx, s_needle_north);
  }
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(bounds.size.w / 2, bounds.size.h / 2), 5);
}

static void time_draw(Layer *layer, GContext *ctx){
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);
  
  GRect bounds = layer_get_bounds(layer); 
  
  graphics_context_set_text_color(ctx, bg);
  GFont fo = fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49);
  int y = PBL_IF_ROUND_ELSE(55, 49);
  graphics_draw_text(ctx, s_buffer, fo, GRect(2, y, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(-2, y, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(0, y+2, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(0, y-2, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(-1, y+1, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(1, y-1, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(-1, y-1, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(1, y+1, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  
  graphics_draw_text(ctx, s_buffer, fo, GRect(2, y+2, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(3, y+3, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_buffer, fo, GRect(4, y+4, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_buffer, fo, GRect(0, y, bounds.size.w,50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void anim_timer_callback(void* data) {
  offset = (offset + 5) % 360;
  layer_mark_dirty(super_layer);
  if(--timer_scheduled) {
    app_timer_register(200, anim_timer_callback, NULL);
  } else {
    timer_scheduled = false;
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // Copy date into buffer from tm structure
  static char date_buffer[16];
  strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
  
  layer_mark_dirty(s_time_layer);
  
  if(!timer_scheduled) {
    timer_scheduled = 20;
    app_timer_register(200, anim_timer_callback, NULL);
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if(!timer_scheduled) {
    app_timer_register(200, anim_timer_callback, NULL);
  }
  timer_scheduled = 20;
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer); 
  
  // Create the TextLayer with specific bounds
  s_time_layer = layer_create(bounds);

  // Add it as a child layer to the Window's root layer
  
  
  //super_layer = layer_create(GRect(10, 30, 50, 50));
  super_layer = layer_create(bounds);
  layer_set_update_proc(super_layer, super_layer_draw);
  layer_add_child(window_layer, super_layer);
  
  layer_set_update_proc(s_time_layer, time_draw);
  layer_add_child(window_layer, s_time_layer);
  
  accel_tap_service_subscribe(tap_handler);
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  layer_destroy(s_time_layer);
  layer_destroy(super_layer);
  gpath_destroy(s_needle_north);
}


static void init() {
  bg = GColorFromHEX(0x000071);
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time();

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}