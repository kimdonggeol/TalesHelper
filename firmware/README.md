# rawkey — let the keyboard do the typing

Instead of TalesHelper putting the keys from the wheel button menu and the
per-button keys in with `SendInput`, this **asks the QMK or Vial keyboard you
are already using to type them**. Not an imitation: the input really does come
from that keyboard.

The program works without it. Come here only if you want it.

```
rawkey/rawkey.c
rawkey/rawkey.h
```

## Why

A key put in with `SendInput` carries the operating system's injection marks.
The low-level hook gets the `INJECTED` flag, and raw input has no sending
device, so `hDevice` arrives as `0`. The kernel writes both at the moment
`SendInput` is called, and no process can touch them.

A key the keyboard types has no reason to carry any of that. It comes up the
same way a key pressed by hand does, and raw input shows that keyboard's
VID/PID.

Modifiers get simpler as a bonus. The `SendInput` path has to space the
modifier and the key 20ms apart, because a game reads its input once a frame;
the firmware's `register_code16` gets the order right by itself.

How to build and flash is written out separately in [BUILD.md](BUILD.md).

## Installing

1. Put `rawkey.c` and `rawkey.h` in the keymap folder.

2. **`rules.mk`**

   ```make
   SRC += rawkey.c
   ```

   `RAW_ENABLE = yes` is already on in Vial, so it need not be added. We share
   the channel the Vial app uses to pass keymaps back and forth.

3. Build and flash.

If something already owns `raw_hid_receive_kb`, it and the built-in hook are
defined twice and the link breaks. Define `RAWKEY_NO_HOOK` in `config.h` and
call `rawkey_receive()` from the function you already have. A `false` back
means it was not a rawkey command, so carry on with whatever you were doing.

If the command ids `0x40`–`0x43` collide with another module, `RAWKEY_PRESS`
and friends can be changed at build time. Change `TH_PRESS` in the PC's
`tales_helper.py` to match.

## Checking it works

Once it is flashed TalesHelper uses it on its own. There is no switch — with
no keyboard, or no receiver, it sends the old way, so there is nothing to ask.

Whether it went in is something you see from outside. Take a ring key with the
`input-origin` tool, outside this repository: before flashing it reads as
injected software, and after flashing as hardware, with the keyboard's
VID/PID. Measured:

```
input              low-level hook   raw input             verdict
Shift down         none             VID 4552 PID 0014     hardware     <- rawkey
M down             none             VID 4552 PID 0014     hardware     <- rawkey
M up               none             VID 4552 PID 0014     hardware     <- rawkey
Shift up           none             VID 4552 PID 0014     hardware     <- rawkey
F13 down           none             no device             injected     <- SendInput
```

Unplug the keyboard, or flash something without the receiver, or ask for a key
it cannot send, and it **falls back quietly**. The ring keeps working.

## Protocol

A 32-byte report. The first byte is the report id Windows insists on (`0x00`),
so the firmware's `data[0]` is the one after it.

| `data[0]` | `data[1..2]` | does |
|---|---|---|
| `0x40` | QMK keycode (little endian) | presses |
| `0x41` | same, `0` for whatever is held | releases |
| `0x42` | — | answers `0x42, version` |
| `0x43` | `data[1]` is the count, then keycodes two bytes each | presses them all together |

`0x43` arrived in v2. A v1 receiver does not know it, so the PC reads the
version from a ping and sends with `SendInput` instead.

Do not send `0x40` several times to hold several keys. Each press lets go of
the one before it and only the last survives. They have to go down inside one
report to reach the host together.

Press and release are separate because `TAP_CODE_DELAY` is 0 in a good many
builds. The key would then go down and up inside a USB frame or two, and a
game that reads input once a frame misses the lot. How long to hold is the
PC's business (`BOARD_HOLD_MS`, 40ms by default).

Up to `RAWKEY_HELD_MAX` keys (10) can be held. Closing the program lets go of
anything still down. Where it dies without the chance, the keyboard is left
holding that key, but the next press clears it, so the state does not carry.

### The auto-repeat on a key being held

The key does not come up, but its **auto-repeat dies.** Windows gives the
repeat to the most recently pressed key and to no other; a key the keyboard
sends is a real key, so it takes the repeat, and letting go stops it rather
than handing it back to the key still held down. Whatever was going out while
that key was held stops there. Injected keys never enter that state machine,
which is why the problem did not exist before. Measured:

| key dropped in | repeats after |
|---|---|
| a real key from the keyboard | 0 (stops) |
| a key put in with `SendInput` | 71 (back within 4ms) |

In the game this ends a **repeating skill** held on a number key.

`rawkey_restore_repeat()` hands the repeat back by **taking the held key out
of the report and putting it straight back**. The repeating skill survives,
but the host sees that key go up once, so a **channelled skill — one that
lasts only while the key is held — ends at that release.** Typing on the one
device means giving up one or the other.

### v4: types on the other keyboard device

Built with NKRO, the keyboard offers the host two keyboard devices: 6KRO
(`MI_00`) and NKRO (`MI_02&COL04`), and the keys from the hand only ever come
out of one of them. v4 **sends ours out the idle one.** Windows runs the
auto-repeat per device, so ours does not take the repeat from the key the hand
is holding — nothing to hand back, and no release of that key for anyone to
see. The hardware is unchanged.

| ring, while holding 2 (channelled) or 1 (repeating) | 2 | 1 |
|---|---|---|
| I by hand | survives | — |
| `SendInput` | survives | survives |
| v3, same device, restore on | **ends** (2 up/down) | — |
| v3, same device, restore off | survives | **ends** (repeat stops) |
| **v4, other device** | survives | survives |

With NKRO off it is hand → 6KRO, ours → NKRO; with it on, the other way
round. On the 6KRO side only six keys fit in one report. Under the boot
protocol (a BIOS, say) there is no NKRO device, so we type the same way the
hand does. The third byte of a ping's answer is the device in use (0 the same
one, 1 6KRO, 2 NKRO). Defining `RAWKEY_SAME_DEVICE` types the one way, as v3
did.

Below v3 the PC **skips the keyboard while any key is held** and sends with
`SendInput`. From v3 on it always goes to the keyboard.

### A ping is answered twice

rawkey sends its own answer (`42 01`), and then VIA hands the report it was
given straight back (`42 00`). The returned one has a zero where the version
belongs. There is no stopping it, so the PC **reads until an answer carries a
non-zero version**. Either order gives the same result.

Presses and releases come back the same way. That handle is **opened for
writing only**, so none of it queues up.

## Worth knowing

- **The ring still runs on the PC.** Deciding which direction was chosen is
  the PC's job; the keyboard is the finger that types what it is told.
- **Sending the same key breaks it.** The release wipes that key from the
  report, and it does not come back until the hand actually lets go and
  presses again. Keep the ring's keys off the ones you hold.
- **A modifier applies to everything held at that moment.** While `Shift+M`
  goes out, a key held alongside it reads as held with Shift.
- **Putting the mouse click back is still `SendInput`.** Send that from the
  keyboard too and the hook cannot tell it was the one that put it back, so it
  swallows it and puts it back again, round and round.
- **On a split, the half plugged into USB receives and types.** Flash both
  halves with the same firmware and either one works.
