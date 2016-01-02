#include <pebble.h>
#define INTERVAL_ACCEL_CHECK 20

static Window *s_main_window;

static Layer *s_layer_stripes;
static Layer *s_layer_falling_stripes;
static GPath *s_path_stripe[4];
static GPath *s_path_falling_stripe[4];

// ======= Below is stuff for selection

static Window *s_select_window;
//static TextLayer *s_text_layer;
//static TextLayer *s_text_layer_2;
static Layer *s_graphics_layer;
static Layer *s_palettes_layer;
static Layer *s_loading_layer;

static Window *s_confirm_window;
static Layer *s_confirm_layer;
static TextLayer *s_confirm_text;

static bool s_confirm_timer_status = false; // false not running    true running
static bool s_select_timer_status = false;

static uint16_t s_confirm_progress = 80;

static int8_t oldPosition = -1;
static time_t positionChangeTimeSeconds = 0; 
static uint16_t positionChangeTimeMillis = 0;
static time_t currentTimeSeconds = 0; 
static uint16_t currentTimeMillis = 0;
static time_t lastTapTimeSeconds = 0; 
static uint16_t lastTapTimeMillis = 0;
static int8_t position = 0;

static int16_t s_x_position_confirm = 0;
static int16_t s_y_position_confirm = 0;

static PropertyAnimation *s_palette_move_animation;

// =======

static GPathInfo s_path_stripe_info[4] = {{
  .num_points = 4,
  .points = (GPoint []) {{0, 0}, {36, 0}, {36, 168}, {0, 168}}
}, {
  .num_points = 4,
  .points = (GPoint []) {{36, 0}, {72, 0}, {72, 168}, {36 , 168}}
}, {
  .num_points = 4,
  .points = (GPoint []) {{72, 0}, {108, 0}, {108, 168}, {72, 168}}
}, {
  .num_points = 4,
  .points = (GPoint []) {{108, 0}, {144, 0}, {144, 168}, {108, 168}}
}};
static GPathInfo s_path_falling_stripe_info[4] = {{
  .num_points = 4,
  .points = (GPoint []) {{0, 0}, {36, 0}, {36, 168}, {0, 168}}
}, {
  .num_points = 4,
  .points = (GPoint []) {{36, 0}, {72, 0}, {72, 168}, {36 , 168}}
}, {
  .num_points = 4,
  .points = (GPoint []) {{72, 0}, {108, 0}, {108, 168}, {72, 168}}
}, {
  .num_points = 4,
  .points = (GPoint []) {{108, 0}, {144, 0}, {144, 168}, {108, 168}}
}};

static int8_t s_fallDirection[] = {10, -5, 0, -10};

static uint8_t s_falling = 0;
static bool s_falling_reset = true;

// ------  begin time code  ------

static TextLayer *s_time_layer[4];
static TextLayer *s_colon_layer;
static char s_oldTimeDigits[4][2] = {"0", "0", "0", "0"};
static uint8_t s_currentColorNum = 4;
static uint8_t s_colorNumbers[] = {0, 1, 2, 3};
static uint8_t s_fallingColorNumbers[] = {0, 1, 2, 3};

static uint8_t s_colorPaletteUsed = 3;

//static uint8_t s_color_palette[] = {
//    0b11111110, 0b11111101, 0b11111100, 0b11111001, 0b11111000, 0b11110100, 0b11110000, 0b11110101, 0b11110001, 0b11110010, 0b11110111, 0b11110011, 0b11100111, 0b11100011, 0b11101011, 0b11010111, 0b11010011, 0b11011011, 0b11001011, 0b11011111, 0b11011110, 0b11001110, 0b11001100, 0b11101100, 0b11101101
//};

