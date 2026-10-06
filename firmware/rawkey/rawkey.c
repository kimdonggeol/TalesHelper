/* rawkey - types the keys it is handed over raw HID.
 *
 * Press and release are separate commands because tap_code16 leaves
 * TAP_CODE_DELAY between them, which is 0 in a good many builds: a game that
 * reads input once a frame sees nothing at all. How long to hold is the PC's
 * business.
 *
 * Installing: RAW_ENABLE = yes in rules.mk (Vial already has it), and
 * SRC += rawkey.c. If something already owns the receiving function, define
 * RAWKEY_NO_HOOK in config.h and call rawkey_receive() from it.
 */

#include QMK_KEYBOARD_H
#include "raw_hid.h"
#include "rawkey.h"
#include "action_util.h"
#include "host.h"
#include <string.h>
#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
#    include "usb_device_state.h"
#endif

/* Older QMK, which is what vial-qmk still is, has no RAW_EPSIZE in
 * raw_hid.h. */
#ifndef RAW_EPSIZE
#    define RAW_EPSIZE 32
#endif

/* What is being held. If the PC dies without sending the release, the next
 * press clears it. There is room for several because some keys have to go
 * down together - sent one at a time, each press drops the one before it and
 * only the last survives. */
static uint16_t rawkey_held[RAWKEY_HELD_MAX];
static bool     rawkey_held_apart[RAWKEY_HELD_MAX]; /* sent on the other device */
static uint8_t  rawkey_held_count = 0;

/* Which keyboard device to type on.
 *
 * Built with NKRO, the keyboard offers the host two devices: the 6KRO (boot)
 * keyboard and the NKRO one, and the keys from the hand only ever come out of
 * one of them. Sending ours out the idle one makes them, to the host, keys
 * from a different keyboard. Windows runs the auto-repeat per device, so ours
 * does not take the repeat away from the key the hand is holding - nothing to
 * hand back, and no release of that key for anyone to see.
 *
 * Under the boot protocol (a BIOS, say) there is no NKRO device, so we type
 * the same way the hand does. */
enum { RAWKEY_VIA_SAME, RAWKEY_VIA_6KRO, RAWKEY_VIA_NKRO };
static uint8_t rawkey_via = RAWKEY_VIA_SAME;

/* Room for the keyboard's own code to send a key of the hand's out the other
 * device (rawkey_apart_press). It rides in the same report as the PC's keys. */
static uint8_t rawkey_extra[RAWKEY_HELD_MAX];
static uint8_t rawkey_extra_count = 0;

static uint8_t rawkey_pick_via(void) {
#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS) && !defined(RAWKEY_SAME_DEVICE)
    if (usb_device_state_get_protocol() != USB_PROTOCOL_REPORT) {
        return RAWKEY_VIA_SAME;
    }
    return keymap_config.nkro ? RAWKEY_VIA_6KRO : RAWKEY_VIA_NKRO;
#else
    return RAWKEY_VIA_SAME;
#endif
}

/* Whether the key can go out on its own. Only basic keycodes, modifiers, and
 * a basic keycode with modifiers on it can be written into a report
 * directly. Everything else is typed the way the hand types. */
static bool rawkey_plain(uint16_t code) {
    if (IS_QK_BASIC(code)) {
        return IS_BASIC_KEYCODE(code) || IS_MODIFIER_KEYCODE(code);
    }
    return IS_QK_MODS(code) && IS_BASIC_KEYCODE(QK_MODS_GET_BASIC_KEYCODE(code));
}

#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
/* Builds that device's report from scratch out of the keys sent apart. There
 * are never more than ten, so rebuilding every time is the simpler thing. */
