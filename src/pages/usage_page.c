#include "pages/usage_page.h"
#include "components/page_shell.h"
#include "components/usage_bar.h"
#include "services/claude_usage.h"
#include "services/codex_usage.h"
#include "services/cursor_usage.h"

#include <stdlib.h>

typedef struct {
    lv_obj_t *claude_five_hour_bar;
    lv_obj_t *claude_week_bar;
    lv_obj_t *week_bar;
    lv_obj_t *five_hour_bar;
    lv_obj_t *cursor_bar;
    lv_obj_t *other_bar;
    lv_timer_t *timer;
} usage_view_t;

static void update_usage_view(lv_timer_t *timer)
{
    usage_view_t *view = timer->user_data;
    codex_usage_snapshot_t usage = codex_usage_get();
    cursor_usage_snapshot_t cursor = cursor_usage_get();
    claude_usage_snapshot_t claude = claude_usage_get();
    if (claude.status == CLAUDE_USAGE_READY) {
        usage_bar_set_usage(view->claude_five_hour_bar,
                            100.0 - claude.five_hour_used_percent);
        usage_bar_set_usage(view->claude_week_bar,
                            100.0 - claude.week_used_percent);
    } else {
        usage_bar_set_usage(view->claude_five_hour_bar, 0);
        usage_bar_set_usage(view->claude_week_bar, 0);
    }
    if (cursor.status == CURSOR_USAGE_READY) {
        usage_bar_set_usage(view->cursor_bar,
                            100.0 - cursor.cursor_models_used_percent);
        usage_bar_set_usage(view->other_bar,
                            100.0 - cursor.other_models_used_percent);
    } else {
        usage_bar_set_usage(view->cursor_bar, 0);
        usage_bar_set_usage(view->other_bar, 0);
    }
    if (usage.status == CODEX_USAGE_CONNECTING) {
        usage_bar_set_usage(view->week_bar, 0);
        usage_bar_set_usage(view->five_hour_bar, 0);
        return;
    }
    if (usage.status != CODEX_USAGE_READY) {
        usage_bar_set_usage(view->week_bar, 0);
        usage_bar_set_usage(view->five_hour_bar, 0);
        return;
    }

    usage_bar_set_usage(view->week_bar, 100 - usage.used_percent);
    usage_bar_set_usage(view->five_hour_bar,
                        100 - usage.five_hour_used_percent);
}

static void delete_usage_view(lv_event_t *event)
{
    usage_view_t *view = lv_event_get_user_data(event);
    lv_timer_del(view->timer);
    free(view);
}

lv_obj_t *usage_page_create(lv_obj_t *parent)
{
    lv_obj_t *content = page_shell_create(parent, lv_color_black(), 12);

    usage_view_t *view = calloc(1, sizeof(*view));

    view->claude_five_hour_bar = usage_bar_create(
        content, lv_color_hex(0xD97757), "5h", 0);
    view->claude_week_bar = usage_bar_create(
        content, lv_color_hex(0xB85F43), "week", 0);
    view->five_hour_bar = usage_bar_create(
        content, lv_color_hex(0x2B8FFF), "5h", 0);
    view->week_bar = usage_bar_create(
        content, lv_color_hex(0x2476D1), "week", 0);
    view->cursor_bar = usage_bar_create(content, lv_color_white(),
                                        "cursor", 0);
    view->other_bar = usage_bar_create(content, lv_color_hex(0xC8C8C8),
                                       "other", 0);

    codex_usage_start();
    cursor_usage_start();
    claude_usage_start();
    view->timer = lv_timer_create(update_usage_view, 1000, view);
    lv_obj_add_event_cb(lv_obj_get_parent(content), delete_usage_view,
                        LV_EVENT_DELETE, view);

    return lv_obj_get_parent(content);
}
