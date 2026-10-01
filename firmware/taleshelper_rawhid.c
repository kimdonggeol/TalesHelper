/* TalesHelper -> 키보드 로우 HID 수신기
 *
 * 고리에서 고른 키를 PC 가 SendInput 으로 넣는 대신, 이 키보드에게
 * "눌러라 / 떼라" 고 시킵니다. 그러면 그 키는 흉내가 아니라 실제로 이
 * 키보드가 보낸 것이 됩니다. 운영체제가 보기에 손가락으로 친 것과 같고,
 * 주입 플래그도 붙지 않습니다.
 *
 * 누름과 뗌을 따로 둔 이유: tap_code16 은 둘 사이가 TAP_CODE_DELAY 인데
 * 기본값이 0 인 빌드가 많습니다. 그러면 USB 프레임 한두 개 안에 끝나서,
 * 한 프레임에 한 번 입력을 읽는 게임은 못 보고 지나갑니다. 얼마나 눌러
 * 둘지는 PC 쪽이 정하게 두는 편이 확실합니다.
 *
 * 설치
 *   1) rules.mk 에  RAW_ENABLE = yes       (Vial 이면 이미 켜져 있습니다)
 *   2) 이 파일을 keymap 폴더에 두고 rules.mk 에
 *          SRC += taleshelper_rawhid.c
 *      또는 keymap.c 에서 #include "taleshelper_rawhid.c"
 *
 * 이미 raw_hid_receive_kb 를 쓰고 있다면 아래 내용을 그 함수 안으로
 * 옮기세요. 두 번 정의하면 링크가 깨집니다. VIA/Vial 은 자기가 모르는
 * 명령 ID 를 이 함수로 넘겨 주므로, 0x40~0x42 는 가로채이지 않습니다.
 */

#include QMK_KEYBOARD_H
#include "raw_hid.h"

#define TH_PRESS   0x40
#define TH_RELEASE 0x41
#define TH_PING    0x42
#define TH_VERSION 0x01

/* 한 번에 하나만 눌러 둡니다. PC 가 뗌을 못 보내고 죽으면 키가 눌린 채로
 * 남는데, 다음 누름이 들어올 때 앞의 것을 정리해 그 상태가 이어지지 않게
 * 합니다. */
static uint16_t th_held = KC_NO;

void th_release_held(void) {
    if (th_held != KC_NO) {
        unregister_code16(th_held);
        th_held = KC_NO;
    }
}

void raw_hid_receive_kb(uint8_t *data, uint8_t length) {
    if (length < 3) {
        return;
    }
    uint16_t code = data[1] | ((uint16_t)data[2] << 8);

    switch (data[0]) {
        case TH_PRESS:
            th_release_held();
            if (code != KC_NO) {
                register_code16(code);
                th_held = code;
            }
            break;

        case TH_RELEASE:
            if (code == KC_NO || code == th_held) {
                th_release_held();
            } else {
                unregister_code16(code);
            }
            break;

        case TH_PING: {
            /* PC 가 이 펌웨어인지 확인하는 데 씁니다. 답이 없으면 아직
             * 안 구운 것이고, PC 는 조용히 SendInput 으로 돌아갑니다. */
            uint8_t reply[RAW_EPSIZE] = {0};
            reply[0] = TH_PING;
            reply[1] = TH_VERSION;
            raw_hid_send(reply, sizeof(reply));
            break;
        }
    }
}
