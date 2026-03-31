#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path


class BF:
    def __init__(self) -> None:
        self.parts: list[str] = []
        self.ptr = 0

    def emit(self, text: str) -> None:
        self.parts.append(text)

    def comment(self, text: str) -> None:
        forbidden = set("<>+-.,[]")
        safe = "".join("_" if ch in forbidden else ch for ch in text)
        self.parts.append(f"\n{safe}\n")

    def move(self, target: int) -> None:
        delta = target - self.ptr
        if delta > 0:
            self.parts.append(">" * delta)
        elif delta < 0:
            self.parts.append("<" * (-delta))
        self.ptr = target

    def finish(self) -> str:
        return "".join(self.parts)


HOME = 0
CELL = {
    "gap_op": -2,
    "gap_scratch": -1,
    "file0": 0,
    "file1": 1,
    "file2": 2,
    "tape0": 3,
    "tape1": 4,
    "tape2": 5,
    "depth0": 6,
    "depth1": 7,
    "ch": 8,
    "tmp": 9,
    "flag": 10,
    "out": 11,
}
STATE_ORDER = [
    "file0",
    "file1",
    "file2",
    "tape0",
    "tape1",
    "tape2",
    "depth0",
    "depth1",
    "ch",
    "tmp",
    "flag",
    "out",
]


def off(name: str) -> int:
    return CELL[name]


def clear(bf: BF, name: str) -> None:
    bf.move(off(name))
    bf.emit("[-]")


def add_const(bf: BF, name: str, value: int) -> None:
    bf.move(off(name))
    if value >= 0:
        bf.emit("+" * value)
    else:
        bf.emit("-" * (-value))


def set_const(bf: BF, name: str, value: int) -> None:
    clear(bf, name)
    add_const(bf, name, value)


def copy_preserve(bf: BF, src: str, dst: str, tmp: str) -> None:
    clear(bf, dst)
    clear(bf, tmp)
    bf.move(off(src))
    bf.emit("[")
    bf.emit("-")
    bf.move(off(dst))
    bf.emit("+")
    bf.move(off(tmp))
    bf.emit("+")
    bf.move(off(src))
    bf.emit("]")
    bf.move(off(tmp))
    bf.emit("[")
    bf.emit("-")
    bf.move(off(src))
    bf.emit("+")
    bf.move(off(tmp))
    bf.emit("]")


def if_zero_destructive(bf: BF, cell: str, body) -> None:
    clear(bf, "flag")
    add_const(bf, "flag", 1)
    bf.move(off(cell))
    bf.emit("[")
    bf.emit("-")
    clear(bf, "flag")
    bf.move(off(cell))
    bf.emit("]")
    bf.move(off("flag"))
    bf.emit("[")
    bf.emit("-")
    body()
    bf.move(off("flag"))
    bf.emit("]")


def if_nonzero_destructive(bf: BF, cell: str, body) -> None:
    bf.move(off(cell))
    bf.emit("[")
    bf.emit("[-]")
    body()
    bf.move(off(cell))
    bf.emit("]")


def if_eq_const(bf: BF, src: str, value: int, body) -> None:
    copy_preserve(bf, src, "tmp", "flag")
    bf.move(off("tmp"))
    bf.emit("-" * value)
    if_zero_destructive(bf, "tmp", body)


def inc16(bf: BF, lo: str, hi: str) -> None:
    add_const(bf, lo, 1)

    def carry_hi() -> None:
        add_const(bf, hi, 1)

    copy_preserve(bf, lo, "tmp", "flag")
    if_zero_destructive(bf, "tmp", carry_hi)


def inc24_once(bf: BF, a: str, b: str, c: str) -> None:
    add_const(bf, a, 1)

    def carry_b() -> None:
        add_const(bf, b, 1)

        def carry_c() -> None:
            add_const(bf, c, 1)

        copy_preserve(bf, b, "tmp", "flag")
        if_zero_destructive(bf, "tmp", carry_c)

    copy_preserve(bf, a, "tmp", "flag")
    if_zero_destructive(bf, "tmp", carry_b)


def add7_24(bf: BF, a: str, b: str, c: str) -> None:
    for _ in range(7):
        inc24_once(bf, a, b, c)