static uint8_t s_color_palette[5][7] = {
    {0b11101100, 0b11111100, 0b11111001, 0b11111000, 0b11110100, 0b11110000, 0b11110001},
    {0b11110010, 0b11111011, 0b11110111, 0b11100011, 0b11100010, 0b11100001, 0b11110110},
    {0b11101100, 0b11011100, 0b11001111, 0b11011110, 0b11111100, 0b11011101, 0b11101111},
    {0b11010111, 0b11000111, 0b11000011, 0b11011011, 0b11001111, 0b11010011, 0b11010010},
    {0b11111101, 0b11110000, 0b11110111, 0b11010111, 0b11001111, 0b11001100, 0b11100011}
};
// ------  begin main code

static uint16_t progressToRatio(AnimationProgress progress, int full) {
    return (int) (float) ((float) progress / (float) ANIMATION_NORMALIZED_MAX * (float) full);
}

static void fallingStripesCallback(Animation *animation, AnimationProgress rawProgress) {
    for (uint8_t i = 0; i < 4; i++) {
        if (s_falling & (1 << i)) {
            uint16_t progress = progressToRatio(rawProgress, 168);
            uint8_t steps = 168;
            s_path_falling_stripe_info[i].points[0] = (GPoint)
                {(s_fallDirection[i] * progress) / steps + (0 * (steps - progress)) / steps + i * 36,
                (168 * progress) / steps + (0 * (steps - progress)) / steps};
            s_path_falling_stripe_info[i].points[1] = (GPoint)
                {((36 + s_fallDirection[i]) * progress) / steps + (36 * (steps - progress)) / steps + i * 36,
                (168 * progress) / steps + (0 * (steps - progress)) / steps};
        } else {
            s_path_falling_stripe_info[i].points[0] = (GPoint) {i * 36, 0};
            s_path_falling_stripe_info[i].points[1] = (GPoint) {(i + 1) * 36, 0};   
        }
        s_path_falling_stripe_info[i].points[2] = (GPoint) {(i + 1) * 36, 168};
        s_path_falling_stripe_info[i].points[3] = (GPoint) {i * 36, 168};
    }
    layer_mark_dirty(s_layer_falling_stripes);
}

static void redraw_stripes(Layer *layer, GContext *ctx) {
    for (uint8_t i = 0; i < 4; i++) {
        GColor color = (GColor) {
            .argb = s_color_palette[s_colorPaletteUsed][s_colorNumbers[i]]
        };
        graphics_context_set_fill_color(ctx, color);
        gpath_draw_filled(ctx, s_path_stripe[i]);
        graphics_context_set_stroke_color(ctx, color);
        gpath_draw_outline(ctx, s_path_stripe[i]);
    }
}

static void redraw_falling_stripes(Layer *layer, GContext *ctx) {
    if (s_falling_reset == true) {
        for (uint8_t i = 0; i < 4; i++) {
            s_path_falling_stripe_info[i].points[0] = (GPoint) {i * 36, 0};
            s_path_falling_stripe_info[i].points[1] = (GPoint) {(i + 1) * 36, 0};   
            s_path_falling_stripe_info[i].points[2] = (GPoint) {(i + 1) * 36, 168};
            s_path_falling_stripe_info[i].points[3] = (GPoint) {i * 36, 168};
        }
        s_falling_reset = false;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if (s_falling & (1 << i)) {
            GColor color = (GColor) {
                .argb = s_color_palette[s_colorPaletteUsed][s_fallingColorNumbers[i]]
            };
            graphics_context_set_fill_color(ctx, color);
            gpath_draw_filled(ctx, s_path_falling_stripe[i]);
            graphics_context_set_stroke_color(ctx, color);
            gpath_draw_outline(ctx, s_path_falling_stripe[i]);
        }
    }
}

static void setup_time_paths() {
    for (uint8_t i = 0; i < 4; i++) {
        s_path_stripe[i] = gpath_create(&s_path_stripe_info[i]);
        s_path_falling_stripe[i] = gpath_create(&s_path_falling_stripe_info[i]);
    }
}

