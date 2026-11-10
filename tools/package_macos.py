"""Build a relocatable, ad-hoc-signed macOS app from native build outputs."""
import argparse
import hashlib
import json
import platform
import plistlib
import re
import shutil
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--output', default='build/macos/Lunoryx.app')
p.add_argument('--runtime', required=True)
args = p.parse_args()
app = Path(args.output)
if app.exists():
    raise SystemExit(f'Refusing to overwrite {app}')
contents = app / 'Contents'
(contents / 'MacOS').mkdir(parents=True)
resources = contents / 'Resources'
resources.mkdir()
shutil.copy2('engine', contents / 'MacOS/Lunoryx')
version = re.search(r'LUNORYX_VERSION "([^"]+)"', Path('engine_version.h').read_text())[1]
info = dict(CFBundleIdentifier='org.lunoryx.studio', CFBundleName='Lunoryx',
            CFBundleDisplayName='Lunoryx Studio', CFBundleExecutable='Lunoryx',
            CFBundlePackageType='APPL', CFBundleShortVersionString=version,
            CFBundleVersion=version, LSMinimumSystemVersion='13.0',
            NSHighResolutionCapable=True, CFBundleIconFile='Lunoryx.icns')
with (contents / 'Info.plist').open('wb') as f:
    plistlib.dump(info, f)
arch = 'arm64' if platform.machine() == 'arm64' else 'x64'
shutil.copytree(args.runtime, resources / 'CSharp' / arch)
for folder in ['docs', 'examples']:
    shutil.copytree(folder, resources / folder)
shutil.copytree('assets/platforms', resources/'assets/platforms')
shutil.copytree('assets/readme', resources/'assets/readme')
shutil.copy2('assets/lunoryx-logo.png', resources/'assets/lunoryx-logo.png')
for name in ['LICENSE', 'NOTICE', 'README.md', 'README.ru.md']:
    shutil.copy2(name, resources / name)
iconset = Path('build/Lunoryx.iconset')
iconset.mkdir(exist_ok=True)
for size in (16, 32, 128, 256, 512):
    for scale in (1, 2):
        suffix = '@2x' if scale == 2 else ''
        subprocess.run(['sips', '-z', str(size*scale), str(size*scale),
                        'assets/lunoryx-logo.png', '--out',
                        str(iconset / f'icon_{size}x{size}{suffix}.png')], check=True,
                       stdout=subprocess.DEVNULL)
subprocess.run(['iconutil', '-c', 'icns', str(iconset), '-o', str(resources/'Lunoryx.icns')], check=True)
subprocess.run(['dylibbundler', '-od', '-b', '-x', str(contents/'MacOS/Lunoryx'),
                '-d', str(contents/'Frameworks'), '-p', '@executable_path/../Frameworks/'], check=True)
# Sign inner native libraries first, then the enclosing bundle. No publisher
# certificate is implied: this is an explicit ad-hoc signature for local use.
for library in sorted((contents/'Frameworks').glob('*.dylib')):
    subprocess.run(['codesign', '--force', '--sign', '-', str(library)], check=True)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
hashes = {str(f.relative_to(app)): hashlib.sha256(f.read_bytes()).hexdigest()
          for f in app.rglob('*') if f.is_file()}
Path(str(app)+'.sha256.json').write_text(json.dumps(hashes, indent=2)+'\n')