def dec16_or_error(bf: BF) -> None:
    def low_is_zero() -> None:
        def high_is_zero() -> None:
            error_out(bf)

        def high_nonzero() -> None:
            add_const(bf, "depth1", -1)
            set_const(bf, "depth0", 255)

        copy_preserve(bf, "depth1", "tmp", "flag")
        if_zero_destructive(bf, "tmp", high_is_zero)
        copy_preserve(bf, "depth1", "tmp", "flag")
        if_nonzero_destructive(bf, "tmp", high_nonzero)

    def low_nonzero() -> None:
        add_const(bf, "depth0", -1)

    copy_preserve(bf, "depth0", "tmp", "flag")
    if_zero_destructive(bf, "tmp", low_is_zero)
    copy_preserve(bf, "depth0", "tmp", "flag")
    if_nonzero_destructive(bf, "tmp", low_nonzero)


def shift_state_right_one_record(bf: BF) -> None:
    max_offset = off("out")
    for rel in range(max_offset, -1, -1):
        bf.move(rel)
        bf.emit("[")
        bf.emit("-")
        bf.move(rel + 2)
        bf.emit("+")
        bf.move(rel)
        bf.emit("]")
    bf.move(HOME)
    bf.emit(">>")
    bf.ptr = HOME


def accept_helper(bf: BF, helper_low: int, depth_mode: str) -> None:
    if depth_mode == "open":
        inc16(bf, "depth0", "depth1")
    elif depth_mode == "close":
        dec16_or_error(bf)

    add_const(bf, "gap_op", helper_low)
    add7_24(bf, "file0", "file1", "file2")
    add7_24(bf, "tape0", "tape1", "tape2")
    shift_state_right_one_record(bf)


def error_out(bf: BF) -> None:
    bf.move(off("gap_op"))
    bf.emit("<<[<<]<")
    bf.ptr = off("gap_op") - 3


def emit_const_stream(bf: BF, items) -> None:
    current = 0
    clear(bf, "out")
    for item in items:
        if isinstance(item, int):
            bf.move(off("out"))
            delta = (item - current) % 256
            if delta <= 128:
                bf.emit("+" * delta)
            else:
                bf.emit("-" * (256 - delta))
            bf.emit(".")
            current = item
        else:
            bf.move(off(item))
            bf.emit(".")


def header_bytes():
    return [
        0x7F, 0x45, 0x4C, 0x46, 0x01, 0x01, 0x01,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x81, 0x04, 0x08,
        0x34, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x34, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x80, 0x04, 0x08,
        0x00, 0x80, 0x04, 0x08,
        "file0", "file1", "file2", 0x00,
        "file0", "file1", "file2", 0x01,
        0x07, 0x00, 0x00, 0x00,
        0x00, 0x10, 0x00, 0x00,
    ]


def runtime_prefix_bytes():
    return [
        0xBE, "tape0", "tape1", "tape2", 0x08,
        0x89, 0xF7, 0xB8, 0xEA, 0x81, 0x04, 0x08, 0xFF, 0xE0,
        0x47, 0xC3,
        0x39, 0xF7, 0x75, 0x22, 0xB8, 0x04, 0x00, 0x00, 0x00,
        0xBB, 0x02, 0x00, 0x00, 0x00, 0xB9, 0xC9, 0x81, 0x04, 0x08,
        0xBA, 0x21, 0x00, 0x00, 0x00, 0xCD, 0x80, 0xB8, 0x01, 0x00,
        0x00, 0x00, 0xBB, 0x01, 0x00, 0x00, 0x00, 0xCD, 0x80, 0x4F, 0xC3,
        0xFE, 0x07, 0xC3, 0xFE, 0x0F, 0xC3,
        0xB8, 0x04, 0x00, 0x00, 0x00, 0xBB, 0x01, 0x00, 0x00, 0x00,
        0x89, 0xF9, 0xBA, 0x01, 0x00, 0x00, 0x00, 0xCD, 0x80, 0xC3,
        0xB8, 0x03, 0x00, 0x00, 0x00, 0x31, 0xDB, 0x89, 0xF9, 0xBA,
        0x01, 0x00, 0x00, 0x00, 0xCD, 0x80, 0x83, 0xF8, 0x01, 0x74,
        0x03, 0xC6, 0x07, 0x00, 0xC3,
        0x80, 0x3F, 0x00, 0x75, 0x29, 0x8B, 0x1C, 0x24, 0xB9, 0x01,
        0x00, 0x00, 0x00, 0x8B, 0x43, 0x01, 0x3D, 0x6B, 0x81, 0x04,
        0x08, 0x74, 0x11, 0x3D, 0x9A, 0x81, 0x04, 0x08, 0x75, 0x0B,
        0x49, 0x75, 0x08, 0x83, 0xC3, 0x07, 0x89, 0x1C, 0x24, 0xC3,
        0x41, 0x83, 0xC3, 0x07, 0xEB, 0xDF, 0xC3,
        0x80, 0x3F, 0x00, 0x74, 0x29, 0x8B, 0x1C, 0x24, 0x83, 0xEB,
        0x0E, 0xB9, 0x01, 0x00, 0x00, 0x00, 0x8B, 0x43, 0x01, 0x3D,
        0x9A, 0x81, 0x04, 0x08, 0x74, 0x0E, 0x3D, 0x6B, 0x81, 0x04,
        0x08, 0x75, 0x08, 0x49, 0x75, 0x05, 0x89, 0x1C, 0x24, 0xC3,
        0x41, 0x83, 0xEB, 0x07, 0xEB, 0xE2, 0xC3,
        0x72, 0x75, 0x6E, 0x74, 0x69, 0x6D, 0x65, 0x20, 0x65, 0x72,
        0x72, 0x6F, 0x72, 0x3A, 0x20, 0x70, 0x6F, 0x69, 0x6E, 0x74,
        0x65, 0x72, 0x20, 0x75, 0x6E, 0x64, 0x65, 0x72, 0x66, 0x6C,
        0x6F, 0x77, 0x0A,
    ]