static void main_window_load(Window *window) {
    layer_add_child(window_get_root_layer(window), s_layer_stripes);
    for (uint8_t i = 0; i < 4; i++) {
        s_time_layer[i] = text_layer_create(GRect((i * 36), 55, 36, 50));
        text_layer_set_background_color(s_time_layer[i], GColorClear);
        text_layer_set_text_color(s_time_layer[i], GColorBlack);
        text_layer_set_text(s_time_layer[i], "0");
        text_layer_set_font(s_time_layer[i], fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
        text_layer_set_text_alignment(s_time_layer[i], GTextAlignmentCenter);
        layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer[i]));
    }
    s_colon_layer = text_layer_create(GRect(0, 55, 144, 50));
    text_layer_set_background_color(s_colon_layer, GColorClear);
    text_layer_set_text_color(s_colon_layer, GColorBlack);
    text_layer_set_text(s_colon_layer, ":");
    text_layer_set_font(s_colon_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
    text_layer_set_text_alignment(s_colon_layer, GTextAlignmentCenter);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_colon_layer));

    layer_add_child(window_get_root_layer(window), s_layer_falling_stripes);
}

static void main_window_unload(Window *window) {
    layer_destroy(s_layer_stripes);
    layer_destroy(s_layer_falling_stripes);
    text_layer_destroy(s_colon_layer);
    for (uint8_t i = 0; i < 4; i++) {
        text_layer_destroy(s_time_layer[i]);
    }
}

static void animation_stopped(Animation *animation, bool stopped, void *context) {
    animation_destroy(animation);
    s_falling = 0;
}

static void animate(int duration, int delay,
        AnimationImplementation *implementation, bool useHandlers) {
    Animation *animation = animation_create();
    animation_set_delay(animation, delay);
    animation_set_duration(animation, duration);
    animation_set_curve(animation, AnimationCurveEaseOut);
    animation_set_implementation(animation, implementation);
    animation_schedule(animation);
    if (useHandlers) {
        animation_set_handlers(animation, (AnimationHandlers) {
            .stopped = animation_stopped
        }, NULL);
    }
}

static void update_time(bool playAnimation) {
    s_falling = 0;
    time_t currentTime = time(NULL);
    struct tm *tick_time = localtime(&currentTime);
    static char buffer[] = "0000";
    static char buffers[4][2] = {"0", "0", "0", "0"};
    if (clock_is_24h_style() == true) {
        strftime(buffer, sizeof("0000"), "%H%M", tick_time); //H
    } else {
        strftime(buffer, sizeof("0000"), "%I%M", tick_time); //I
    }
    for (uint8_t i = 0; i < 4; i++) {
        buffers[i][0] = buffer[i];
        text_layer_set_text(s_time_layer[i], buffers[i]);
        if (playAnimation) {
            if (s_oldTimeDigits[i][0] != buffer[i]) {
                s_falling |= 1 << i;
            }
        }
        s_oldTimeDigits[i][0] = buffer[i];
    }
    for (uint8_t i = 0; i < 4; i++) {
        s_fallDirection[i] = (rand() % 21) - 10;
    }
    if (s_falling != 0) {
        bool fittingColorFound = false;
        for (uint8_t i = 0; i < 4; i++) {
            fittingColorFound = false;
            s_fallingColorNumbers[i] = s_colorNumbers[i];
            if (s_falling & (1 << i)) {
                for (uint8_t attemps = 0; attemps < 4 && fittingColorFound == false; attemps++) {
                    // ^^ this is necessary just in case a palette has less than colors (we don't want infinite loops!)
                    fittingColorFound = true;
                    s_currentColorNum++;
                    s_currentColorNum %= sizeof(s_color_palette[0]) / sizeof(s_color_palette[0][0]);
                    for (uint8_t stripeNum = 0; stripeNum < 4; stripeNum++) {
                        if (s_colorNumbers[stripeNum] == s_currentColorNum) {
                            fittingColorFound = false;
                        }
                    }
                }
                s_colorNumbers[i] = s_currentColorNum;
            }
        }
        static AnimationImplementation fallingStripesImplementation = {
            .update = fallingStripesCallback
        };
        animate(500, 0, &fallingStripesImplementation, true);
        s_falling_reset = true;
        layer_mark_dirty(s_layer_falling_stripes);
        layer_mark_dirty(s_layer_stripes);
    }
    APP_LOG(APP_LOG_LEVEL_WARNING, "Bytes free: %i", heap_bytes_free());
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    update_time(true);
}

