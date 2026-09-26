# -*- coding: utf-8 -*-
"""Send one keystroke to the game, to see whether it accepts made-up input.

The radial menu can only work if the game reacts to a key the program sends,
so this answers that before anything is built on top of it.

    python tools/send_key_test.py i          # a letter
    python tools/send_key_test.py f1         # a function key
    python tools/send_key_test.py xbutton1   # the side button instead

It counts down, brings the game to the front, sends the key once, and stops.
Watch the game: if it reacts the same way it does to the real key, made-up
input is good enough.
"""
import ctypes
import sys
import time
from ctypes import wintypes

user32 = ctypes.windll.user32

INPUT_MOUSE, INPUT_KEYBOARD = 0, 1
KEYEVENTF_KEYUP, KEYEVENTF_SCANCODE = 0x0002, 0x0008
MOUSEEVENTF_XDOWN, MOUSEEVENTF_XUP = 0x0080, 0x0100
MAPVK_VK_TO_VSC = 0
NAMED = {f"f{n}": 0x70 + n - 1 for n in range(1, 13)}
NAMED.update({"esc": 0x1B, "tab": 0x09, "space": 0x20, "enter": 0x0D})


class MOUSEINPUT(ctypes.Structure):
    _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG),
                ("mouseData", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
                ("time", wintypes.DWORD),
                ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))]


class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wintypes.WORD), ("wScan", wintypes.WORD),
                ("dwFlags", wintypes.DWORD), ("time", wintypes.DWORD),
                ("dwExtraInfo", ctypes.POINTER(ctypes.c_ulong))]


class INPUT(ctypes.Structure):
    class _U(ctypes.Union):
        _fields_ = [("mi", MOUSEINPUT), ("ki", KEYBDINPUT)]
    _anonymous_ = ("u",)
    _fields_ = [("type", wintypes.DWORD), ("u", _U)]


def send(events):
    array = (INPUT * len(events))(*events)
    return user32.SendInput(len(events), array, ctypes.sizeof(INPUT))


def key_events(vk):
    """Both the virtual key and the scan code: a game reading the keyboard at
    a lower level often looks at the scan code and ignores the rest."""
    scan = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    down = INPUT(type=INPUT_KEYBOARD)
    down.ki = KEYBDINPUT(vk, scan, KEYEVENTF_SCANCODE, 0, None)
    up = INPUT(type=INPUT_KEYBOARD)
    up.ki = KEYBDINPUT(vk, scan, KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP, 0, None)
    return [down, up]


def side_events(which):
    down = INPUT(type=INPUT_MOUSE)
    down.mi = MOUSEINPUT(0, 0, which, MOUSEEVENTF_XDOWN, 0, None)
    up = INPUT(type=INPUT_MOUSE)
    up.mi = MOUSEINPUT(0, 0, which, MOUSEEVENTF_XUP, 0, None)
    return [down, up]


def game_window():
    found = []

    def visit(hwnd, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
        length = user32.GetWindowTextLengthW(hwnd)
        if length and user32.IsWindowVisible(hwnd):
            buf = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buf, length + 1)
            if "Talesweaver" in buf.value:
                found.append(hwnd)
        return True

    proto = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(proto(visit), 0)
    return found[0] if found else None


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    what = sys.argv[1].lower()
    if what in ("xbutton1", "xbutton2"):
        events = side_events(1 if what == "xbutton1" else 2)
    elif what in NAMED:
        events = key_events(NAMED[what])
    elif len(what) == 1:
        events = key_events(ord(what.upper()))
    else:
        sys.exit(f"do not know the key {what!r}")

    hwnd = game_window()
    if not hwnd:
        sys.exit("the game window was not found")
    for left in (3, 2, 1):
        print(f"  sending {what} in {left}...", flush=True)
        time.sleep(1)
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.3)
    sent = send(events)
    print(f"sent {sent} of {len(events)} events. Did the game react?")


if __name__ == "__main__":
    main()
