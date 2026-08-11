#!/usr/bin/env python3
import os
import re
import sys


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//.*", "", text)
    return text


def parse_defines(path: str, seen=None):
    if seen is None:
        seen = set()
    path = os.path.normpath(path)
    if path in seen or not os.path.exists(path):
        return {}
    seen.add(path)
    base = os.path.dirname(path)
    out = {}
    data = strip_comments(open(path, "r", encoding="utf-8", errors="ignore").read())
    for inc in re.finditer(r'^\s*#include\s+"([^"]+)"', data, re.M):
        out.update(parse_defines(os.path.join(base, inc.group(1)), seen))
    for m in re.finditer(r'^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+([^\s]+)', data, re.M):
        name, value = m.group(1), m.group(2)
        if value.startswith("("):
            continue
        try:
            out[name] = int(value, 0)
        except ValueError:
            pass
    return out


def normalize_lines(path: str):
    data = strip_comments(open(path, "r", encoding="utf-8", errors="ignore").read())
    lines = []
    buf = ""
    for raw in data.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith(",") and buf:
            buf += " " + line
        else:
            if buf:
                lines.append(buf)
            buf = line
    if buf:
        lines.append(buf)
    return lines


def resolve_id(token: str, defines):
    token = token.strip()
    if token in defines:
        return defines[token]
    return int(token, 0)


def decode_string(token: str) -> str:
    token = token.strip()
    if token.startswith('"') and token.endswith('"'):
        token = token[1:-1]
    token = token.replace(r'\"', '"').replace(r'\t', '\t').replace(r'\n', '\n').replace(r'\\', '\\')
    return token


def esc(text: str) -> str:
    return text.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\t", "\\t")


def parse_flags(rest: str):
    flags = 0
    up = rest.upper()
    if "CHECKED" in up:
        flags |= 0x00000008
    if "GRAYED" in up:
        flags |= 0x00000001
    if "DISABLED" in up:
        flags |= 0x00000002
    return flags


def emit_menu(out, key: str, items):
    out.append(f'MENU "{esc(key)}"')
    for entry in items:
        kind = entry["kind"]
        if kind == "popup":
            out.append(f'POPUP {entry["flags"]} "{esc(entry["text"])}"')
            emit_items(out, entry["items"])
            out.append("END")
        elif kind == "item":
            out.append(f'ITEM {entry["flags"]} {entry["id"]} "{esc(entry["text"])}"')
        elif kind == "sep":
            out.append(f'SEP {entry["flags"]}')
    out.append("ENDMENU")


def emit_items(out, items):
    for entry in items:
        kind = entry["kind"]
        if kind == "popup":
            out.append(f'POPUP {entry["flags"]} "{esc(entry["text"])}"')
            emit_items(out, entry["items"])
            out.append("END")
        elif kind == "item":
            out.append(f'ITEM {entry["flags"]} {entry["id"]} "{esc(entry["text"])}"')
        elif kind == "sep":
            out.append(f'SEP {entry["flags"]}')


def parse_rc(path: str):
    defines = parse_defines(path)
    lines = normalize_lines(path)
    menus = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*|0x[0-9A-Fa-f]+|\d+)\s+MENU\b', line)
        if not m:
            i += 1
            continue
        key = m.group(1)
        items = []
        stack = [items]
        i += 1
        while i < len(lines) and lines[i] not in ("BEGIN", "{"):
            i += 1
        i += 1
        while i < len(lines):
            line = lines[i]
            if line in ("BEGIN", "{"):
                i += 1
                continue
            if line in ("END", "}"):
                if len(stack) == 1:
                    break
                stack.pop()
                i += 1
                continue
            pm = re.match(r'^POPUP\s+"((?:[^"\\]|\\.)*)"(.*)$', line, re.I)
            if pm:
                entry = {"kind": "popup", "text": decode_string(f'"{pm.group(1)}"'), "flags": parse_flags(pm.group(2)), "items": []}
                stack[-1].append(entry)
                stack.append(entry["items"])
                i += 1
                continue
            if re.match(r'^MENUITEM\s+SEPARATOR\b', line, re.I):
                stack[-1].append({"kind": "sep", "flags": parse_flags(line)})
                i += 1
                continue
            im = re.match(r'^MENUITEM\s+"((?:[^"\\]|\\.)*)"\s*,\s*([A-Za-z_][A-Za-z0-9_]*|0x[0-9A-Fa-f]+|\d+)(.*)$', line, re.I)
            if im:
                stack[-1].append({
                    "kind": "item",
                    "text": decode_string(f'"{im.group(1)}"'),
                    "id": resolve_id(im.group(2), defines),
                    "flags": parse_flags(im.group(3)),
                })
                i += 1
                continue
            i += 1
        if key in defines:
            key = str(defines[key])
        menus.append((key, items))
        i += 1
    return menus


def main():
    if len(sys.argv) != 3:
        print("usage: rc_menu_gen.py input.rc output.mnu", file=sys.stderr)
        return 1
    menus = parse_rc(sys.argv[1])
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        out = []
        for key, items in menus:
            emit_menu(out, key, items)
        f.write("\n".join(out))
        if out:
            f.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