// begin select code

static void property_animation_done_handler(Animation *animation, bool finished, void *context) {
    property_animation_destroy(s_palette_move_animation);
}

static void palettes_update(Layer *layer, GContext *ctx) {
    static uint8_t paletteNum = 0;
    static uint8_t colorNum = 0;
    for (paletteNum = 0; paletteNum < 5; paletteNum++) {
        for (colorNum = 0; colorNum < 7; colorNum++) {
            GColor color = (GColor) {
                .argb = s_color_palette[paletteNum][colorNum]
            };
            graphics_context_set_fill_color(ctx, color);
            graphics_fill_rect(ctx, GRect(9 + 18 * colorNum, paletteNum * 26, 18, 18), 2, (colorNum == 0) ? GCornersLeft : ((colorNum == 6) ? GCornersRight : GCornerNone));
        }
    }
}

static void graphics_layer_update(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(0, 72, 144, 24), 0, GCornerNone);
}

static void loading_layer_update(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(0, 148, 144, 20), 0, GCornerNone);
    int32_t timeDiffMillis = (currentTimeSeconds * 1000 + currentTimeMillis) - (positionChangeTimeSeconds * 1000 + positionChangeTimeMillis);
    uint8_t barWidth = (uint8_t) (float) 140 * ((float) timeDiffMillis / 2000);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(2, 150, barWidth > 140 ? 140 : barWidth, 16), 0, GCornerNone);
}

static void confirm_layer_update(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 168), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(53, 64, 40, 40), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(67 + (s_x_position_confirm / 10), 79 + (s_y_position_confirm / 10), 10, 10), 5, GCornersAll);

    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(0, 148, 144, 20), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, GColorLightGray);
    graphics_fill_rect(ctx, GRect(2, 150, s_confirm_progress > 140 ? 140 : s_confirm_progress, 16), 0, GCornerNone);
}

static void select_update_call();
static void select_timer_start();
static void select_timer_stop();

static void select_update(bool reset) {
    if (reset) {
        oldPosition = -1;
        time_ms(&positionChangeTimeSeconds, &positionChangeTimeMillis);
    }
    time_ms(&currentTimeSeconds, &currentTimeMillis);

    AccelData accel_data = (AccelData) { .x = 0, .y = 0, .z = 0 };
    static char buffer[10];
    static char buffer2[10];
    accel_service_peek(&accel_data);
    position = accel_data.y / -150;
    position = position - 1; // sorry bout the magic numbers.
    if (position < 0) {
        position = 0;
    } if (position > 4) {
        position = 4;
    }
    if (position != oldPosition) {
        animation_unschedule_all();
        time_ms(&positionChangeTimeSeconds, &positionChangeTimeMillis);
        oldPosition = position;
        GRect fromFrame = layer_get_frame(s_palettes_layer);
        GRect toFrame = GRect(0, -26 * position + 75, 144, 168);
        s_palette_move_animation = property_animation_create_layer_frame(s_palettes_layer, &fromFrame, &toFrame);
        animation_set_duration((Animation*)s_palette_move_animation, 100);
        animation_set_delay((Animation*)s_palette_move_animation, 0);
        animation_set_curve((Animation*)s_palette_move_animation, AnimationCurveEaseInOut);
        animation_set_handlers((Animation*)s_palette_move_animation, (AnimationHandlers) {
            .stopped = property_animation_done_handler
        }, NULL);
        animation_schedule((Animation*)s_palette_move_animation);
    }
    int32_t timeDiffMillis = (currentTimeSeconds * 1000 + currentTimeMillis) - (positionChangeTimeSeconds * 1000 + positionChangeTimeMillis);
    if (timeDiffMillis > 2000) {
        snprintf(buffer, sizeof(buffer), "%i", position);
    } else {
        snprintf(buffer, sizeof(buffer), "%i SELECT", position);
    }
    layer_mark_dirty(s_loading_layer);
    //text_layer_set_text(s_text_layer, buffer);
    //snprintf(buffer2, sizeof(buffer2), "%li", timeDiffMillis);
    //text_layer_set_text(s_text_layer_2, buffer2);
    if (timeDiffMillis <= 2000) {
        app_timer_register(INTERVAL_ACCEL_CHECK, select_update_call, NULL);
    } else {
        select_timer_stop();
        s_colorPaletteUsed = position;
        window_stack_remove(s_select_window, true);
    }
    APP_LOG(APP_LOG_LEVEL_WARNING, "Bytes free: %i", heap_bytes_free());
}

