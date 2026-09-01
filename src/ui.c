#include "ui.h"
#include "lvgl.h"
#include "pages/agent_page.h"
#include "pages/usage_page.h"

enum {
    LEADING_USAGE_PAGE,
    AGENT_PAGE,
    USAGE_PAGE,
    TRAILING_AGENT_PAGE,
    CAROUSEL_PAGE_COUNT
};

static lv_obj_t *carousel_pages[CAROUSEL_PAGE_COUNT];
static bool repositioning;

static void carousel_scroll_ended(lv_event_t *event)
{
    lv_obj_t *carousel = lv_event_get_target(event);
    lv_coord_t scroll_x = lv_obj_get_scroll_x(carousel);
    lv_coord_t page_width = lv_obj_get_width(carousel);

    /* A non-NULL parameter marks pointer release. If the page isn't already
       aligned, wait for the following snap-animation completion event. */
    lv_coord_t nearest_page = ((scroll_x + page_width / 2) / page_width) * page_width;
    lv_coord_t alignment_error = scroll_x - nearest_page;
    if ((lv_event_get_param(event) != NULL &&
         (alignment_error < -1 || alignment_error > 1)) || repositioning) {
        return;
    }

    if (scroll_x < page_width / 2) {
        repositioning = true;
        lv_obj_scroll_to_x(carousel, page_width * USAGE_PAGE, LV_ANIM_OFF);
        repositioning = false;
    } else if (scroll_x > page_width * 5 / 2) {
        repositioning = true;
        lv_obj_scroll_to_x(carousel, page_width * AGENT_PAGE, LV_ANIM_OFF);
        repositioning = false;
    }
}

void ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_radius(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);

    lv_obj_t *carousel = lv_obj_create(screen);
    lv_obj_remove_style_all(carousel);
    lv_obj_set_style_radius(carousel, 0, 0);
    lv_obj_set_size(carousel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(carousel, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(carousel, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(carousel, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_flag(carousel, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scrollbar_mode(carousel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(carousel, carousel_scroll_ended,
                        LV_EVENT_SCROLL_END, NULL);

    carousel_pages[LEADING_USAGE_PAGE] = usage_page_create(carousel);
    carousel_pages[AGENT_PAGE] = agent_page_create(carousel);
    carousel_pages[USAGE_PAGE] = usage_page_create(carousel);
    carousel_pages[TRAILING_AGENT_PAGE] = agent_page_create(carousel);

    lv_obj_update_layout(carousel);
    lv_obj_scroll_to_view(carousel_pages[AGENT_PAGE], LV_ANIM_OFF);
}
