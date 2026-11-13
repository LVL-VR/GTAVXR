"""Build synthetic delta fixtures with a locally supplied Zstandard DLL."""

import argparse
from compression import zstd
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile


def identity(data):
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest().upper()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('codec', type=Path)
    args = parser.parse_args()
    args.output.mkdir()
    base = b''.join(hashlib.sha256(bytes([index])).digest() for index in range(256))
    target = base[200:2000] + b'owned fixture changed bytes' + base[3000:6000]
    patch = zstd.compress(target, options={zstd.CompressionParameter.checksum_flag: 1},
                          zstd_dict=zstd.ZstdDict(base, is_raw=True).as_prefix)
    launch = {'id': 'synthetic-v1', 'schema': 'gtavxr-suite2-launch-v1', 'session_mode': 'suite',
              'settings_mode': 'suite', 'args': ['-logfile', '{LOGFILE}']}
    data = [('patch', 'target.zst', patch), ('codec', 'libzstd.dll', args.codec.read_bytes()),
            ('metadata', 'suite-launch.json', json.dumps(launch).encode()),
            ('payload', 'common/example.dat', b'owned synthetic data')]
    game = args.output / 'synthetic-game'
    game.mkdir()
    (game / 'GTA5.exe').write_bytes(base)
    (args.output / 'expected.json').write_text(json.dumps(identity(target)), encoding='utf-8')

    for case in ('valid', 'bad-checksum', 'trailing-frame', 'wrong-target', 'wrong-size',
                 'wrong-base', 'member-drift', 'duplicate', 'traversal', 'extra-member'):
        rows = data.copy()
        if case == 'bad-checksum':
            rows[0] = ('patch', 'target.zst', patch[:-1] + bytes((patch[-1] ^ 1,)))
        if case == 'trailing-frame':
            rows[0] = ('patch', 'target.zst', patch + patch)
        manifest = {'schema': 'gtavxr-embedded-release-v1', 'id': 'synthetic-v1', 'base': identity(base),
                    'target': identity(target), 'entries': [dict(identity(value), path=path, kind=kind) for kind, path, value in rows]}
        if case == 'wrong-target':
            manifest['target']['sha256'] = '0' * 64
        if case == 'wrong-size':
            manifest['target']['bytes'] += 1
        if case == 'wrong-base':
            manifest['base']['sha256'] = '0' * 64
        with ZipFile(args.output / (case + '.zip'), 'x', allowZip64=False) as archive:
            for _, path, value in rows:
                if case == 'member-drift' and path == 'common/example.dat':
                    value = value[::-1]
                archive.writestr(path, value)
            if case == 'duplicate':
                archive.writestr('Common/Example.dat', b'duplicate')
            if case == 'traversal':
                archive.writestr('../escape.dat', b'escape')
            if case == 'extra-member':
                archive.writestr('unknown.dat', b'extra')
            archive.writestr('manifest.json', json.dumps(manifest))


if __name__ == '__main__':
    main()
