"""Rebuild the Win64 Trainer release from the files next to this script."""

import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess


MODULE = Path(__file__).resolve().parent
DIAGNOSTIC_NAME = 'trainer/' + 'x' * 125 + '/pugixml.cpp'
RELEASE_TIMESTAMP = 0x6AEFCA2E
RELEASE_SHA256 = 'ddb783dd8eca25e23d440ea9419b4a82377150ba92f2fca78b95625e6da71483'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def seal(data):
    """Normalize the compiler diagnostic name and two COFF timestamps."""
    pe, = struct.unpack_from('<I', data, 60)
    require(data[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H', data, pe+4)[0] == 0x8664,
            'Expected a Win64 PE')
    section_count, timestamp = struct.unpack_from('<HI', data, pe+6)
    optional_size, = struct.unpack_from('<H', data, pe+20)
    optional = pe + 24
    require(struct.unpack_from('<H', data, optional)[0] == 0x20B, 'Expected PE32+')
    require(struct.unpack_from('<I', data, optional+64)[0] == 0, 'Unexpected PE checksum')
    require(struct.unpack_from('<II', data, optional+112+4*8) == (0, 0), 'Unexpected PE signature')
    begin = optional + optional_size
    sections = []
    for index in range(section_count):
        name, _, rva, size, raw = struct.unpack_from('<8sIIII', data, begin+index*40)
        sections.append((name.rstrip(b'\0'), rva, size, raw))
    _, _, size, raw = next(s for s in sections if s[0] == b'.rdata')
    rdata = data[raw:raw+size]
    replacements = 0
    for encoding, zero in (('ascii', b'\0'), ('utf-16le', b'\0\0')):
        name = DIAGNOSTIC_NAME.encode(encoding) + zero
        replacement = '<diagnostic>'.encode(encoding).ljust(len(name), b'\0')
        replacements += rdata.count(name)
        rdata = rdata.replace(name, replacement)
    require(replacements == 76, 'Diagnostic layout changed')
    data[raw:raw+size] = rdata

    debug_rva, debug_size = struct.unpack_from('<II', data, optional+112+6*8)
    _, rva, _, raw = next(s for s in sections if s[1] <= debug_rva < s[1]+s[2])
    begin = raw + debug_rva - rva
    timestamps = [pos+4 for pos in range(begin, begin+debug_size, 28)
                  if struct.unpack_from('<I', data, pos+4)[0] == timestamp]
    require(len(timestamps) == 1, 'Unexpected debug timestamp layout')
    for offset in (pe+8, *timestamps):
        struct.pack_into('<I', data, offset, RELEASE_TIMESTAMP)
    require(hashlib.sha256(data).hexdigest() == RELEASE_SHA256, 'The build differs from the Trainer release')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='Fresh output directory')
    parser.add_argument('--vcvars', type=Path, required=True, help='MSVC 14.51.36248 x64 vcvars64.bat')
    args = parser.parse_args()
    vcvars = args.vcvars.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir()
    pugi = output / 'pugixml-input'
    shutil.copytree(MODULE / 'pugixml', pugi)
    cpp = pugi / 'pugixml.cpp'
    cpp.write_bytes(f'#line 1 "{DIAGNOSTIC_NAME}"\r\n'.encode('ascii') + cpp.read_bytes())
    compile_dir = output / 'compile'
    subprocess.run([str(MODULE / 'build_trainer.cmd'), str(MODULE / 'module'), str(pugi), str(compile_dir), str(vcvars)],
                   check=True, cwd=MODULE)
    sealed = seal(bytearray((compile_dir / 'gtavxr_trainer.dll').read_bytes()))
    with (output / 'gtavxr_trainer.dll').open('xb') as stream:
        stream.write(sealed)
    print(f'Verified release SHA-256 {RELEASE_SHA256.upper()}')


if __name__ == '__main__':
    main()