static void select_update_call() {
    select_update(false);
}

static void select_timer_start() {
    if (s_select_timer_status == false) { // check this so there's never two instances running at the same time!
        s_select_timer_status = true;
        select_update(true);
    }
}

static void select_timer_stop() {
    s_select_timer_status = false;
}

static void confirm_update_call();
static void confirm_timer_start();
static void confirm_timer_stop();

static void confirm_update(bool reset) {
    if (s_confirm_timer_status == false) {
        return;
    }
    AccelData accel_data = (AccelData) { .x = 0, .y = 0, .z = 0 };
    accel_service_peek(&accel_data);
    s_x_position_confirm = accel_data.x;
    s_y_position_confirm = accel_data.y;
    if (s_x_position_confirm > 720) {
        s_x_position_confirm = 720;
    } else if (s_x_position_confirm < -720) {
        s_x_position_confirm = -720;
    }
    if (s_y_position_confirm > 640) { // 64 because of bar at bottom
        s_y_position_confirm = 840;
    } else if (s_y_position_confirm < -840) {
        s_y_position_confirm = -840;
    }

    if (s_x_position_confirm > -200 && s_x_position_confirm < 200 && 
        s_y_position_confirm > -200 && s_y_position_confirm < 200) {
        s_confirm_progress += 1;
    } else {
        s_confirm_progress -= 1;
    }
    layer_mark_dirty(s_confirm_layer);
    if (s_confirm_progress < 140 && s_confirm_progress > 0) {
        app_timer_register(INTERVAL_ACCEL_CHECK, confirm_update_call, NULL);
    } else {
        confirm_timer_stop();
        if (s_confirm_progress >= 140) {
            // finished
            window_stack_push(s_select_window, true);
            window_stack_remove(s_confirm_window, true);
            select_timer_start();
        } else {
            // 0 or less
            position = 80;
            window_stack_remove(s_confirm_window, true);
        }
    }
}

static void confirm_update_call() {
    confirm_update(false);
}

static void confirm_timer_start() {
    if (s_confirm_timer_status == false) { // check this so there's never two instances running at the same time!
        s_confirm_timer_status = true;
        confirm_update(true);
    }
}

static void confirm_timer_stop() {
    s_confirm_timer_status = false;
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
    AccelData accel_data = (AccelData) { .x = 0, .y = 0, .z = 0 };
    accel_service_peek(&accel_data);
    if (window_stack_get_top_window() == s_main_window) {
        time_ms(&currentTimeSeconds, &currentTimeMillis);
        int32_t timeDiffMillis = (currentTimeSeconds * 1000 + currentTimeMillis) - (lastTapTimeSeconds * 1000 + lastTapTimeMillis);
        if (timeDiffMillis < 3000) {
            window_stack_push(s_confirm_window, true);
            confirm_timer_stop();
            vibes_short_pulse();
            s_confirm_progress = 80;
            app_timer_register(1000, confirm_timer_start, NULL);
            layer_mark_dirty(s_confirm_layer);
        } else {
            time_ms(&lastTapTimeSeconds, &lastTapTimeMillis);
        }
    }
}

