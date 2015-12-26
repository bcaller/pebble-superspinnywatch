#include <pebble.h>
#include "OutlinedTextLayer.h"

typedef struct {
    GTextOverflowMode overflow;
    GTextAlignment alignment;
    GColor bg;
    GColor fg;
    GFont font;
    char* text;
} OutlinedTextData;

OutlinedTextData* outlined_text_layer_get_outline_data(OutlinedTextLayer* layer) {
    return layer_get_data((Layer*)layer);
}

void draw_outline(GPoint origin, GSize size, GContext *ctx, OutlinedTextData *data) {
    graphics_draw_text(ctx, data->text, data->font, (GRect){.origin=origin,.size=size}, data->overflow, data->alignment, NULL);
}

#define IMPRESSION(xx,yy) draw_outline((GPoint){bounds.origin.x+xx,bounds.origin.y+yy}, bounds.size, ctx, data);draw_outline((GPoint){bounds.origin.x-xx,bounds.origin.y-yy}, bounds.size, ctx, data)
void draw(Layer *layer, GContext *ctx) {
    OutlinedTextData* data = outlined_text_layer_get_outline_data((OutlinedTextLayer*)layer);
    GRect bounds = layer_get_bounds((Layer*)layer);
    graphics_context_set_text_color(ctx, data->bg);
    IMPRESSION(2,0);
    IMPRESSION(0,2);
    IMPRESSION(1,1);
    IMPRESSION(1,-1);
    graphics_context_set_text_color(ctx, data->fg);
    IMPRESSION(0,0);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", data->text);
}

OutlinedTextLayer *outlined_text_layer_create(GRect frame) {
    Layer *layer = layer_create_with_data(frame, sizeof(OutlinedTextData));
    OutlinedTextData* data = layer_get_data(layer);
    data->alignment = GTextAlignmentCenter;
    layer_set_update_proc(layer, draw);
    
    return (OutlinedTextLayer*) layer;
}

void outlined_text_layer_destroy(OutlinedTextLayer *layer) {
    layer_destroy((Layer*)layer);
}

Layer* outlined_text_layer_get_layer(OutlinedTextLayer* layer) {
    return (Layer*)layer;
}

void outlined_text_layer_set_text(OutlinedTextLayer* layer, char* text) {
    OutlinedTextData* data = outlined_text_layer_get_outline_data((OutlinedTextLayer*)layer);
    data->text = text;
    layer_mark_dirty((Layer*)layer);
}
void outlined_text_layer_set_font(OutlinedTextLayer* layer, GFont font) {
    OutlinedTextData* data = outlined_text_layer_get_outline_data((OutlinedTextLayer*)layer);
    data->font = font;
    layer_mark_dirty((Layer*)layer);
}

void outlined_text_layer_set_text_alignment(OutlinedTextLayer * layer, GTextAlignment text_alignment){
    outlined_text_layer_get_outline_data((OutlinedTextLayer*)layer)->alignment = text_alignment;
}

void outlined_text_layer_set_text_overflow_mode(OutlinedTextLayer * layer, GTextOverflowMode overflow){
    outlined_text_layer_get_outline_data((OutlinedTextLayer*)layer)->overflow = overflow;
}

void outlined_text_layer_set_colors(OutlinedTextLayer * layer, GColor background, GColor foreground) {
    OutlinedTextData* data = outlined_text_layer_get_outline_data((OutlinedTextLayer*)layer);
    data->fg = foreground;
    data->bg = background;
}