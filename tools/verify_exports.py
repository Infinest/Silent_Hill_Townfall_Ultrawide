#!/usr/bin/env python3
"""Compare the export table (ordinal, name) of a built proxy against the real
system DLL. Usage: verify_exports.py <target> (e.g. dxgi)"""
import struct
import subprocess
import sys


def u16(d, o):
    return struct.unpack_from("<H", d, o)[0]


def u32(d, o):
    return struct.unpack_from("<I", d, o)[0]


def pe_exports(path):
    d = open(path, "rb").read()
    pe = u32(d, 0x3C)
    opt = pe + 24
    dd_base = opt + (112 if u16(d, opt) == 0x20B else 96)
    exp_rva = u32(d, dd_base)
    sect_off = opt + u16(d, pe + 20)
    sections = []
    for i in range(u16(d, pe + 6)):
        s = sect_off + 40 * i
        sections.append((u32(d, s + 12), u32(d, s + 8), u32(d, s + 20), u32(d, s + 16)))

    def off(rva):
        for va, vsz, raw, rsz in sections:
            if va <= rva < va + max(vsz, rsz):
                return raw + (rva - va)
        return None

    eo = off(exp_rva)
    base = u32(d, eo + 16)
    n_names = u32(d, eo + 24)
    names, ords = off(u32(d, eo + 32)), off(u32(d, eo + 36))
    out = {}
    for i in range(n_names):
        noff = off(u32(d, names + 4 * i))
        name = d[noff : d.index(b"\0", noff)].decode("ascii")
        out[base + u16(d, ords + 2 * i)] = name
    return out


def dumpbin_exports(path):
    dumpbin = r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe"
    out = subprocess.run([dumpbin, "/exports", path], capture_output=True, text=True).stdout
    exports = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[0].isdigit() and all(c in "0123456789ABCDEF" for c in parts[1]):
            exports[int(parts[0])] = parts[3] if parts[2] != "[NONAME]" else None
    return exports


t = sys.argv[1]
real = pe_exports(r"C:\Windows\System32\%s.dll" % t)
proxy = dumpbin_exports(r"dist\%s.dll" % t)
missing = {o: n for o, n in real.items() if o not in proxy}
extra = {o: n for o, n in proxy.items() if o not in real}
wrongname = {o: (n, proxy.get(o)) for o, n in real.items() if o in proxy and proxy[o] != n}
if missing or extra or wrongname:
    print("%s: MISMATCH missing=%d extra=%d wrongname=%d" % (t, len(missing), len(extra), len(wrongname)))
    for o, n in list(missing.items())[:5]:
        print("  missing @%d %s" % (o, n))
    for o, n in list(extra.items())[:5]:
        print("  extra @%d %s" % (o, n))
    for o, p in list(wrongname.items())[:5]:
        print("  wrongname @%d real=%s proxy=%s" % (o, p[0], p[1]))
    sys.exit(1)
print("%s: identical (%d exports)" % (t, len(real)))
