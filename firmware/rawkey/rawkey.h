/* rawkey - PC 가 로우 HID 로 "이 키 눌러 / 떼" 를 시키면 키보드가 칩니다. */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 명령 ID. VIA/Vial 이 쓰는 ID 와 겹치지 않는 자리입니다. 다른 모듈과
 * 부딪히면 빌드할 때 바꿀 수 있습니다. 그때는 PC 쪽도 같이 바꾸세요. */
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

/* 한꺼번에 잡고 있을 수 있는 키 수. HID 리포트가 담는 만큼이면 됩니다. */
#ifndef RAWKEY_HELD_MAX
#    define RAWKEY_HELD_MAX 10
#endif

/* 프로토콜 버전. 핑에 대한 답의 두 번째 바이트로 나갑니다. 세 번째
 * 바이트는 키를 칠 장치입니다 (0 같은 길, 1 6KRO, 2 NKRO).
 * v4: 손가락이 쓰지 않는 쪽 키보드 장치로 칩니다. */
#define RAWKEY_VERSION 0x04

/* 받은 리포트가 rawkey 명령이면 처리하고 true, 아니면 건드리지 않고
 * false 를 돌려줍니다. */
bool rawkey_receive(uint8_t *data, uint8_t length);

/* 눌러 둔 키가 있으면 전부 뗍니다. */
void rawkey_release(void);

/* 손가락이 누르고 있는 키의 자동 반복을 되살립니다. 그 키의 뗌이 호스트에
 * 한 번 보여 채널링 스킬이 끊기므로, RAWKEY_RESTORE_REPEAT 를 정의했을
 * 때만 뗌 처리에서 부릅니다. v4 처럼 다른 장치로 치면 필요 없습니다. */
void rawkey_restore_repeat(void);
