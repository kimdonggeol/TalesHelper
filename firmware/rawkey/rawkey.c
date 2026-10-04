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
static uint8_t  rawkey_held_count = 0;

void rawkey_release(void) {
    while (rawkey_held_count) {
        unregister_code16(rawkey_held[--rawkey_held_count]);
    }
}

static void rawkey_hold(uint16_t code) {
    if (code == KC_NO || rawkey_held_count >= RAWKEY_HELD_MAX) {
        return;
    }
    register_code16(code);
    rawkey_held[rawkey_held_count++] = code;
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
            rawkey_restore_repeat();
            return true;

        case RAWKEY_PING: {
            uint8_t reply[RAW_EPSIZE] = {0};
            reply[0] = RAWKEY_PING;
            reply[1] = RAWKEY_VERSION;
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
