"""Build a relocatable, ad-hoc-signed macOS app from native build outputs."""
import argparse
import hashlib
import json
import platform
import plistlib
import shutil
import subprocess
from pathlib import Path
from macos_bundle_checks import is_macho, read_version, validate_dependencies

p = argparse.ArgumentParser()
p.add_argument('--output', default='build/macos/Lunoryx.app')
p.add_argument('--runtime', required=True)
args = p.parse_args()
version = read_version(Path('engine_version.h').read_text())
app = Path(args.output)
if app.exists():
    raise SystemExit(f'Refusing to overwrite {app}')
contents = app / 'Contents'
(contents / 'MacOS').mkdir(parents=True)
resources = contents / 'Resources'
resources.mkdir()
shutil.copy2('engine', contents / 'MacOS/Lunoryx')
info = dict(CFBundleIdentifier='org.lunoryx.studio', CFBundleName='Lunoryx',
            CFBundleDisplayName='Lunoryx Studio', CFBundleExecutable='Lunoryx',
            CFBundlePackageType='APPL', CFBundleShortVersionString=version,
            CFBundleVersion=version, LSMinimumSystemVersion='15.0',
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
if iconset.is_symlink():iconset.unlink()
elif iconset.exists():shutil.rmtree(iconset)
iconset.mkdir()
try:
    for size in (16, 32, 128, 256, 512):
        for scale in (1, 2):
            suffix = '@2x' if scale == 2 else ''
            subprocess.run(['sips', '-z', str(size*scale), str(size*scale),
                            'assets/lunoryx-logo.png', '--out',
                            str(iconset / f'icon_{size}x{size}{suffix}.png')], check=True,
                           stdout=subprocess.DEVNULL)
    subprocess.run(['iconutil', '-c', 'icns', str(iconset), '-o', str(resources/'Lunoryx.icns')], check=True)
finally:
    shutil.rmtree(iconset)
subprocess.run(['dylibbundler', '-od', '-b', '-x', str(contents/'MacOS/Lunoryx'),
                '-d', str(contents/'Frameworks'), '-p', '@executable_path/../Frameworks/'], check=True)
# Homebrew now supplies SDL2 through sdl2-compat, which dlopens SDL3.
# Bundle that hidden dependency explicitly; otool alone cannot discover it.
try:
    sdl3_prefix=subprocess.check_output(['brew','--prefix','sdl3'],text=True).strip()
    sdl3=Path(sdl3_prefix)/'lib/libSDL3.dylib'
    if sdl3.exists():
        target=contents/'Frameworks/libSDL3.dylib'
        shutil.copy2(sdl3,target)
        subprocess.run(['dylibbundler','-of','-b','-x',str(target),'-d',str(contents/'Frameworks'),'-p','@loader_path/'],check=True)
        subprocess.run(['install_name_tool','-id','@loader_path/libSDL3.dylib',str(target)],check=True)
except subprocess.CalledProcessError:
    raise SystemExit('Cannot bundle the SDL3 dependency required by sdl2-compat')
main_commands=subprocess.check_output(['otool','-l',str(contents/'MacOS/Lunoryx')],text=True)
worker=resources/'CSharp'/arch/'Luaryx.Worker'
worker_commands=subprocess.check_output(['otool','-l',str(worker)],text=True)
for native in (f for f in app.rglob('*') if f.is_file() and is_macho(f)):
    dependencies=subprocess.check_output(['otool','-L',str(native)],text=True)
    commands=subprocess.check_output(['otool','-l',str(native)],text=True)
    managed=native.is_relative_to(resources/'CSharp')
    validate_dependencies(native,app,dependencies,commands,
                          worker_commands if managed else main_commands,
                          worker if managed else contents/'MacOS/Lunoryx')
# Sign inner native libraries first, then the enclosing bundle. No publisher
# certificate is implied: this is an explicit ad-hoc signature for local use.
for library in sorted((contents/'Frameworks').glob('*.dylib')):
    subprocess.run(['codesign', '--force', '--sign', '-', str(library)], check=True)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
hashes = {str(f.relative_to(app)): hashlib.sha256(f.read_bytes()).hexdigest()
          for f in app.rglob('*') if f.is_file()}
Path(str(app)+'.sha256.json').write_text(json.dumps(hashes, indent=2)+'\n')
