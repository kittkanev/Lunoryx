"""Package the tested Windows editor and its real managed runtime."""
import argparse
import hashlib
import shutil
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--artifact-dir',required=True)
p.add_argument('--output-dir',required=True)
a=p.parse_args();artifact=Path(a.artifact_dir);output=Path(a.output_dir)
binary=(artifact/'engine.exe').read_bytes()
assert binary[:2]==b'MZ'
pe=struct.unpack_from('<I',binary,0x3c)[0]
assert binary[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',binary,pe+4)[0]==0x8664
assert struct.unpack_from('<H',binary,pe+24+68)[0]==2,'Expected a Windows GUI executable'
assert (artifact/'Runtime/Luaryx.Worker.exe').is_file(),'Self-contained C# runtime is required'
stage=output/'windows-stage/Lunoryx';stage.mkdir(parents=True,exist_ok=False)
shutil.copy2(artifact/'engine.exe',stage/'Lunoryx.exe')
for name in ['README.md','README.ru.md','LICENSE','NOTICE']:
    shutil.copy2(name,stage/name)
for folder in ['Runtime','examples','docs','assets','licenses']:
    shutil.copytree(artifact/folder,stage/folder)
commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
(stage/'BUILD.txt').write_text(f'Lunoryx source commit: {commit}\nSource: https://github.com/kittkanev/Lunoryx\nUnzip the whole folder; keep Runtime next to Lunoryx.exe.\n')
subprocess.run([sys.executable,'tools/generate_sbom.py','--root',str(stage),'--output',str(stage/'SBOM.cdx.json')],check=True)
(stage/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.relative_to(stage).as_posix()+'\n' for f in sorted(stage.rglob('*')) if f.is_file()))
archive=output/'Lunoryx-windows-x64.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as bundle:
    for f in sorted(stage.rglob('*')):
        if f.is_file():bundle.write(f,Path('Lunoryx')/f.relative_to(stage))
print(f'Packaged tested editor + runtime: {archive}')
