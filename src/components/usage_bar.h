#ifndef TINY_GUY_USAGE_BAR_H
#define TINY_GUY_USAGE_BAR_H

#include "lvgl.h"

/* Usage is the percentage remaining, from 0 to 100. The displayed value is
 * rounded and the filled width represents that remaining amount. */
lv_obj_t *usage_bar_create(lv_obj_t *parent, lv_color_t color,
                           const char *label, double usage);
void usage_bar_set_usage(lv_obj_t *bar, double usage);

#endif
