"""Merge independently tested arm64/x64 apps; preserve both managed runtimes."""
import argparse
import shutil
import subprocess
import tarfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('arm64')
p.add_argument('x64')
p.add_argument('--output', default='build/universal/Lunoryx.app')
args = p.parse_args()
staging = Path('build/macos-merge')
for architecture, archive in [('arm', args.arm64), ('intel', args.x64)]:
    destination = staging/architecture
    destination.mkdir(parents=True, exist_ok=False)
    with tarfile.open(archive) as bundle:
        bundle.extractall(destination, filter='data')
arm, intel = staging/'arm/Lunoryx.app', staging/'intel/Lunoryx.app'
app = Path(args.output)
shutil.copytree(arm, app, symlinks=True)
shutil.copytree(intel/'Contents/Resources/CSharp/x64', app/'Contents/Resources/CSharp/x64', symlinks=True)
native = [Path('Contents/MacOS/Lunoryx')]
native.extend(f.relative_to(arm) for f in (arm/'Contents/Frameworks').glob('*.dylib'))
for relative in native:
    if not (intel/relative).exists():
        raise SystemExit(f'Native library missing from Intel build: {relative}')
    subprocess.run(['lipo', '-create', str(arm/relative), str(intel/relative),
                    '-output', str(app/relative)], check=True)
    subprocess.run(['lipo', str(app/relative), '-verify_arch', 'arm64', 'x86_64'], check=True)
    subprocess.run(['codesign', '--force', '--sign', '-', str(app/relative)], check=True)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
