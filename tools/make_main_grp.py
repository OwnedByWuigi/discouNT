#!/usr/bin/env python3
"""Build the small legacy Program Manager group used by the default image."""

import struct
import sys


def u16(value):
    return struct.pack("<H", value & 0xffff)


def build():
    group_name = b"Main\0"
    program_name = b"Command Prompt\0"
    command_line = b"CMD.EXE\0"
    icon_file = b"\0"

    title_offset = 34 + 2
    program_offset = title_offset + len(group_name)
    icon_info_offset = program_offset + 24 + len(program_name) + len(command_line) + len(icon_file)
    extension_offset = icon_info_offset + 12

    header = bytearray(34)
    header[0:4] = b"PMXX"
    header[6:8] = u16(extension_offset)
    header[8:10] = u16(1)       # SW_SHOWNORMAL
    header[10:12] = u16(0)      # x
    header[12:14] = u16(0)      # y
    header[14:16] = u16(480)    # width
    header[16:18] = u16(320)    # height
    header[22:24] = u16(title_offset)
    header[24:26] = u16(0x20)
    header[26:28] = u16(0x20)
    header[28:30] = u16(0x108)
    header[32:34] = u16(1)      # one program

    entry = bytearray(24)
    entry[0:2] = u16(16)        # icon x
    entry[2:4] = u16(16)        # icon y
    entry[6:8] = u16(0x048c)    # icon record format
    entry[8:10] = u16(0)        # XOR bytes
    entry[10:12] = u16(0)       # AND bits * 8
    entry[12:14] = u16(icon_info_offset)
    entry[14:16] = u16(icon_info_offset)
    entry[16:18] = u16(icon_info_offset)
    name_offset = program_offset + 24
    command_offset = name_offset + len(program_name)
    icon_offset = command_offset + len(command_line)
    entry[18:20] = u16(name_offset)
    entry[20:22] = u16(command_offset)
    entry[22:24] = u16(icon_offset)

    # The loader accepts an empty icon bitmap, but needs a complete icon-info
    # record for the dimensions/format fields it reads.
    icon_info = bytearray(12)
    icon_info[4:6] = u16(16)
    icon_info[6:8] = u16(16)
    icon_info[10:12] = u16(1)

    return bytes(header) + u16(program_offset) + group_name + bytes(entry) + \
        program_name + command_line + icon_file + bytes(icon_info)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_main_grp.py OUTPUT")
    with open(sys.argv[1], "wb") as output:
        output.write(build())
