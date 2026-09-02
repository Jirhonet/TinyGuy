#include "pages/agent_page.h"
#include "components/page_shell.h"
#include "services/agent_status.h"

#ifndef AGENT_SLEEP_TIMEOUT_MS
#define AGENT_SLEEP_TIMEOUT_MS (3U * 60U * 1000U)
#endif

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
    PEN_WIGGLE_DEGREES = 3,
    PEN_WIGGLE_PERIOD_MS = 180,
    FALL_ASLEEP_DURATION = 3500,
    DROWSY_EYE_LENGTH = 22,
    WAKE_BLINK_CLOSE_DURATION = 160,
    WAKE_OPEN_DURATION = 110,
    WAKE_HOLD_DURATION = 220,
    WAKE_SETTLE_DURATION = 420,
    SHOCKED_EYE_LENGTH = 84,
    EYE_DRAW_WIDTH = 80,
    EYE_DRAW_HEIGHT = 112,
};

typedef enum {
    FACE_IDLE,
    FACE_FALLING_ASLEEP,
    FACE_SLEEPING,
    FACE_WAKING,
} face_state_t;

typedef struct {
    lv_obj_t *surface;
    lv_obj_t *left_eye;
    lv_obj_t *right_eye;
    void *left_eye_buffer;
    void *right_eye_buffer;
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
    face_state_t state;
    uint32_t last_activity;
    uint32_t state_started;
    uint8_t sleep_blinks_remaining;
    bool sleep_blinking;
    uint8_t wake_phase;
    bool work_after_waking;
} agent_face_t;

static const uint16_t sleep_close_durations[] = {900, 1400, 2400};

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

