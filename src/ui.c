#include "ui.h"
#include "lvgl.h"

static void button_clicked(lv_event_t *event)
{
    lv_obj_t *label = lv_event_get_user_data(event);
    lv_label_set_text(label, "Ready for ESP32-S3!");
}

void ui_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080A0F), 0);

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, LV_PCT(86), LV_PCT(72));
    lv_obj_center(panel);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 24, 0);
    lv_obj_set_style_radius(panel, 28, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x151A26), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x7C5CFC), 0);
    lv_obj_set_style_border_width(panel, 2, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Tiny Guy");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t *status = lv_label_create(panel);
    lv_label_set_text(status, "LVGL 8.3 desktop simulator");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(0xA9B1C7), 0);

    lv_obj_t *button = lv_btn_create(panel);
    lv_obj_set_size(button, 210, 58);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x7C5CFC), 0);
    lv_obj_add_event_cb(button, button_clicked, LV_EVENT_CLICKED, status);

    lv_obj_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, "Test touch");
    lv_obj_set_style_text_font(button_label, &lv_font_montserrat_18, 0);
    lv_obj_center(button_label);
}

