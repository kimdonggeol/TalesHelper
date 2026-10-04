# rawkey v3 굽기

이 문서만 보고 진행할 수 있게 적었습니다. 앞뒤 맥락은 몰라도 됩니다.

## 하려는 것

`C:\project\tales-pip\firmware\rawkey\` 의 `rawkey.c` / `rawkey.h` 를
Vial 키보드 펌웨어에 넣어 다시 굽습니다. 이 저장소의 TalesHelper 가 그
키보드에게 "이 키 눌러" 를 시키는 수신기입니다.

지금 키보드에는 **v1** 이 올라가 있고, **v3** 으로 올려야 합니다.

| | 추가된 것 |
|---|---|
| v2 | `RAWKEY_CHORD (0x43)` — 키 여럿을 한 리포트 안에서 같이 누름 |
| v3 | `rawkey_restore_repeat()` — 뗀 뒤에 눌려 있던 키의 자동 반복을 되살림 |

v3 가 올라가야 **키를 누른 채로 기능을 써도 자동 반복이 안 끊깁니다.**
그 전까지는 TalesHelper 가 그 상황을 피해 `SendInput` 으로 우회합니다.

## 대상

| | |
|---|---|
| 키보드 | `Tomak79H` (era/sirind/tomak79h), 스플릿 |
| 펌웨어 | vial-qmk, `C:\vial-qmk` |
| keymap | `C:\vial-qmk\keyboards\era\sirind\tomak79h\keymaps\vial\` |
| USB | VID `0x4552` PID `0x0014` |

`rules.mk` 에 `SRC += rawkey.c` 는 **이미 들어가 있습니다.** `RAW_ENABLE` 은
Vial 이 켜 두므로 따로 넣지 않습니다.

## 순서

**1. 파일 복사** — 저장소 쪽이 정본입니다. 둘 다 덮어쓰세요.

```
C:\project\tales-pip\firmware\rawkey\rawkey.c
C:\project\tales-pip\firmware\rawkey\rawkey.h
        ↓
C:\vial-qmk\keyboards\era\sirind\tomak79h\keymaps\vial\
```

**2. 빌드**

```
qmk compile -kb era/sirind/tomak79h -km vial
```

또는 `make era/sirind/tomak79h:vial`. 평소 쓰시던 쪽이면 됩니다.

**3. 굽기** — 이전에 구울 때와 같은 방법입니다. `.uf2` 면 부트로더로 들어간
드라이브에 끌어다 놓고, `.hex` 면 QMK Toolbox 로. 스플릿이지만 **좌우에 같은
펌웨어**를 구우면 어느 쪽을 꽂아도 됩니다.

## 빌드가 깨지면

이 vial-qmk 는 최신 QMK 보다 오래돼서 이름이 다르거나 없는 것이 있을 수
있습니다. 실제로 전에 `RAW_EPSIZE` 가 없어서 `rawkey.c` 에 대체 정의를
넣었습니다 (지금도 파일 안에 있습니다).

v3 에서 새로 쓰는 것들입니다. 깨지면 이 중 하나입니다.

| 쓰는 것 | 어디 있나 / 대안 |
|---|---|
| `#include "action_util.h"` | 없으면 `quantum.h` 나 `host.h` 로 |
| `keyboard_report` | `action_util.h` 의 `extern report_keyboard_t *keyboard_report;` |
| `add_key` / `del_key` / `send_keyboard_report` | 같은 헤더 |
| `keymap_config.nkro` | `eeconfig.h`. `NKRO_ENABLE` 일 때만 씁니다 |
| `KEYBOARD_REPORT_BITS` | NKRO 비트 수. 이름이 다르면 `#if defined` 블록째 지워도 됩니다 |
| `KEYBOARD_REPORT_KEYS` | 6KRO 리포트의 키 칸 수 (보통 6) |

NKRO 분기를 통째로 들어내도 동작합니다. 그 경우 6KRO 경로만 남고, 리포트에
들어가는 키 여섯 개까지만 반복이 되살아납니다. 실사용에는 충분합니다.

**고친 내용은 저장소 쪽 파일에도 그대로 반영해 주세요.** 안 그러면 다음에
또 같은 데서 깨집니다.

## 구운 뒤 확인

**1. 버전** — TalesHelper 설정 창 → **마우스** → `연결 확인`

```
연결됨 (VID 4552 PID 0014, 펌웨어 v3)
```

`v1` 이 그대로면 안 구워진 것이고, `수신기가 없습니다` 가 뜨면 `SRC +=
rawkey.c` 가 빠졌거나 `raw_hid_receive_kb` 가 두 번 정의된 것입니다.

**2. 자동 반복** — 이게 v3 의 핵심입니다.

게임을 앞에 두고 **숫자 1 을 누르고 있는 채로** 사이드 버튼 고리를 한 번
쓰세요. 스킬이 끊기지 않아야 합니다.

**3. 기록으로 확인** — 설정 → 마우스 → `입력 기록`

같은 동작을 하고 `저장하고 닫기`. `C:\project\tales-pip\keytrace.txt` 에
이렇게 찍혀야 맞습니다.

```
 7.722  I   누름   VID 4552      ← 고리가 보낸 키
 7.773  I   뗌     VID 4552
 7.774  1   뗌     VID 4552      ← v3 가 되살리는 한 쌍
 7.775  1   누름   VID 4552
 7.79   1   누름   VID 4552      ← 반복이 다시 이어짐
 7.81   1   누름   VID 4552
```

`1 뗌` / `1 누름` 한 쌍이 없고 그 뒤 반복도 없으면 `rawkey_restore_repeat()`
가 안 불린 것입니다.

## 겸사겸사 (선택)

`rules.mk` 의 `VIAL_INSECURE = yes` 가 **정말 필요한지** 확인해 볼 만합니다.
Vial 의 잠금 해제 절차를 통째로 끄는 옵션이라, 켜 두면 아무 프로그램이나
키맵을 읽고 바꿀 수 있습니다.

rawkey 명령은 `raw_hid_receive_kb` 로 들어오는 경로라 Vial 의 잠금 분기에
애초에 안 들어갑니다. **꺼서 구운 뒤 `연결 확인` 이 그대로 되면 꺼두는 게
맞습니다.**

## 돌아와서 알려줄 것

- `연결 확인` 에 뜬 문구
- 1 을 누른 채로 고리를 썼을 때 스킬이 끊겼는지
- `keytrace.txt` (있으면 그대로)
- 빌드 중에 고친 것이 있으면 무엇을 어떻게
- `VIAL_INSECURE` 를 꺼도 되던지
