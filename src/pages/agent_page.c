#include "pages/agent_page.h"
#include "components/page_shell.h"
#include "services/agent_status.h"

enum {
    DISPLAY_WIDTH = 368,
    DISPLAY_HEIGHT = 448,
    EYE_SPACING = 46,
    EYE_LENGTH = 72,
    EYE_BLINK_LENGTH = 3,
    EYE_WIDTH = 28,
    WRITING_LEFT = -58,
    WRITING_RIGHT = 58,
    WRITING_TOP = -10,
    WRITING_LINE_HEIGHT = 27,
    WRITING_LINE_COUNT = 4,
    INK_OFFSET_Y = 110,
};

typedef struct {
    lv_obj_t *surface;
    int32_t x;
    int32_t y;
    int32_t angle;
    int32_t length;
    int32_t start_x;
    int32_t start_y;
    int32_t start_angle;
    int32_t start_length;
    int32_t target_x;
    int32_t target_y;
    int32_t target_angle;
    int32_t target_length;
    uint32_t animation_started;
    uint32_t animation_duration;
    uint32_t next_action;
    bool eyes_closed;
    bool returning_to_margin;
    bool running;
    uint8_t writing_line;
} agent_face_t;

static int32_t interpolate(int32_t from, int32_t to, uint32_t elapsed,
                           uint32_t duration)
{
    if (elapsed >= duration) {
        return to;
    }

    /* Smoothstep easing, calculated as fixed-point thousandths. */
    int32_t t = (int32_t)(elapsed * 1000 / duration);
    int32_t eased = t * t / 1000 * (3000 - 2 * t) / 1000;
    return from + (to - from) * eased / 1000;
}

static void begin_animation(agent_face_t *face, int32_t x, int32_t y,
                            int32_t angle, int32_t length, uint32_t duration)
{
    face->start_x = face->x;
    face->start_y = face->y;
    face->start_angle = face->angle;
    face->start_length = face->length;
    face->target_x = x;
    face->target_y = y;
    face->target_angle = angle;
    face->target_length = length;
    face->animation_started = lv_tick_get();
    face->animation_duration = duration;
}

static void draw_eye(lv_draw_ctx_t *draw_context, lv_coord_t center_x,
                     lv_coord_t center_y, int32_t angle, int32_t length)
{
    int32_t half = length / 2;
    int32_t dx = half * lv_trigo_sin(angle) >> LV_TRIGO_SHIFT;
    int32_t dy = half * lv_trigo_cos(angle) >> LV_TRIGO_SHIFT;
    lv_point_t start = {center_x - dx, center_y - dy};
    lv_point_t end = {center_x + dx, center_y + dy};

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_white();
    line.opa = LV_OPA_COVER;
    line.width = EYE_WIDTH;
    line.round_start = true;
    line.round_end = true;
    lv_draw_line(draw_context, &line, &start, &end);
}

static void draw_squiggle(lv_draw_ctx_t *draw_context, lv_coord_t center_x,
                          lv_coord_t y, int32_t end_x)
{
    lv_draw_line_dsc_t ink;
    lv_draw_line_dsc_init(&ink);
    ink.color = lv_color_white();
    ink.opa = LV_OPA_COVER;
    ink.width = 4;
    ink.round_start = true;
    ink.round_end = true;

    int32_t x = WRITING_LEFT;
    while (x < end_x) {
        int32_t next_x = x + 6;
        if (next_x > end_x) next_x = end_x;
        int32_t start_angle = (x - WRITING_LEFT) * 10;
        int32_t end_angle = (next_x - WRITING_LEFT) * 10;
        lv_point_t start = {
            center_x + x,
            y + (7 * lv_trigo_sin(start_angle) >> LV_TRIGO_SHIFT)
        };
        lv_point_t end = {
            center_x + next_x,
            y + (7 * lv_trigo_sin(end_angle) >> LV_TRIGO_SHIFT)
        };
        lv_draw_line(draw_context, &ink, &start, &end);
        x = next_x;
    }
}

static void draw_pen(lv_draw_ctx_t *draw_context, lv_coord_t x, lv_coord_t y)
{
    lv_draw_line_dsc_t pen;
    lv_draw_line_dsc_init(&pen);
    pen.color = lv_color_white();
    pen.opa = LV_OPA_COVER;
    pen.width = 6;
    pen.round_start = true;
    pen.round_end = true;

    lv_point_t nib = {x, y};
    lv_point_t end = {x + 19, y - 24};
    lv_draw_line(draw_context, &pen, &nib, &end);
}

