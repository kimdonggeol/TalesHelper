/* rawkey - 로우 HID 로 받은 키를 키보드가 직접 칩니다.
 *
 * 누름과 뗌을 따로 둔 이유: tap_code16 은 둘 사이가 TAP_CODE_DELAY 인데
 * 기본값이 0 인 빌드가 많아서, 한 프레임에 한 번 입력을 읽는 게임은 못
 * 보고 지나갑니다. 얼마나 눌러 둘지는 PC 쪽이 정합니다.
 *
 * 설치: rules.mk 에 RAW_ENABLE = yes (Vial 이면 이미 켜짐), SRC += rawkey.c
 * 받는 함수를 이미 쓰고 있다면 config.h 에 RAWKEY_NO_HOOK 을 정의하고
 * 그 함수 안에서 rawkey_receive() 를 부르세요.
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

/* 옛 QMK(지금의 vial-qmk 포함)는 raw_hid.h 에 RAW_EPSIZE 가 없습니다. */
#ifndef RAW_EPSIZE
#    define RAW_EPSIZE 32
#endif

/* 눌러 둔 것들. PC 가 뗌을 못 보내고 죽어도 다음 누름이 앞의 것을
 * 정리합니다. 여럿인 이유는 한 번에 같이 눌러야 하는 경우가 있어서입니다
 * — 하나씩 보내면 앞의 것이 떨어져 마지막 하나만 남습니다. */
static uint16_t rawkey_held[RAWKEY_HELD_MAX];
static bool     rawkey_held_apart[RAWKEY_HELD_MAX]; /* 따로 된 키보드로 보냈는지 */
static uint8_t  rawkey_held_count = 0;

/* 어느 키보드 장치로 칠지.
 *
 * NKRO 를 넣고 빌드하면 키보드가 호스트에 장치를 둘 내놓습니다. 6KRO
 * (부트) 키보드와 NKRO 키보드이고, 손가락이 치는 키는 그중 하나로만
 * 나갑니다. 우리 키를 놀고 있는 다른 하나로 내보내면, 호스트에게는 다른
 * 키보드에서 온 키가 됩니다. 윈도우는 자동 반복을 장치마다 따로 돌리므로
 * 손가락이 누르고 있는 키의 반복을 빼앗지 않습니다 - 되살릴 일도, 그
 * 키의 뗌을 보일 일도 없습니다.
 *
 * 부트 프로토콜(BIOS 등)이면 NKRO 장치가 없으니 같은 길로 칩니다. */
enum { RAWKEY_VIA_SAME, RAWKEY_VIA_6KRO, RAWKEY_VIA_NKRO };
static uint8_t rawkey_via = RAWKEY_VIA_SAME;

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

/* 따로 보낼 수 있는 키인지. 기본 키와 수정자, 그리고 수정자를 얹은 기본
 * 키만 리포트에 직접 적을 수 있습니다. 나머지는 같은 길로 칩니다. */
static bool rawkey_plain(uint16_t code) {
    if (IS_QK_BASIC(code)) {
        return IS_BASIC_KEYCODE(code) || IS_MODIFIER_KEYCODE(code);
    }
    return IS_QK_MODS(code) && IS_BASIC_KEYCODE(QK_MODS_GET_BASIC_KEYCODE(code));
}

#if defined(NKRO_ENABLE) && defined(NKRO_REPORT_BITS)
/* 따로 보낸 키들로 그 장치의 리포트를 처음부터 다시 만들어 보냅니다. 키가
 * 열 개를 넘지 않으니 매번 새로 만드는 쪽이 단순합니다. */
static void rawkey_send_apart(void) {
    uint8_t mods = 0;
    uint8_t keys[RAWKEY_HELD_MAX];
    uint8_t count = 0;

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
        /* 6KRO 는 여섯 칸까지만 담깁니다. 넘치는 것은 버려집니다. */
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
    if (rawkey_held_count == 0) {
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

/* 우리 키를 떼고 나면, 손가락이 누르고 있던 키의 자동 반복이 죽습니다.
 *
 * 윈도우는 반복을 가장 최근에 눌린 키 하나에만 걸어 줍니다. 우리가 보낸
 * 키가 그것을 가져가고, 떼면 반복이 그냥 멈춥니다 - 아직 눌려 있는 키로
 * 돌아오지 않습니다. 키를 누른 채로 무언가 나가고 있었다면 거기서 끊깁니다.
 *
 * 되살리려면 그 키에 새 누름 전환을 만들어 주어야 합니다. 리포트에서 한 번
 * 빼고 다시 넣으면 호스트가 새로 눌린 것으로 보고 반복을 거기로 되돌립니다.
 * 어느 키가 눌려 있는지는 리포트가 이미 알고 있으므로 PC 가 알려줄 필요가
 * 없습니다. 우리 키를 먼저 뗀 다음에 들여다보기 때문에 그 안에는 손가락이
 * 쥐고 있는 것만 남습니다. */
void rawkey_restore_repeat(void) {
    uint8_t held[RAWKEY_HELD_MAX];
    uint8_t count = 0;

#if defined(NKRO_ENABLE)
    /* add_key() 가 NKRO 리포트에 넣는 조건과 같아야 키가 실제로 담긴 쪽을
     * 읽습니다. 새 QMK(지금의 vial-qmk 포함)는 NKRO 가 따로 nkro_report 에
     * 담기고, 옛 QMK 는 keyboard_report 안의 nkro 에 담깁니다. */
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
            /* data[1] 이 개수, 그다음부터 키코드가 둘씩 이어집니다. 한
             * 리포트 안에서 전부 눌러야 호스트에 한 번에 나갑니다. */
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
            /* 우리가 누른 것만 쥐고 있으므로, 하나를 집어 떼든 통째로
             * 떼든 결과가 같습니다. */
            rawkey_release();
#ifdef RAWKEY_RESTORE_REPEAT
            /* 기본으로는 하지 않습니다. 되살리느라 호스트에 그 키의 뗌이 한
             * 번 보이는데, 누르고 있는 동안만 이어지는 채널링 스킬은 그
             * 뗌에서 끊깁니다. 다른 장치로 치면(v4) 반복을 빼앗지 않으므로
             * 되살릴 일 자체가 없습니다. 같은 장치로만 칠 수 있을 때를 위해
             * 남겨 둡니다. */
            rawkey_restore_repeat();
#endif
            return true;

        case RAWKEY_PING: {
            uint8_t reply[RAW_EPSIZE] = {0};
            reply[0] = RAWKEY_PING;
            reply[1] = RAWKEY_VERSION;
            /* 다음 키를 어느 장치로 칠지: 0 같은 길, 1 6KRO, 2 NKRO */
            reply[2] = rawkey_held_count ? rawkey_via : rawkey_pick_via();
            raw_hid_send(reply, sizeof(reply));
            return true;
        }
    }
    return false;
}

#ifndef RAWKEY_NO_HOOK
#    ifdef VIA_ENABLE
/* VIA/Vial 이 자기가 모르는 명령을 넘겨 주는 자리입니다. 우리 것도
 * 아니면 원래대로 모르는 명령이라고 표시해 돌려보냅니다. */
#        include "via.h"
void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (!rawkey_receive(data, length)) {
        data[0] = id_unhandled;
    }
}
#    else
/* VIA/Vial 이 없으면 로우 HID 는 전부 이 함수로 옵니다. */
void raw_hid_receive(uint8_t *data, uint8_t length) {
    rawkey_receive(data, length);
}
#    endif
#endif
