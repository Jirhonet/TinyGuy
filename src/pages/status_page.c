#include "pages/status_page.h"
#include "components/page_shell.h"

lv_obj_t *status_page_create(lv_obj_t *parent)
{
    lv_obj_t *content = page_shell_create(parent, lv_color_hex(0x090B12), 22);

    lv_obj_t *badge = lv_obj_create(content);
    lv_obj_set_size(badge, 86, 86);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x7C5CFC), 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *face = lv_label_create(badge);
    lv_label_set_text(face, "^  ^\n  o");
    lv_obj_set_style_text_align(face, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(face, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(face, lv_color_white(), 0);
    lv_obj_center(face);

    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "Tiny Guy is working");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *subtitle = lv_label_create(content);
    lv_label_set_text(subtitle, "Codex  |  active now");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA9B1C7), 0);

    lv_obj_t *hint = lv_label_create(content);
    lv_label_set_text(hint, "<  swipe to view usage  >");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x747B90), 0);

    return lv_obj_get_parent(content);
}
