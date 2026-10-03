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

/* 프로토콜 버전. 핑에 대한 답의 두 번째 바이트로 나갑니다. */
#define RAWKEY_VERSION 0x01

/* 받은 리포트가 rawkey 명령이면 처리하고 true, 아니면 건드리지 않고
 * false 를 돌려줍니다. */
bool rawkey_receive(uint8_t *data, uint8_t length);

/* 눌러 둔 키가 있으면 뗍니다. */
void rawkey_release(void);
