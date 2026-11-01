"""Exercise the native packer and a relocated, self-contained runtime."""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

engine = Path(sys.argv[1]).resolve()
Path("build").mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='Lunoryx ') as root:
    directory = Path(root) / 'Игра с пробелами'
    directory.mkdir()
    project = directory / 'example.lunoryx'
    game = directory / ('GemTrail.exe' if os.name == 'nt' else 'GemTrail')
    subprocess.run([engine, '--create-example', project], check=True, timeout=60)
    document = json.loads(project.read_text(encoding='utf-8'))
    source_assets = directory / 'assets'
    source_assets.mkdir()
    (source_assets / 'pickup.wav').write_bytes(bytes(document['sounds'][0]['data']))
    document['sounds'][0]['data'] = []
    document['sounds'][0]['path'] = 'assets/pickup.wav'
    material = document['objects'][0]['materials'][0]
    (source_assets / 'checker.png').write_bytes(bytes(material['image']))
    material['image'] = []
    material['texture'] = 'assets/checker.png'
    project.write_text(json.dumps(document), encoding='utf-8')
    original = project.read_bytes()
    subprocess.run([engine, '--build-game', project, game], check=True, timeout=60)
    assert project.read_bytes() == original
    mesa = os.environ.get('LUNORYX_MESA_DIR')
    if mesa:
        for dll in Path(mesa).glob('*.dll'):
            shutil.copy2(dll, directory / dll.name)
    payload = game.read_bytes()
    assert payload[-32:-24] == b'LNXPCK01'
    offset, size, crc, version = struct.unpack('<QQII', payload[-24:])
    assert version == 1 and offset + size + 32 == len(payload)
    metadata_size = struct.unpack_from('<Q', payload, offset)[0]
    manifest = json.loads(payload[offset + 8:offset + 8 + metadata_size])
    assert any(entry['name'] == 'project.lunoryx' for entry in manifest)
    project.unlink()  # The runtime must not need the source document or assets.
    shutil.rmtree(source_assets)
    subprocess.run([game, '--verify-package'], check=True, timeout=60)
    damaged = directory / ('Damaged.exe' if os.name == 'nt' else 'Damaged')
    corrupt = bytearray(payload)
    corrupt[offset + 8] ^= 1
    damaged.write_bytes(corrupt)
    if os.name != 'nt':
        damaged.chmod(0o755)
    assert subprocess.run([damaged, '--verify-package'], timeout=60).returncode != 0
    content = payload[offset + 8 + metadata_size:-32]

    def check_manifest(entries, expected_error=None):
        encoded = json.dumps(entries).encode() if not isinstance(entries, bytes) else entries
        invalid_payload = struct.pack('<Q', len(encoded)) + encoded + content
        malformed = payload[:offset] + invalid_payload + b'LNXPCK01' + struct.pack(
            '<QQII', offset, len(invalid_payload), zlib.crc32(invalid_payload), 1)
        damaged.write_bytes(malformed)
        result = subprocess.run([damaged, '--verify-package'], capture_output=True,
                                text=True, timeout=60)
        if expected_error:
            assert result.returncode == 1, (result.returncode, result.stderr)
            assert expected_error in result.stderr, result.stderr
        else:
            assert result.returncode == 0, result.stderr

    invalid_manifest = [dict(entry) for entry in manifest]
    invalid_manifest[0]['offset'] = 999999999
    check_manifest(invalid_manifest, 'exceeds payload')
    # A correct CRC must not hide range aliasing and memory amplification.
    alias = dict(manifest[0], name='alias')
    check_manifest(manifest + [alias], 'Overlapping')
    partial = dict(alias, offset=alias['offset'] + 1, size=alias['size'] - 1)
    check_manifest(manifest + [partial], 'Overlapping')
    check_manifest(manifest + [dict(manifest[0])], 'Duplicate')
    for name in ('.', '../outside', 'a/../b', 'a:stream'):
        check_manifest(manifest + [dict(alias, name=name)], 'resource path')
    check_manifest(b'[' * 1000 + b'0' + b']' * 1000, 'nesting exceeds limit')
    # Disjoint resources need not appear in offset order; empty ranges are legal.
    check_manifest(list(reversed(manifest)) + [dict(alias, name='empty', size=0)])
    if os.environ.get('LUNORYX_GAME_GUI'):
        environment = dict(os.environ, TEST_TIMEOUT='60', LUNORYX_AUDIO_NULL='1', LUNORYX_GAME_SMOKE='1')
        capture = environment.get('LUNORYX_CAPTURE_PATH')
        if capture:
            Path(capture).parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([game], env=environment, check=True, timeout=180)
        if capture:
            assert Path(capture).is_file(), 'Gameplay completed without producing the required screenshot'
    output = Path('build')
    output.mkdir(exist_ok=True)
    shutil.copy2(game, output / 'GemTrail.exe')
print('PASS: native single-executable packer, UTF-8 paths, relocation, removed source, corrupted payload, overlap and nesting rejection')
