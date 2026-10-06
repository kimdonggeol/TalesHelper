/* rawkey - the PC says "press this / let go" over raw HID and the keyboard
 * does the typing. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Command ids, chosen where VIA and Vial do not reach. If something else
 * wants them, change them at build time - and change the PC side to match. */
#ifndef RAWKEY_PRESS
#    define RAWKEY_PRESS 0x40
#endif
#ifndef RAWKEY_RELEASE
#    define RAWKEY_RELEASE 0x41
#endif
#ifndef RAWKEY_PING
#    define RAWKEY_PING 0x42
#endif
#ifndef RAWKEY_CHORD
#    define RAWKEY_CHORD 0x43
#endif

/* How many keys can be held at once. As many as the report carries is
 * plenty. */
#ifndef RAWKEY_HELD_MAX
#    define RAWKEY_HELD_MAX 10
#endif

/* Protocol version, sent as the second byte of the answer to a ping. The
 * third byte says which device the keys go out on (0 the same one, 1 6KRO,
 * 2 NKRO).
 * v4: types on whichever keyboard device the hand is not using. */
#define RAWKEY_VERSION 0x04

/* Handles the report and returns true if it was a rawkey command; returns
 * false and leaves it alone otherwise. */
bool rawkey_receive(uint8_t *data, uint8_t length);

/* Lets go of everything being held. Only of what the PC asked for. */
void rawkey_release(void);

/* Lets the keyboard's own code press and release one basic key on the
 * device the hand is not using. It rides in the same report as the keys the
 * PC sent. Where there is no such device - boot protocol, or a build without
 * NKRO - rawkey_apart_ready() is false. */
bool rawkey_apart_ready(void);
void rawkey_apart_press(uint8_t key);
void rawkey_apart_release(uint8_t key);

/* Hands the auto-repeat back to the key the hand is still holding. The host
 * sees that key go up once, which cuts a channelled skill short, so this is
 * only called on release when RAWKEY_RESTORE_REPEAT is defined. Typing on
 * the other device, as v4 does, needs none of it. */
void rawkey_restore_repeat(void);
