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

/* 옛 QMK(지금의 vial-qmk 포함)는 raw_hid.h 에 RAW_EPSIZE 가 없습니다. */
#ifndef RAW_EPSIZE
#    define RAW_EPSIZE 32
#endif

/* 한 번에 하나만 눌러 둡니다. PC 가 뗌을 못 보내고 죽어도 다음 누름이
 * 앞의 것을 정리합니다. */
static uint16_t rawkey_held = KC_NO;

void rawkey_release(void) {
    if (rawkey_held != KC_NO) {
        unregister_code16(rawkey_held);
        rawkey_held = KC_NO;
    }
}

bool rawkey_receive(uint8_t *data, uint8_t length) {
    if (length < 3) {
        return false;
    }
    uint16_t code = data[1] | ((uint16_t)data[2] << 8);

    switch (data[0]) {
        case RAWKEY_PRESS:
            rawkey_release();
            if (code != KC_NO) {
                register_code16(code);
                rawkey_held = code;
            }
            return true;

        case RAWKEY_RELEASE:
            if (code == KC_NO || code == rawkey_held) {
                rawkey_release();
            } else {
                unregister_code16(code);
            }
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
