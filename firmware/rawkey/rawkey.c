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
