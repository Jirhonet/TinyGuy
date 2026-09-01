#ifndef TINY_GUY_PAGE_SHELL_H
#define TINY_GUY_PAGE_SHELL_H

#include "lvgl.h"

lv_obj_t *page_shell_create(lv_obj_t *parent, lv_color_t background,
                            lv_coord_t row_gap);

#endif