static void update_eye_objects(agent_face_t *face)
{
    enum { CLOSED_EYE_HEIGHT = 10, CLOSED_EYE_RADIUS = 3 };
    int32_t height = face->length + EYE_WIDTH;
    int32_t radius = EYE_WIDTH / 2;

    if (face->length < DROWSY_EYE_LENGTH) {
        /* Reduce the corner radius as the rectangle gets shorter so the
           closed eye becomes a thin rounded bar instead of a circle. */
        int32_t range = DROWSY_EYE_LENGTH - EYE_BLINK_LENGTH;
        int32_t progress =
            (DROWSY_EYE_LENGTH - face->length) * 1000 / range;
        if (progress > 1000) progress = 1000;

        height = height + (CLOSED_EYE_HEIGHT - height) * progress / 1000;
        radius = EYE_WIDTH / 2 +
                 (CLOSED_EYE_RADIUS - EYE_WIDTH / 2) * progress / 1000;
    }
    lv_obj_t *eyes[] = {face->left_eye, face->right_eye};
    for (uint8_t i = 0; i < 2; i++) {
        lv_canvas_fill_bg(eyes[i], lv_color_black(), LV_OPA_TRANSP);
        lv_draw_rect_dsc_t eye;
        lv_draw_rect_dsc_init(&eye);
        eye.bg_color = lv_color_white();
        eye.bg_opa = LV_OPA_COVER;
        eye.radius = radius;
        lv_canvas_draw_rect(eyes[i],
                            (EYE_DRAW_WIDTH - EYE_WIDTH) / 2,
                            (EYE_DRAW_HEIGHT - height) / 2,
                            EYE_WIDTH, height, &eye);
        lv_img_set_pivot(eyes[i], EYE_DRAW_WIDTH / 2, EYE_DRAW_HEIGHT / 2);
        lv_img_set_angle(eyes[i], face->angle * 10);
        lv_obj_align(eyes[i], LV_ALIGN_CENTER,
                     face->x + (i == 0 ? -EYE_SPACING : EYE_SPACING),
                     face->y);
    }
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

static void draw_pen(lv_draw_ctx_t *draw_context, lv_coord_t tip_x,
                     lv_coord_t tip_y, int32_t free_end_angle)
{
    lv_draw_line_dsc_t pen;
    lv_draw_line_dsc_init(&pen);
    pen.color = lv_color_white();
    pen.opa = LV_OPA_COVER;
    pen.width = 6;
    pen.round_start = true;
    pen.round_end = true;

    /* Keep the writing tip fixed and rotate only the free end around it. */
    int32_t normalized_angle = (free_end_angle + 360) % 360;
    int32_t sine = lv_trigo_sin(normalized_angle);
    int32_t cosine = lv_trigo_sin((normalized_angle + 90) % 360);
    lv_point_t writing_tip = {tip_x, tip_y};
    lv_point_t free_end = {
        tip_x + ((19 * cosine + 24 * sine) >> LV_TRIGO_SHIFT),
        tip_y + ((19 * sine - 24 * cosine) >> LV_TRIGO_SHIFT),
    };
    lv_draw_line(draw_context, &pen, &writing_tip, &free_end);
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
    if (face->running) {
        int32_t pen_angle = 0;
        if (!face->returning_to_margin) {
            int32_t phase = (lv_tick_get() % PEN_WIGGLE_PERIOD_MS) * 360 /
                            PEN_WIGGLE_PERIOD_MS;
            pen_angle = PEN_WIGGLE_DEGREES * lv_trigo_sin(phase) >>
                        LV_TRIGO_SHIFT;
        }
        draw_pen(draw_context, center_x, center_y + INK_OFFSET_Y, pen_angle);
    }

}

static void wake_up(agent_face_t *face, uint32_t now)
{
    face->state = FACE_WAKING;
    face->state_started = now;
    face->eyes_closed = false;
    face->wake_phase = 0;
    /* Reverse the final falling-asleep close first: gradually lift the eyes
       from fully closed back to their drowsy shape. */
    begin_animation(face, 0, 30, 0, DROWSY_EYE_LENGTH,
                    sleep_close_durations[2]);
}

static void start_working(agent_face_t *face, uint32_t now)
{
    face->running = true;
    face->work_after_waking = false;
    face->state = FACE_IDLE;
    face->writing_line = 0;
    face->returning_to_margin = true;
    begin_animation(face, WRITING_LEFT, WRITING_TOP, -6,
                    EYE_LENGTH, 250);
    face->next_action = now + 300;
}

static void face_tapped(lv_event_t *event)
{
    agent_face_t *face = lv_event_get_user_data(event);
    uint32_t now = lv_tick_get();
    face->last_activity = now;
    if (!face->running && (face->state == FACE_FALLING_ASLEEP ||
                           face->state == FACE_SLEEPING))
        wake_up(face, now);
}

static void face_tick(lv_timer_t *timer)
{
    agent_face_t *face = timer->user_data;
    uint32_t now = lv_tick_get();
    bool running = agent_is_running();

    if (running && !face->running && !face->work_after_waking) {
        face->eyes_closed = false;
        face->last_activity = now;
        if (face->state == FACE_FALLING_ASLEEP ||
            face->state == FACE_SLEEPING) {
            face->work_after_waking = true;
            wake_up(face, now);
        } else {
            start_working(face, now);
        }
    } else if (!running && (face->running || face->work_after_waking)) {
        face->running = false;
        face->work_after_waking = false;
        face->eyes_closed = false;
        face->last_activity = now;
        face->state = FACE_IDLE;
        face->returning_to_margin = false;
        begin_animation(face, 0, 0, 0, EYE_LENGTH, 250);
        face->next_action = now + 400;
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
    update_eye_objects(face);

    if (!face->running && face->state == FACE_IDLE &&
        now - face->last_activity >= AGENT_SLEEP_TIMEOUT_MS) {
        face->state = FACE_FALLING_ASLEEP;
        face->state_started = now;
        face->sleep_blinks_remaining = 3;
        face->sleep_blinking = false;
        face->eyes_closed = false;
        begin_animation(face, 0, 30, 0, DROWSY_EYE_LENGTH,
                        FALL_ASLEEP_DURATION);
    }

    if (face->state == FACE_FALLING_ASLEEP) {
        if (!face->sleep_blinking &&
            now - face->state_started >= FALL_ASLEEP_DURATION) {
            face->sleep_blinking = true;
            face->sleep_blinks_remaining--;
            face->eyes_closed = true;
            uint32_t close_duration = sleep_close_durations[0];
            begin_animation(face, face->x, face->y, face->angle,
                            EYE_BLINK_LENGTH, close_duration);
            face->next_action = now + close_duration;
        } else if (face->sleep_blinking &&
            (int32_t)(now - face->next_action) >= 0) {
            if (face->eyes_closed) {
                if (face->sleep_blinks_remaining == 0) {
                    /* The slowest close is final: stay shut and begin the
                       gentle sleeping movement without reopening. */
                    face->state = FACE_SLEEPING;
                    face->state_started = now;
                    begin_animation(face, 0, 40, 0,
                                    EYE_BLINK_LENGTH, 1200);
                } else {
                    face->eyes_closed = false;
                    begin_animation(face, face->x, face->y, face->angle,
                                    DROWSY_EYE_LENGTH, 160);
                    face->next_action = now + 320;
                }
            } else {
                uint8_t close_index = 3 - face->sleep_blinks_remaining;
                uint32_t close_duration =
                    sleep_close_durations[close_index];
                face->sleep_blinks_remaining--;
                face->eyes_closed = true;
                begin_animation(face, face->x, face->y, face->angle,
                                EYE_BLINK_LENGTH, close_duration);
                face->next_action = now + close_duration;
            }
        }
        lv_obj_invalidate(face->surface);
        return;
    }

    if (face->state == FACE_SLEEPING) {
        if (elapsed >= face->animation_duration) {
            lv_coord_t next_y = face->target_y == 40 ? 34 : 40;
            begin_animation(face, 0, next_y, 0, EYE_BLINK_LENGTH, 1200);
        }
        lv_obj_invalidate(face->surface);
        return;
    }

    if (face->state == FACE_WAKING) {
        if (elapsed >= face->animation_duration) {
            face->wake_phase++;
            switch (face->wake_phase) {
            case 1: /* Close quickly, reversing the sleep-time reopening. */
                begin_animation(face, 0, 34, 0, EYE_BLINK_LENGTH,
                                WAKE_BLINK_CLOSE_DURATION);
                break;
            case 2: /* One slow blink opens, mirroring the 1400 ms close. */
                begin_animation(face, 0, 30, 0, DROWSY_EYE_LENGTH,
                                sleep_close_durations[1]);
                break;
            case 3: /* Snap into the surprised expression. */
                begin_animation(face, 0, -28, 0, SHOCKED_EYE_LENGTH,
                                WAKE_OPEN_DURATION);
                break;
            case 4: /* Hold the shocked eyes for a beat. */
                begin_animation(face, 0, -28, 0, SHOCKED_EYE_LENGTH,
                                WAKE_HOLD_DURATION);
                break;
            case 5:
                begin_animation(face, 0, -18, 0, EYE_LENGTH,
                                WAKE_SETTLE_DURATION);
                break;
            default:
                if (face->work_after_waking && running) {
                    start_working(face, now);
                } else {
                    face->work_after_waking = false;
                    face->state = FACE_IDLE;
                    begin_animation(face, 0, 0, 0, EYE_LENGTH, 280);
                    face->next_action = now + 650;
                }
                break;
            }
        }
        lv_obj_invalidate(face->surface);
        return;
    }

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
        .last_activity = lv_tick_get(),
    };

    lv_obj_t **eyes[] = {&face->left_eye, &face->right_eye};
    void **buffers[] = {&face->left_eye_buffer, &face->right_eye_buffer};
    for (uint8_t i = 0; i < 2; i++) {
        *buffers[i] = lv_mem_alloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(
            EYE_DRAW_WIDTH, EYE_DRAW_HEIGHT));
        LV_ASSERT_MALLOC(*buffers[i]);
        *eyes[i] = lv_canvas_create(surface);
        lv_canvas_set_buffer(*eyes[i], *buffers[i], EYE_DRAW_WIDTH,
                             EYE_DRAW_HEIGHT, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_obj_clear_flag(*eyes[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    }
    update_eye_objects(face);

    lv_obj_add_event_cb(surface, draw_face, LV_EVENT_DRAW_MAIN, face);
    lv_obj_add_event_cb(surface, face_tapped, LV_EVENT_CLICKED, face);
    lv_obj_add_flag(surface, LV_OBJ_FLAG_CLICKABLE);
    lv_timer_create(face_tick, 30, face);
    return page;
}
