#!/usr/bin/env python3
"""Cross-platform replacement for libx11/src/util/makekeys.c.

Generates ks_tables.h from keysym definition headers. The original build
compiled makekeys.c with the host gcc (/usr/bin/gcc); this Python port
produces identical output and only requires Python, which the build already
depends on via find_package(Python3 REQUIRED).

Usage: makekeys.py [-o OUT] <keysymdef.h> <XF86keysym.h> ...
"""

import sys

KTNUM = 4000
XK_VoidSymbol = 0xFFFFFF
MIN_REHASH = 15
MATCHES = 10
MASK32 = 0xFFFFFFFF


def parse_line(buf):
    """Return (key, val, prefix) for a keysym definition, or None."""
    s = buf.strip()

    # #define XK_foo 0x1234
    if s.startswith("#define"):
        rest = s[len("#define"):].strip()
        parts = rest.split(None, 2)
        if len(parts) >= 2:
            key = parts[0]
            valstr = parts[1]
            if valstr.startswith("0x") and "XK_" in key:
                try:
                    val = int(valstr[2:], 16)
                except ValueError:
                    return None
                tmp = key.find("XK_")
                prefix = key[:tmp]
                key = key[tmp + 3:]
                return key, val, prefix
            if valstr.startswith("_EVDEVK(0x") and "XK_" in key:
                inner = valstr[len("_EVDEVK(0x"):].rstrip(")")
                try:
                    val = int(inner, 16) + 0x10081000
                except ValueError:
                    return None
                tmp = key.find("XK_")
                prefix = key[:tmp]
                key = key[tmp + 3:]
                return key, val, prefix
            # #define XK_foo XK_bar (alias)
            alias = valstr
            if "XK_" in key and "XK_" in alias:
                tmp = key.find("XK_")
                prefix = key[:tmp]
                key = key[tmp + 3:]
                tmpa = alias.find("XK_")
                alias = alias[tmpa + 3:]
                # resolve alias against already-parsed definitions (reverse order)
                for name, v in reversed(info):
                    if name == alias:
                        return key, v, prefix
    return None


info = []  # list of (name, val)


