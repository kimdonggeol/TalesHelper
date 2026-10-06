# Flashing rawkey

Written so this page is enough on its own. None of the surrounding context is
needed.

## What this is for

Putting `rawkey.c` / `rawkey.h` from
`C:\project\tales-pip\firmware\rawkey\` into the Vial keyboard firmware and
flashing it again. It is the receiver that TalesHelper, in this repository,
tells "press this key".

**Flashed as v4 on 2026-10-04.**

| | added |
|---|---|
| v2 | `RAWKEY_CHORD (0x43)` — several keys pressed together inside one report |
| v3 | goes to the keyboard even while a key is held. `rawkey_restore_repeat()` only where `RAWKEY_RESTORE_REPEAT` is defined |
| v4 | types on whichever keyboard device (6KRO/NKRO) the hand is not using. The third byte of a ping's answer says which |

From v3 on, TalesHelper no longer detours through `SendInput` when a key is
held. v3 typed on the one device, which ended either the repeating skill or
the channelled one; v4 was confirmed in the game to leave both running. The
details are under "The auto-repeat on a key being held" and "v4" in
`README.md`.

## The target

| | |
|---|---|
| keyboard | `Tomak79H` (era/sirind/tomak79h), split |
| firmware | vial-qmk, `C:\vial-qmk` |
| keymap | `C:\vial-qmk\keyboards\era\sirind\tomak79h\keymaps\vial\` |
| USB | VID `0x4552` PID `0x0014` |

`SRC += rawkey.c` is **already in** `rules.mk`. `RAW_ENABLE` is on from Vial,
so it is not added separately.

## Steps

**1. Copy the files** — the repository holds the canonical copy. Overwrite
both.

```
C:\project\tales-pip\firmware\rawkey\rawkey.c
C:\project\tales-pip\firmware\rawkey\rawkey.h
        ↓
C:\vial-qmk\keyboards\era\sirind\tomak79h\keymaps\vial\
```

**2. Build**

```
qmk compile -kb era/sirind/tomak79h -km vial
```

or `make era/sirind/tomak79h:vial`. Whichever you normally use.

**3. Flash** — the same way as last time. A `.uf2` goes onto the drive the
bootloader puts up; a `.hex` goes through QMK Toolbox. It is a split, but
flashing **both halves with the same firmware** means either one works
plugged in.

## If the build breaks

This vial-qmk is older than current QMK, so names may differ or be missing.
`RAW_EPSIZE` was missing once already, and `rawkey.c` carries a fallback
definition for it to this day.

These are what v3 and v4 reach for. A break will be one of them.

| used | where it lives / what else |
|---|---|
| `#include "action_util.h"` | try `quantum.h` or `host.h` if absent |
| `keyboard_report` | `extern report_keyboard_t *keyboard_report;` in `action_util.h` |
| `add_key` / `del_key` / `send_keyboard_report` | same header |
| `keymap_config.nkro` | `eeconfig.h`. Only used under `NKRO_ENABLE` |
| `nkro_report` / `NKRO_REPORT_BITS` | newer QMK, this vial-qmk included. NKRO is kept apart from `keyboard_report` |
| `keyboard_report->nkro` / `KEYBOARD_REPORT_BITS` | older QMK. Taken when `NKRO_REPORT_BITS` is absent |
| `usb_device_state_get_protocol()` | newer QMK. Older QMK has `keyboard_protocol` |
| `KEYBOARD_REPORT_KEYS` | how many key slots the 6KRO report has (usually 6) |
| `host_keyboard_send` / `host_nkro_send` | v4. `host.h`. Sends a report to one device directly |
| `add_key_bit` | v4. `report.h`. Writes one key into an NKRO report |
| `IS_QK_MODS` / `QK_MODS_GET_MODS` and so on | v4. `keycodes.h`, `quantum_keycodes.h` |

If v4 breaks and is hard to fix, defining `RAWKEY_SAME_DEVICE` in `config.h`
types the one way, as v3 did. Either the repeating skill or the channelled one
ends then (see README).

**Do not take the NKRO branch out.** With NKRO on, a held key is only in the
NKRO report and the 6KRO one is empty: it builds, and not one repeat comes
back.

**Put whatever you fixed back into the repository's copy.** Otherwise the next
build breaks in the same place.

## After flashing

**1. Version** — TalesHelper has no button for this. Ask in Python.

```bash
python -c "import sys; sys.path.insert(0, r'C:\project\tales-pip'); import tales_helper as t; print(t.KeyboardLink().ping())"
```

```
(True, '연결됨 (VID 4552 PID 0014, 펌웨어 v4, NKRO 쪽)')
```

The program answers in Korean. An unchanged version means it was not
flashed, and `수신기가 없습니다` ("no receiver") means `SRC += rawkey.c` is
missing, or `raw_hid_receive_kb` is defined twice.

**2. While a key is held** — with the game in front, **hold a number key** and
use the wheel button ring once. Neither the repeating skill nor the channelled
one should end.

**3. Which device it goes out on** — at the end of that ping's answer.
`NKRO 쪽` or `6KRO 쪽` ("the NKRO / 6KRO one") means it is typing on the device
the hand is not using, which is what keeps it from taking the held key's
auto-repeat. `같은 장치` ("the same device") behaves as v3 did.

An up/down pair on the held key at that same moment means it was flashed with
the repeat restore on. A channelled skill ends there.

## VIAL_INSECURE (undecided)

`rules.mk` carries `VIAL_INSECURE = yes`. An earlier draft of this page said
leaving it on lets any program read and change the keymap, which **overstated
it.** Reading and writing the keymap work with the lock on.

The lock covers reading the matrix, changing macros, placing `QK_BOOT`, and
entering the bootloader, and nothing else. Building with it off also needs
`VIAL_UNLOCK_COMBO_ROWS` / `VIAL_UNLOCK_COMBO_COLS` chosen.

Neither way affects how rawkey behaves. Nothing urgent.

## What to report back

- what the ping above answered
- whether a skill ended when the ring was used with 1 held down
- anything fixed during the build, and how
- whether `VIAL_INSECURE` can be turned off