static void rawkey_send_apart(void) {
    uint8_t mods = 0;
    uint8_t keys[RAWKEY_HELD_MAX * 2];
    uint8_t count = 0;

    for (uint8_t i = 0; i < rawkey_extra_count; i++) {
        keys[count++] = rawkey_extra[i];
    }
    for (uint8_t i = 0; i < rawkey_held_count; i++) {
        if (!rawkey_held_apart[i]) {
            continue;
        }
        uint16_t code = rawkey_held[i];
        if (IS_QK_MODS(code)) {
            uint8_t m = QK_MODS_GET_MODS(code);
            mods |= (m & 0x10) ? (uint8_t)((m & 0x0F) << 4) : (m & 0x0F);
            code = QK_MODS_GET_BASIC_KEYCODE(code);
        }
        if (IS_MODIFIER_KEYCODE(code)) {
            mods |= MOD_BIT(code);
        } else {
            keys[count++] = (uint8_t)code;
        }
    }

    if (rawkey_via == RAWKEY_VIA_NKRO) {
        static report_nkro_t report;
        memset(&report, 0, sizeof(report));
        report.mods = mods;
        for (uint8_t i = 0; i < count; i++) {
            add_key_bit(&report, keys[i]);
        }
        host_nkro_send(&report);
    } else {
        /* 6KRO holds six. Anything past that is dropped. */
        static report_keyboard_t report;
        memset(&report, 0, sizeof(report));
        report.mods = mods;
        for (uint8_t i = 0; i < count && i < KEYBOARD_REPORT_KEYS; i++) {
            report.keys[i] = keys[i];
        }
        host_keyboard_send(&report);
    }
}
#endif

void rawkey_release(void) {
    bool apart = false;
    while (rawkey_held_count) {
        rawkey_held_count--;
        if (rawkey_held_apart[rawkey_held_count]) {
            apart = true;
        } else {
            unregister_code16(rawkey_held[rawkey_held_count]);
        }
    }
#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
    if (apart) {
        rawkey_send_apart();
    }
#else
    (void)apart;
#endif
}

static void rawkey_hold(uint16_t code) {
    if (code == KC_NO || rawkey_held_count >= RAWKEY_HELD_MAX) {
        return;
    }
    if (rawkey_held_count == 0 && rawkey_extra_count == 0) {
        rawkey_via = rawkey_pick_via();
    }
    bool apart = rawkey_via != RAWKEY_VIA_SAME && rawkey_plain(code);
    rawkey_held[rawkey_held_count]       = code;
    rawkey_held_apart[rawkey_held_count] = apart;
    rawkey_held_count++;
#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
    if (apart) {
        rawkey_send_apart();
        return;
    }
#endif
    register_code16(code);
}

bool rawkey_apart_ready(void) {
    return rawkey_pick_via() != RAWKEY_VIA_SAME;
}

void rawkey_apart_press(uint8_t key) {
#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
    if (rawkey_extra_count >= RAWKEY_HELD_MAX) {
        return;
    }
    if (rawkey_held_count == 0 && rawkey_extra_count == 0) {
        rawkey_via = rawkey_pick_via();
    }
    rawkey_extra[rawkey_extra_count++] = key;
    rawkey_send_apart();
#else
    (void)key;
#endif
}

void rawkey_apart_release(uint8_t key) {
#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
    for (uint8_t i = 0; i < rawkey_extra_count; i++) {
        if (rawkey_extra[i] == key) {
            for (; i + 1 < rawkey_extra_count; i++) {
                rawkey_extra[i] = rawkey_extra[i + 1];
            }
            rawkey_extra_count--;
            rawkey_send_apart();
            return;
        }
    }
#else
    (void)key;
#endif
}

/* Letting go of our key kills the auto-repeat on the key the hand is holding.
 *
 * Windows gives the repeat to the most recently pressed key and to no other.
 * The key we sent takes it, and letting go stops the repeat rather than
 * handing it back to the key still held down. Whatever was going out while
 * that key was held stops there.
 *
 * Getting it back means making a fresh press of that key: take it out of the
 * report and put it straight back, and the host reads that as newly pressed
 * and follows with the repeat. Which keys are held is something the report
 * already knows, so the PC need not say. We look after letting go of our own,
 * so what is left in there is only what the hand is holding. */
