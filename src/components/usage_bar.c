#include "components/usage_bar.h"

#include <math.h>
#include <stdlib.h>

enum {
    BAR_HEIGHT = 44,
    HORIZONTAL_PADDING = 16,
    BAR_RADIUS = 6
};

typedef struct {
    lv_obj_t *indicator;
    lv_obj_t *black_left;
    lv_obj_t *black_right;
    lv_obj_t *color_right;
    double usage;
} usage_bar_state_t;

static double clamp_usage(double usage)
{
    if (usage < 0.0) return 0.0;
    if (usage > 100.0) return 100.0;
    return usage;
}

static void position_indicator_text(lv_obj_t *bar, usage_bar_state_t *state)
{
    lv_obj_update_layout(bar);
    lv_coord_t bar_width = lv_obj_get_content_width(bar);
    lv_coord_t indicator_width = (lv_coord_t)lround(
        bar_width * clamp_usage(state->usage) / 100.0);

    lv_obj_set_width(state->indicator, indicator_width);
    lv_obj_set_pos(state->black_left, HORIZONTAL_PADDING,
                   (BAR_HEIGHT - lv_obj_get_height(state->black_left)) / 2);
    lv_obj_set_pos(state->black_right,
                   bar_width - HORIZONTAL_PADDING - lv_obj_get_width(state->black_right),
                   (BAR_HEIGHT - lv_obj_get_height(state->black_right)) / 2);
}

static void bar_size_changed(lv_event_t *event)
{
    lv_obj_t *bar = lv_event_get_target(event);
    usage_bar_state_t *state = lv_event_get_user_data(event);
    position_indicator_text(bar, state);
}

static void bar_deleted(lv_event_t *event)
{
    free(lv_event_get_user_data(event));
}

void usage_bar_set_usage(lv_obj_t *bar, double usage)
{
    usage_bar_state_t *state = lv_obj_get_user_data(bar);
    if (state == NULL) return;

    state->usage = clamp_usage(usage);
    int rounded = (int)lround(state->usage);
    lv_label_set_text_fmt(state->color_right, "%d%%", rounded);
    lv_label_set_text_fmt(state->black_right, "%d%%", rounded);
    lv_obj_update_layout(bar);
    lv_obj_align(state->color_right, LV_ALIGN_RIGHT_MID, -HORIZONTAL_PADDING, 0);
    position_indicator_text(bar, state);
}

lv_obj_t *usage_bar_create(lv_obj_t *parent, lv_color_t color,
                           const char *label, double usage)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), BAR_HEIGHT);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(bar, BAR_RADIUS, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_clip_corner(bar, true, 0);

    usage_bar_state_t *state = calloc(1, sizeof(*state));
    if (state == NULL) return bar;
    lv_obj_set_user_data(bar, state);

    lv_obj_t *color_left = lv_label_create(bar);
    lv_label_set_text(color_left, label);
    lv_obj_set_style_text_font(color_left, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(color_left, color, 0);
    lv_obj_align(color_left, LV_ALIGN_LEFT_MID, HORIZONTAL_PADDING, 0);

    state->color_right = lv_label_create(bar);
    lv_obj_set_style_text_font(state->color_right, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(state->color_right, color, 0);
    lv_obj_align(state->color_right, LV_ALIGN_RIGHT_MID, -HORIZONTAL_PADDING, 0);

    state->indicator = lv_obj_create(bar);
    lv_obj_remove_style_all(state->indicator);
    lv_obj_set_height(state->indicator, LV_PCT(100));
    lv_obj_set_pos(state->indicator, 0, 0);
    lv_obj_clear_flag(state->indicator, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(state->indicator, BAR_RADIUS, 0);
    lv_obj_set_style_bg_color(state->indicator, color, 0);
    lv_obj_set_style_bg_opa(state->indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(state->indicator, true, 0);

    state->black_left = lv_label_create(state->indicator);
    lv_label_set_text(state->black_left, label);
    lv_obj_set_style_text_font(state->black_left, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(state->black_left, lv_color_black(), 0);

    state->black_right = lv_label_create(state->indicator);
    lv_obj_set_style_text_font(state->black_right, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(state->black_right, lv_color_black(), 0);

    lv_obj_add_event_cb(bar, bar_size_changed, LV_EVENT_SIZE_CHANGED, state);
    lv_obj_add_event_cb(bar, bar_deleted, LV_EVENT_DELETE, state);
    usage_bar_set_usage(bar, usage);
    return bar;
}