def main(argv):
    out = sys.stdout
    if len(argv) >= 2 and argv[0] == "-o":
        # No newline translation, so the generated header is identical on every
        # platform (which keeps the build reproducible across hosts).
        out = open(argv[1], "w", newline="\n")
        argv = argv[2:]

    ksnum = 0
    for path in argv:
        try:
            # Read byte transparently: only ASCII tokens are interpreted, so the
            # encoding of the comments in the headers must never break the build.
            with open(path, "r", encoding="latin-1") as fptr:
                for buf in fptr:
                    r = parse_line(buf)
                    if r is None:
                        continue
                    key, val, prefix = r
                    if val == XK_VoidSymbol:
                        val = 0
                    if val > 0x1FFFFFFF:
                        continue
                    name = prefix + key
                    info.append((name, val))
                    ksnum += 1
                    if ksnum == KTNUM:
                        sys.stderr.write("makekeys: too many keysyms!\n")
                        sys.exit(1)
        except OSError as e:
            sys.stderr.write("couldn't open %s\n" % path)

    out.write("/* This file is generated from keysymdef.h. */\n")
    out.write("/* Do not edit. */\n")
    out.write("\n")

    # ---- string -> keysym table ----
    best_max_rehash = ksnum
    num_found = 0
    best_z = 0
    for z in range(ksnum, KTNUM):
        max_rehash = 0
        tab = [0] * z
        for i in range(ksnum):
            name = info[i][0]
            sig = 0
            for ch in name:
                sig = ((sig << 1) + ord(ch)) & MASK32
            first = j = sig % z
            k = 0
            while tab[j]:
                k += 1
                j += first + 1
                if j >= z:
                    j -= z
                if j == first:
                    # give up on this z
                    k = -1
                    break
            if k == -1:
                break
            tab[j] = 1
            if k > max_rehash:
                max_rehash = k
        else:
            # completed all i without break
            if max_rehash < MIN_REHASH:
                if max_rehash < best_max_rehash:
                    best_max_rehash = max_rehash
                    best_z = z
                num_found += 1
                if num_found >= MATCHES:
                    break

    z = best_z
    if z == 0:
        sys.stderr.write("makekeys: failed to find small enough hash!\n"
                         "Try increasing KTNUM in makekeys.c\n")
        sys.exit(1)

    out.write("#ifdef NEEDKTABLE\n")
    out.write("const unsigned char _XkeyTable[] = {\n")
    out.write("0,\n")
    offsets = [0] * KTNUM
    indexes = [0] * KTNUM
    k = 1
    for i in range(ksnum):
        name = info[i][0]
        sig = 0
        for ch in name:
            sig = ((sig << 1) + ord(ch)) & MASK32
        first = j = sig % z
        while offsets[j]:
            j += first + 1
            if j >= z:
                j -= z
        offsets[j] = k
        indexes[i] = k
        val = info[i][1]
        out.write("0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x, 0x%.2x, " % (
            (sig >> 8) & 0xFF, sig & 0xFF,
            (val >> 24) & 0xFF, (val >> 16) & 0xFF,
            (val >> 8) & 0xFF, val & 0xFF))
        k += 7
        for ch in name:
            out.write("'%c'," % ch)
            k += 1
        out.write("0\n" if i == (ksnum - 1) else "0,\n")
    out.write("};\n")
    out.write("\n")
    out.write("#define KTABLESIZE %d\n" % z)
    out.write("#define KMAXHASH %d\n" % (best_max_rehash + 1))
    out.write("\n")
    out.write("static const unsigned short hashString[KTABLESIZE] = {\n")
    for i in range(z):
        out.write("0x%.4x" % offsets[i])
        i += 1
        if i == z:
            break
        out.write(", " if (i & 7) else ",\n")
    out.write("\n")
    out.write("};\n")
    out.write("#endif /* NEEDKTABLE */\n")

    # ---- keysym -> string table ----
    best_max_rehash = ksnum
    num_found = 0
    best_z = 0
    for z in range(ksnum, KTNUM):
        max_rehash = 0
        tab = [0] * z
        values = [0] * z
        ok = True
        for i in range(ksnum):
            val = info[i][1]
            first = j = val % z
            k = 0
            while tab[j]:
                if values[j] == val:
                    break
                k += 1
                j += first + 1
                if j >= z:
                    j -= z
                if j == first:
                    ok = False
                    break
            if not ok:
                break
            if not tab[j]:
                tab[j] = 1
                values[j] = val
                if k > max_rehash:
                    max_rehash = k
        if not ok:
            continue
        if max_rehash < MIN_REHASH:
            if max_rehash < best_max_rehash:
                best_max_rehash = max_rehash
                best_z = z
            num_found += 1
            if num_found >= MATCHES:
                break

    z = best_z
    if z == 0:
        sys.stderr.write("makekeys: failed to find small enough hash!\n"
                         "Try increasing KTNUM in makekeys.c\n")
        sys.exit(1)

    for i in range(z):
        offsets[i] = 0
    for i in range(ksnum):
        val = info[i][1]
        first = j = val % z
        while offsets[j]:
            if values[j] == val:
                break
            j += first + 1
            if j >= z:
                j -= z
        if not offsets[j]:
            offsets[j] = indexes[i] + 2
            values[j] = val

    out.write("\n")
    out.write("#ifdef NEEDVTABLE\n")
    out.write("#define VTABLESIZE %d\n" % z)
    out.write("#define VMAXHASH %d\n" % (best_max_rehash + 1))
    out.write("\n")
    out.write("static const unsigned short hashKeysym[VTABLESIZE] = {\n")
    for i in range(z):
        out.write("0x%.4x" % offsets[i])
        i += 1
        if i == z:
            break
        out.write(", " if (i & 7) else ",\n")
    out.write("\n")
    out.write("};\n")
    out.write("#endif /* NEEDVTABLE */\n")

    sys.exit(0)


if __name__ == "__main__":
    main(sys.argv[1:])
