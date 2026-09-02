#include "pages/usage_dials_page.h"
#include "components/page_shell.h"
#include "services/claude_usage.h"
#include "services/codex_usage.h"
#include "services/cursor_usage.h"

#include <math.h>
#include <stdlib.h>

typedef struct {
    lv_obj_t *arc;
    lv_obj_t *value_label;
} usage_dial_t;

typedef struct {
    usage_dial_t claude_five_hour;
    usage_dial_t claude_week;
    usage_dial_t codex_five_hour;
    usage_dial_t codex_week;
    usage_dial_t cursor_models;
    usage_dial_t cursor_other;
    lv_timer_t *timer;
} usage_dials_view_t;

static double clamp_percent(double percent)
{
    if (percent < 0.0) return 0.0;
    if (percent > 100.0) return 100.0;
    return percent;
}

static void usage_dial_set(usage_dial_t *dial, double percent, bool ready)
{
    if (!ready) {
        lv_arc_set_value(dial->arc, 0);
        lv_label_set_text(dial->value_label, "--");
        return;
    }

    int rounded = (int)lround(clamp_percent(percent));
    lv_arc_set_value(dial->arc, rounded);
    lv_label_set_text_fmt(dial->value_label, "%d%%", rounded);
}

static usage_dial_t usage_dial_create(lv_obj_t *parent, const char *label,
                                      lv_color_t color)
{
    usage_dial_t dial = {0};
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, 112, 112);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    dial.arc = lv_arc_create(cell);
    lv_obj_remove_style(dial.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(dial.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(dial.arc, 82, 82);
    lv_arc_set_rotation(dial.arc, 135);
    lv_arc_set_bg_angles(dial.arc, 0, 270);
    lv_arc_set_range(dial.arc, 0, 100);
    lv_arc_set_value(dial.arc, 0);
    lv_obj_set_style_arc_width(dial.arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(dial.arc, lv_color_hex(0x292929), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(dial.arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(dial.arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(dial.arc, color, LV_PART_INDICATOR);
    lv_obj_align(dial.arc, LV_ALIGN_TOP_MID, 0, 0);

    dial.value_label = lv_label_create(cell);
    lv_label_set_text(dial.value_label, "--");
    lv_obj_set_style_text_font(dial.value_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(dial.value_label, lv_color_white(), 0);
    lv_obj_align(dial.value_label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *caption = lv_label_create(cell);
    lv_label_set_text(caption, label);
    lv_obj_set_style_text_font(caption, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(caption, lv_color_hex(0xA0A0A0), 0);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, 0);
    return dial;
}

static lv_obj_t *provider_row_create(lv_obj_t *parent, const char *provider,
                                     lv_color_t color)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 120);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, provider);
    lv_obj_set_width(name, 84);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(name, color, 0);
    return row;
}

static void update_usage_dials(lv_timer_t *timer)
{
    usage_dials_view_t *view = timer->user_data;
    claude_usage_snapshot_t claude = claude_usage_get();
    codex_usage_snapshot_t codex = codex_usage_get();
    cursor_usage_snapshot_t cursor = cursor_usage_get();

    bool claude_ready = claude.status == CLAUDE_USAGE_READY;
    usage_dial_set(&view->claude_five_hour,
                   100.0 - claude.five_hour_used_percent, claude_ready);
    usage_dial_set(&view->claude_week,
                   100.0 - claude.week_used_percent, claude_ready);

    bool codex_ready = codex.status == CODEX_USAGE_READY;
    usage_dial_set(&view->codex_five_hour,
                   100.0 - codex.five_hour_used_percent, codex_ready);
    usage_dial_set(&view->codex_week,
                   100.0 - codex.used_percent, codex_ready);

    bool cursor_ready = cursor.status == CURSOR_USAGE_READY;
    usage_dial_set(&view->cursor_models,
                   100.0 - cursor.cursor_models_used_percent, cursor_ready);
    usage_dial_set(&view->cursor_other,
                   100.0 - cursor.other_models_used_percent, cursor_ready);
}

static void delete_usage_dials_view(lv_event_t *event)
{
    usage_dials_view_t *view = lv_event_get_user_data(event);
    lv_timer_del(view->timer);
    free(view);
}

lv_obj_t *usage_dials_page_create(lv_obj_t *parent)
{
    lv_obj_t *content = page_shell_create(parent, lv_color_black(), 4);
    lv_obj_set_style_pad_all(content, 8, 0);
    usage_dials_view_t *view = calloc(1, sizeof(*view));

    lv_obj_t *claude = provider_row_create(content, "Claude",
                                            lv_color_hex(0xD97757));
    view->claude_five_hour = usage_dial_create(claude, "5 hour",
                                               lv_color_hex(0xD97757));
    view->claude_week = usage_dial_create(claude, "week",
                                          lv_color_hex(0xB85F43));

    lv_obj_t *codex = provider_row_create(content, "Codex",
                                           lv_color_hex(0x2B8FFF));
    view->codex_five_hour = usage_dial_create(codex, "5 hour",
                                              lv_color_hex(0x2B8FFF));
    view->codex_week = usage_dial_create(codex, "week",
                                         lv_color_hex(0x2476D1));

    lv_obj_t *cursor = provider_row_create(content, "Cursor",
                                            lv_color_white());
    view->cursor_models = usage_dial_create(cursor, "cursor",
                                             lv_color_white());
    view->cursor_other = usage_dial_create(cursor, "other",
                                            lv_color_hex(0xC8C8C8));

    codex_usage_start();
    cursor_usage_start();
    claude_usage_start();
    view->timer = lv_timer_create(update_usage_dials, 1000, view);
    update_usage_dials(view->timer);
    lv_obj_add_event_cb(lv_obj_get_parent(content), delete_usage_dials_view,
                        LV_EVENT_DELETE, view);
    return lv_obj_get_parent(content);
}
