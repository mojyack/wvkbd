#ifndef __SWIPE_H
#define __SWIPE_H

#include <stdint.h>

struct kbd;

/* Swipe gesture layer.
 *
 * When --swipe-arrows is enabled this sits between the Wayland input events and
 * the keyboard: a touch/click is cached instead of being forwarded to the
 * keyboard right away. If it turns into a swipe, arrow keys are emitted directly
 * and the keyboard never sees the press.
 * Only when the press is held still past --swipe-hold, or released as a quick
 * tap, is it handed to the keyboard as a normal key press.
 */

void swipe_init(struct kbd *kb, int timer_fd);
void swipe_down(int x, int y, uint32_t time);
void swipe_motion(int x, int y, uint32_t time);
void swipe_up(uint32_t time);
void swipe_hold(void);

#endif