static void select_window_load(Window *window) {
    s_palettes_layer = layer_create(GRect(0, 0, 144, 168));
    s_graphics_layer = layer_create(GRect(0, 0, 144, 168));
    s_loading_layer = layer_create(GRect(0, 0, 144, 168));
    //s_text_layer = text_layer_create(GRect(0, 0, 144, 168));
    //s_text_layer_2 = text_layer_create(GRect(0, 20, 144, 168));
    layer_set_update_proc(s_palettes_layer, palettes_update);
    layer_mark_dirty(s_palettes_layer);
    layer_set_update_proc(s_graphics_layer, graphics_layer_update);
    layer_mark_dirty(s_graphics_layer);
    layer_set_update_proc(s_loading_layer, loading_layer_update);
    layer_mark_dirty(s_loading_layer);
    
    window_set_background_color(window, GColorBlack);

    //text_layer_set_background_color(s_text_layer, GColorClear);
    //text_layer_set_background_color(s_text_layer_2, GColorClear);
    //text_layer_set_text_color(s_text_layer, GColorWhite);
    //text_layer_set_text_color(s_text_layer_2, GColorWhite);

    //text_layer_set_text(s_text_layer, "Nothing happened");
    //text_layer_set_text(s_text_layer_2, "...yet");
    //text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
    //text_layer_set_text_alignment(s_text_layer_2, GTextAlignmentCenter);

    layer_add_child(window_get_root_layer(window), s_graphics_layer);
    layer_add_child(window_get_root_layer(window), s_palettes_layer);
    //layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_text_layer));
    //layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_text_layer_2));
    layer_add_child(window_get_root_layer(window), s_loading_layer);
    select_timer_start();
}

static void select_window_unload(Window *window) {
    //text_layer_destroy(s_text_layer);
    //text_layer_destroy(s_text_layer_2);
    layer_destroy(s_palettes_layer);
    layer_destroy(s_graphics_layer);
    layer_destroy(s_loading_layer);
}


static void confirm_window_load(Window *window) {
    s_confirm_layer = layer_create(GRect(0, 0, 144, 168));
    s_confirm_text = text_layer_create(GRect(0, 0, 144, 60));

    text_layer_set_background_color(s_confirm_text, GColorClear);
    text_layer_set_text_color(s_confirm_text, GColorWhite);
    text_layer_set_text(s_confirm_text, "Keep the circle in the square to change the color theme");
    text_layer_set_text_alignment(s_confirm_text, GTextAlignmentCenter);

    layer_set_update_proc(s_confirm_layer, confirm_layer_update);
    layer_mark_dirty(s_confirm_layer);
    layer_add_child(window_get_root_layer(window), s_confirm_layer);
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_confirm_text));
}

static void confirm_window_appear(Window *window) {
    ;
}

static void confirm_window_unload(Window *window) {
    layer_destroy(s_confirm_layer);
    text_layer_destroy(s_confirm_text);
}

// -------

static void init() {
    // make the window
    s_main_window = window_create();
    // Add handlers
    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });
    s_select_window = window_create();
    window_set_window_handlers(s_select_window, (WindowHandlers) {
        .load = select_window_load,
        .unload = select_window_unload,
    });
    s_confirm_window = window_create();
    window_set_window_handlers(s_confirm_window, (WindowHandlers) {
        .load = confirm_window_load,
        .appear = confirm_window_appear,
        .unload = confirm_window_unload,
    });

    s_layer_stripes = layer_create(GRect(0, 0, 144, 168));
    s_layer_falling_stripes = layer_create(GRect(0, 0, 144, 168));
    layer_set_update_proc(s_layer_stripes, redraw_stripes);
    layer_set_update_proc(s_layer_falling_stripes, redraw_falling_stripes);
    setup_time_paths();
    // actually show the window, animated
    window_stack_push(s_main_window, true);
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
    accel_tap_service_subscribe(tap_handler);
    update_time(false);
}

static void deinit() {
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
