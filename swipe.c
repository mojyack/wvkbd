#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/timerfd.h>

#include "keyboard.h"
#include "swipe.h"

enum swipe_state {
    SWIPE_NONE = 0, // no active touch
    SWIPE_PENDING,  // touch down, cached, not yet classified
    SWIPE_ACTIVE,   // movement exceeded the step: emitting arrow keys
    SWIPE_COMMITTED // handed to the keyboard as a normal key press
};

static struct kbd *kbd;
static int timer_fd = -1;
static enum swipe_state state = SWIPE_NONE;
static int start_x, start_y; // where the touch went down
static int ref_x, ref_y;     // reference for the next arrow step
static uint32_t last_time;   // most recent event time, for the hold commit

void
swipe_init(struct kbd *kb, int tfd)
{
    kbd = kb;
    timer_fd = tfd;
}

static void
timer_disarm(void)
{
    struct itimerspec its = {0};
    timerfd_settime(timer_fd, 0, &its, NULL);
}

static void
timer_arm(void)
{
    if (kbd->swipe_hold_ms == 0)
        return;
    struct itimerspec its = {0};
    its.it_value.tv_sec = kbd->swipe_hold_ms / 1000;
    its.it_value.tv_nsec = (kbd->swipe_hold_ms % 1000) * 1000000;
    timerfd_settime(timer_fd, 0, &its, NULL);
}

static int
step_px(void)
{
    if (kbd->swipe_step)
        return kbd->swipe_step;
    if (kbd->layout && kbd->layout->keyheight)
        return kbd->layout->keyheight;
    return 50;
}

static void
forward_press(uint32_t time)
{
    struct key *k = kbd_get_key(kbd, start_x, start_y);
    if (k) {
        kbd_press_key(kbd, k, time);
    } else if (kbd->compose) {
        kbd->compose = 0;
        kbd_switch_layout(kbd, kbd->prevlayout, kbd->last_abc_index);
    }
}

void
swipe_down(int x, int y, uint32_t time)
{
    last_time = time;
    state = SWIPE_PENDING;
    start_x = ref_x = x;
    start_y = ref_y = y;
    timer_arm();
}

void
swipe_motion(int x, int y, uint32_t time)
{
    last_time = time;

    if (state == SWIPE_COMMITTED) {
        kbd_motion_key(kbd, time, x, y);
        return;
    }
    if (state != SWIPE_PENDING && state != SWIPE_ACTIVE)
        return;

    int step = step_px();
    int dx = x - ref_x;
    int dy = y - ref_y;
    bool moved = false;
    while (dx >= step) {
        kbd_emit_key(kbd, KEY_RIGHT, time);
        ref_x += step;
        dx -= step;
        moved = true;
    }
    while (dx <= -step) {
        kbd_emit_key(kbd, KEY_LEFT, time);
        ref_x -= step;
        dx += step;
        moved = true;
    }
    while (dy >= step) {
        kbd_emit_key(kbd, KEY_DOWN, time);
        ref_y += step;
        dy -= step;
        moved = true;
    }
    while (dy <= -step) {
        kbd_emit_key(kbd, KEY_UP, time);
        ref_y -= step;
        dy += step;
        moved = true;
    }
    if (moved) {
        state = SWIPE_ACTIVE;
        timer_disarm();
    }
}

void
swipe_up(uint32_t time)
{
    last_time = time;
    timer_disarm();
    switch (state) {
    case SWIPE_PENDING:
        forward_press(time);
        kbd_release_key(kbd, time);
        break;
    case SWIPE_COMMITTED:
        kbd_release_key(kbd, time);
        break;
    case SWIPE_ACTIVE:
    default:
        break;
    }
    state = SWIPE_NONE;
}

void
swipe_hold(void)
{
    if (state != SWIPE_PENDING)
        return;
    struct key *k = kbd_get_key(kbd, start_x, start_y);
    if (k) {
        kbd_press_key(kbd, k, last_time);
        state = SWIPE_COMMITTED;
    }
    // Nothing to press; stay PENDING so a later swipe can still start.
}
