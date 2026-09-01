#ifndef TINY_GUY_PAGE_INDICATOR_H
#define TINY_GUY_PAGE_INDICATOR_H

#include "lvgl.h"

lv_obj_t *page_indicator_create(lv_obj_t *parent, uint8_t selected,
                                uint8_t page_count);

#endif
