#include "pages/usage_page.h"
#include "components/page_indicator.h"
#include "components/page_shell.h"

lv_obj_t *usage_page_create(lv_obj_t *parent)
{
    lv_obj_t *content = page_shell_create(parent, lv_color_hex(0x10221D), 18);

    lv_obj_t *eyebrow = lv_label_create(content);
    lv_label_set_text(eyebrow, "WEEKLY USAGE");
    lv_obj_set_style_text_font(eyebrow, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(eyebrow, lv_color_hex(0x74E3B4), 0);

    lv_obj_t *value = lv_label_create(content);
    lv_label_set_text(value, "68%");
    lv_obj_set_style_text_font(value, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(value, lv_color_white(), 0);

    lv_obj_t *remaining = lv_label_create(content);
    lv_label_set_text(remaining, "remaining");
    lv_obj_set_style_text_font(remaining, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(remaining, lv_color_hex(0xB5C9C1), 0);

    lv_obj_t *bar = lv_bar_create(content);
    lv_obj_set_size(bar, 260, 18);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 68, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x263D35), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x48D597), LV_PART_INDICATOR);

    lv_obj_t *reset = lv_label_create(content);
    lv_label_set_text(reset, "Resets in 3 days");
    lv_obj_set_style_text_font(reset, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(reset, lv_color_hex(0x77988A), 0);

    page_indicator_create(content, 1, 2);
    return lv_obj_get_parent(content);
}
