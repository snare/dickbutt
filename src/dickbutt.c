#include <pebble.h>

#include "dickbutt.h"

static Window *window;
static Layer *bg_layer;
static GBitmap *dickbutt_bitmap;
static GBitmap *arm_bitmap_black;
static GBitmap *arm_bitmap_white;

static RotBitmapLayer *dickbutt_layer;
static RotBitmapLayer *arm_layer_black;
static RotBitmapLayer *arm_layer_white;

static GPath *minute_arrow;
static GPath *hour_arrow;
static GPath *tick_paths[NUM_CLOCK_TICKS];

static GPoint center;

static void bg_update_proc(Layer *layer, GContext *ctx)
{
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

    graphics_context_set_fill_color(ctx, GColorBlack);
    for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
        gpath_draw_filled(ctx, tick_paths[i]);
    }
}

static void update_time()
{
    GPoint offset = {0, 0};
    GRect frame;

    time_t sec = time(NULL);
    struct tm *tick = localtime(&sec);

    uint32_t hour = tick->tm_hour;
    uint32_t minute = tick->tm_min;

    // hour = 6;
    // minute = 15;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "update_hour(%ld)", hour);

    // Dickbutt normally points close to 3, so we offset by 9 segments and a bit extra
    uint32_t angle = (hour + 9) % 12 * (TRIG_MAX_ANGLE/12) + 0x400;
    angle += minute * TRIG_MAX_ANGLE/12/60;
    rot_bitmap_layer_set_angle(dickbutt_layer, angle);
    layer_mark_dirty((Layer *)dickbutt_layer);

    // Update the angle
    angle = minute * (TRIG_MAX_ANGLE/60);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "angle: 0x%lx", angle);
    rot_bitmap_layer_set_angle(arm_layer_black, angle);
    rot_bitmap_layer_set_angle(arm_layer_white, angle);

    // Center arm
    frame = layer_get_frame((Layer *)arm_layer_black);
    frame.origin.x = center.x - frame.size.w/2;
    frame.origin.y = center.y - frame.size.h/2;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "updating arm frame: %d, %d, %d, %d", frame.origin.x, frame.origin.y, frame.size.w,
        frame.size.h);

    layer_set_frame((Layer *)arm_layer_black, frame);
    layer_set_frame((Layer *)arm_layer_white, frame);

    // Mark layers dirty
    layer_mark_dirty((Layer *)arm_layer_black);
    layer_mark_dirty((Layer *)arm_layer_white);
}

static void window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    center = grect_center_point(&bounds);
    GRect frame;

    // Background layer
    bg_layer = layer_create(bounds);
    layer_set_update_proc(bg_layer, bg_update_proc);
    layer_add_child(window_layer, bg_layer);

    // Dickbutt layer
    dickbutt_layer = rot_bitmap_layer_create(dickbutt_bitmap);
    rot_bitmap_set_compositing_mode(dickbutt_layer, GCompOpClear);

    // Frame ends up being 221x221 because it adjusts for rotation, so we need to move the origin offscreen
    frame = layer_get_frame((Layer *)dickbutt_layer);
    frame.origin.x = center.x - frame.size.w / 2;
    frame.origin.y = center.y - frame.size.h / 2;
    layer_set_frame((Layer *)dickbutt_layer, frame);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "frame: %d, %d, %d, %d", frame.origin.x, frame.origin.y, frame.size.w, frame.size.h);

    // Add the dickbutt layer to the window
    layer_add_child(window_get_root_layer(window), (Layer *)dickbutt_layer);

    // Arm layer
    arm_layer_black = rot_bitmap_layer_create(arm_bitmap_black);
    arm_layer_white = rot_bitmap_layer_create(arm_bitmap_white);
    rot_bitmap_set_compositing_mode(arm_layer_black, GCompOpClear);
    rot_bitmap_set_compositing_mode(arm_layer_white, GCompOpOr);

    // Set frame
    frame = layer_get_frame((Layer *)arm_layer_black);
    frame.origin.x = center.x - frame.size.w / 2;
    frame.origin.y = center.y - frame.size.h / 2;
    layer_set_frame((Layer *)arm_layer_black, frame);
    layer_set_frame((Layer *)arm_layer_white, frame);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "frame: %d, %d, %d, %d", frame.origin.x, frame.origin.y, frame.size.w, frame.size.h);

    // Add the arm layer to the window
    layer_add_child(window_get_root_layer(window), (Layer *)arm_layer_black);
    layer_add_child(window_get_root_layer(window), (Layer *)arm_layer_white);
}

static void window_unload(Window *window)
{
    layer_destroy(bg_layer);
}


static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
    update_time();
}

static void init()
{
    // Create window and add dickbutt layer
    window = window_create();

    // Set handlers to manage the elements inside the Window
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });


    // Load dickbutt
    dickbutt_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DICKBUTT_BLACK);

    // Load arm
    arm_bitmap_black = gbitmap_create_with_resource(RESOURCE_ID_ARM_BLACK);
    arm_bitmap_white = gbitmap_create_with_resource(RESOURCE_ID_ARM_WHITE);

    // init hand paths
    minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
    hour_arrow = gpath_create(&HOUR_HAND_POINTS);

    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    const GPoint center = grect_center_point(&bounds);
    gpath_move_to(minute_arrow, center);
    gpath_move_to(hour_arrow, center);

    // init clock face paths
    for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
        tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
    }

    // Show the Window on the watch, with animated=true
    window_stack_push(window, true);

    // Register with TickTimerService
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    update_time();
}

static void deinit(void)
{
    gbitmap_destroy(dickbutt_bitmap);
    rot_bitmap_layer_destroy(dickbutt_layer);
    window_destroy(window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