def exit_bytes():
    return [0xB8, 0x01, 0x00, 0x00, 0x00, 0x31, 0xDB, 0xCD, 0x80]


def generate() -> str:
    bf = BF()

    bf.comment("bootstrap-compiler-state")
    bf.move(4)
    bf.ptr = HOME
    set_const(bf, "file0", 0xF3)
    set_const(bf, "file1", 0x01)
    set_const(bf, "file2", 0x00)
    set_const(bf, "tape0", 0xF3)
    set_const(bf, "tape1", 0x81)
    set_const(bf, "tape2", 0x04)

    bf.comment("read-source")
    bf.move(off("ch"))
    bf.emit(",")
    bf.emit("[")

    mappings = [
        (ord(">"), 0x0E, "plain"),
        (ord("<"), 0x10, "plain"),
        (ord("+"), 0x38, "plain"),
        (ord("-"), 0x3B, "plain"),
        (ord("."), 0x3E, "plain"),
        (ord(","), 0x52, "plain"),
        (ord("["), 0x6B, "open"),
        (ord("]"), 0x9A, "close"),
    ]
    for src_char, helper_low, depth_mode in mappings:
        bf.comment(f"map-{chr(src_char)}")

        def body(src_char=src_char, helper_low=helper_low, depth_mode=depth_mode):
            accept_helper(bf, helper_low, depth_mode)

        if_eq_const(bf, "ch", src_char, body)

    bf.move(off("ch"))
    bf.emit(",")
    bf.move(off("ch"))
    bf.emit("]")

    bf.comment("validate-depth")
    copy_preserve(bf, "depth0", "tmp", "flag")
    if_nonzero_destructive(bf, "tmp", lambda: error_out(bf))
    copy_preserve(bf, "depth1", "tmp", "flag")
    if_nonzero_destructive(bf, "tmp", lambda: error_out(bf))

    bf.comment("emit-header")
    emit_const_stream(bf, header_bytes())
    bf.move(off("out"))
    bf.emit("." * (0x100 - 84))

    bf.comment("emit-runtime-prefix")
    emit_const_stream(bf, runtime_prefix_bytes())

    bf.comment("rewind-to-first-op")
    bf.move(off("gap_op"))
    bf.emit("<<[<<]>>")

    bf.comment("emit-main")
    bf.emit("[")
    bf.emit(">")
    current = 0
    for byte in [0xB8]:
        delta = (byte - current) % 256
        bf.emit("+" * delta if delta <= 128 else "-" * (256 - delta))
        bf.emit(".")
        current = byte
    bf.emit("<.")
    bf.emit(">")
    for byte in [0x81, 0x04, 0x08, 0xFF, 0xD0]:
        delta = (byte - current) % 256
        bf.emit("+" * delta if delta <= 128 else "-" * (256 - delta))
        bf.emit(".")
        current = byte
    bf.emit(">")
    bf.emit("]")

    bf.comment("emit-exit")
    bf.emit(">")
    current = 0
    for byte in exit_bytes():
        delta = (byte - current) % 256
        bf.emit("+" * delta if delta <= 128 else "-" * (256 - delta))
        bf.emit(".")
        current = byte

    return bf.finish()


def main() -> None:
    out_path = Path("src/bfc.bf")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(generate(), encoding="ascii")


if __name__ == "__main__":
    main()
