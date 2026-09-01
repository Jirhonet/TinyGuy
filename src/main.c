#include <SDL2/SDL.h>
#include "lvgl.h"
#include "sdl/sdl.h"
#include "ui.h"

int main(void)
{
    lv_init();
    sdl_init();

    static lv_disp_draw_buf_t draw_buffer;
    static lv_color_t pixels[SDL_HOR_RES * 80];
    lv_disp_draw_buf_init(&draw_buffer, pixels, NULL, SDL_HOR_RES * 80);

    static lv_disp_drv_t display_driver;
    lv_disp_drv_init(&display_driver);
    display_driver.draw_buf = &draw_buffer;
    display_driver.flush_cb = sdl_display_flush;
    display_driver.hor_res = SDL_HOR_RES;
    display_driver.ver_res = SDL_VER_RES;
    lv_disp_drv_register(&display_driver);

    static lv_indev_drv_t pointer_driver;
    lv_indev_drv_init(&pointer_driver);
    pointer_driver.type = LV_INDEV_TYPE_POINTER;
    pointer_driver.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&pointer_driver);

    ui_create();

    uint32_t previous = SDL_GetTicks();
    for (;;) {
        const uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - previous);
        previous = now;
        lv_timer_handler();
        SDL_Delay(5);
    }
}