void rawkey_restore_repeat(void) {
    uint8_t held[RAWKEY_HELD_MAX];
    uint8_t count = 0;

#if defined(NKRO_ENABLE)
    /* This has to match where add_key() puts a key, or we read the wrong
     * report. Newer QMK, vial-qmk included, keeps NKRO in its own
     * nkro_report; older QMK keeps it in keyboard_report's nkro. */
#    if defined(NKRO_REPORT_BITS)
    if (usb_device_state_get_protocol() == USB_PROTOCOL_REPORT && keymap_config.nkro) {
        uint8_t *bits = nkro_report->bits;
        uint16_t nbits = NKRO_REPORT_BITS * 8;
#    else
    if (keyboard_protocol && keymap_config.nkro) {
        uint8_t *bits = keyboard_report->nkro.bits;
        uint16_t nbits = KEYBOARD_REPORT_BITS * 8;
#    endif
        for (uint16_t bit = 0; bit < nbits; bit++) {
            if (count >= RAWKEY_HELD_MAX) {
                break;
            }
            if (bits[bit / 8] & (1 << (bit % 8))) {
                held[count++] = (uint8_t)bit;
            }
        }
    } else
#endif
    {
        for (uint8_t i = 0; i < KEYBOARD_REPORT_KEYS; i++) {
            if (count >= RAWKEY_HELD_MAX) {
                break;
            }
            if (keyboard_report->keys[i]) {
                held[count++] = keyboard_report->keys[i];
            }
        }
    }
    if (!count) {
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        del_key(held[i]);
    }
    send_keyboard_report();
    for (uint8_t i = 0; i < count; i++) {
        add_key(held[i]);
    }
    send_keyboard_report();
}

bool rawkey_receive(uint8_t *data, uint8_t length) {
    if (length < 3) {
        return false;
    }
    uint16_t code = data[1] | ((uint16_t)data[2] << 8);

    switch (data[0]) {
        case RAWKEY_PRESS:
            rawkey_release();
            rawkey_hold(code);
            return true;

        case RAWKEY_CHORD: {
            /* data[1] is the count, and the keycodes follow two bytes each.
             * They all have to go down inside one report to reach the host
             * together. */
            uint8_t count = data[1];
            if (2 + count * 2 > length) {
                return true;
            }
            rawkey_release();
            for (uint8_t i = 0; i < count; i++) {
                rawkey_hold(data[2 + i * 2] | ((uint16_t)data[3 + i * 2] << 8));
            }
            return true;
        }

        case RAWKEY_RELEASE:
            /* We only ever hold what we pressed, so picking one out and
             * dropping the lot come to the same thing. */
            rawkey_release();
#ifdef RAWKEY_RESTORE_REPEAT
            /* Off by default. Handing the repeat back shows the host that
             * key going up once, and a channelled skill - one that lasts
             * only while the key is held - ends right there. Typing on the
             * other device (v4) never takes the repeat, so there is nothing
             * to hand back. This stays for the builds that can only type the
             * one way. */
            rawkey_restore_repeat();
#endif
            return true;

        case RAWKEY_PING: {
            uint8_t reply[RAW_EPSIZE] = {0};
            reply[0] = RAWKEY_PING;
            reply[1] = RAWKEY_VERSION;
            /* Which device the next key goes out on: 0 the same one, 1 6KRO, 2 NKRO */
            reply[2] = (rawkey_held_count || rawkey_extra_count) ? rawkey_via : rawkey_pick_via();
            raw_hid_send(reply, sizeof(reply));
            return true;
        }
    }
    return false;
}

#ifndef RAWKEY_NO_HOOK
#    ifdef VIA_ENABLE
/* Where VIA and Vial hand over a command they do not know. If it is not ours
 * either, mark it unhandled again and send it back as they would. */
#        include "via.h"
void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (!rawkey_receive(data, length)) {
        data[0] = id_unhandled;
    }
}
#    else
/* Without VIA or Vial, every raw HID report arrives here. */
void raw_hid_receive(uint8_t *data, uint8_t length) {
    rawkey_receive(data, length);
}
#    endif
#endif