static void draw_face(lv_event_t *event)
{
    agent_face_t *face = lv_event_get_user_data(event);
    lv_draw_ctx_t *draw_context = lv_event_get_draw_ctx(event);
    lv_area_t coordinates;
    lv_obj_get_coords(face->surface, &coordinates);

    lv_coord_t center_x = coordinates.x1 + DISPLAY_WIDTH / 2 + face->x;
    lv_coord_t center_y = coordinates.y1 + DISPLAY_HEIGHT / 2 + face->y;

    lv_coord_t page_center_x = coordinates.x1 + DISPLAY_WIDTH / 2;
    lv_coord_t page_center_y = coordinates.y1 + DISPLAY_HEIGHT / 2;
    if (face->running && !face->returning_to_margin) {
        draw_squiggle(draw_context, page_center_x,
                      page_center_y + WRITING_TOP +
                          face->writing_line * WRITING_LINE_HEIGHT + INK_OFFSET_Y,
                      face->x);
    }
    if (face->running)
        draw_pen(draw_context, center_x, center_y + INK_OFFSET_Y);

    draw_eye(draw_context, center_x - EYE_SPACING, center_y,
             face->angle, face->length);
    draw_eye(draw_context, center_x + EYE_SPACING, center_y,
             face->angle, face->length);
}

static void face_tick(lv_timer_t *timer)
{
    agent_face_t *face = timer->user_data;
    uint32_t now = lv_tick_get();
    bool running = agent_is_running();

    if (running != face->running) {
        face->running = running;
        face->eyes_closed = false;
        if (running) {
            face->writing_line = 0;
            face->returning_to_margin = true;
            begin_animation(face, WRITING_LEFT, WRITING_TOP, -6,
                            EYE_LENGTH, 250);
            face->next_action = now + 300;
        } else {
            face->returning_to_margin = false;
            begin_animation(face, 0, 0, 0, EYE_LENGTH, 250);
            face->next_action = now + 400;
        }
    }

    uint32_t elapsed = now - face->animation_started;

    face->x = interpolate(face->start_x, face->target_x, elapsed,
                          face->animation_duration);
    face->y = interpolate(face->start_y, face->target_y, elapsed,
                          face->animation_duration);
    face->angle = interpolate(face->start_angle, face->target_angle, elapsed,
                              face->animation_duration);
    face->length = interpolate(face->start_length, face->target_length, elapsed,
                               face->animation_duration);

    if ((int32_t)(now - face->next_action) >= 0) {
        if (!face->running) {
            if (face->eyes_closed) {
                begin_animation(face, face->x, face->y, face->angle,
                                EYE_LENGTH, 120);
                face->eyes_closed = false;
                face->next_action = now + lv_rand(900, 2100);
            } else if (lv_rand(0, 99) < 35) {
                begin_animation(face, face->x, face->y, face->angle,
                                EYE_BLINK_LENGTH, 80);
                face->eyes_closed = true;
                face->next_action = now + 105;
            } else {
                /* Look around while waiting for the next Codex turn. */
                begin_animation(face, lv_rand(-62, 62), lv_rand(-120, 120),
                                lv_rand(-25, 25), EYE_LENGTH,
                                lv_rand(480, 820));
                face->next_action = now + lv_rand(1000, 2200);
            }
        } else if (face->eyes_closed) {
            begin_animation(face, face->x, face->y, face->angle,
                            EYE_LENGTH, 120);
            face->eyes_closed = false;
            face->next_action = now + 140;
        } else if (face->returning_to_margin) {
            /* Follow the next handwritten line from left to right. */
            begin_animation(face, WRITING_RIGHT,
                            WRITING_TOP + face->writing_line * WRITING_LINE_HEIGHT,
                            4, EYE_LENGTH, lv_rand(1250, 1550));
            face->returning_to_margin = false;
            face->next_action = now + face->animation_duration;
        } else if (lv_rand(0, 99) < 28) {
            /* Blink briefly at the end of a line, like a natural pause. */
            begin_animation(face, face->x, face->y, face->angle,
                            EYE_BLINK_LENGTH, 80);
            face->eyes_closed = true;
            face->next_action = now + 105;
        } else {
            /* Return quickly to the left margin and drop down a line. */
            face->writing_line = (face->writing_line + 1) % WRITING_LINE_COUNT;
            begin_animation(face, WRITING_LEFT,
                            WRITING_TOP + face->writing_line * WRITING_LINE_HEIGHT,
                            -6, EYE_LENGTH, 230);
            face->returning_to_margin = true;
            face->next_action = now + face->animation_duration;
        }
    }

    lv_obj_invalidate(face->surface);
}

lv_obj_t *agent_page_create(lv_obj_t *parent)
{
    agent_status_start();
    lv_obj_t *content = page_shell_create(parent, lv_color_black(), 0);
    lv_obj_t *page = lv_obj_get_parent(content);

    lv_obj_t *surface = lv_obj_create(content);
    lv_obj_remove_style_all(surface);
    lv_obj_set_size(surface, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(surface, LV_OBJ_FLAG_SCROLLABLE);

    agent_face_t *face = lv_mem_alloc(sizeof(*face));
    LV_ASSERT_MALLOC(face);
    *face = (agent_face_t) {
        .surface = surface,
        .length = EYE_LENGTH,
        .start_length = EYE_LENGTH,
        .target_length = EYE_LENGTH,
        .animation_duration = 1,
    };

    lv_obj_add_event_cb(surface, draw_face, LV_EVENT_DRAW_MAIN, face);
    lv_timer_create(face_tick, 30, face);
    return page;
}
