"""Emit a CycloneDX inventory of shipped source/dependency and binary hashes."""
import argparse
import datetime
import hashlib
import json
import re
import subprocess
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--root',required=True)
p.add_argument('--output',required=True)
args=p.parse_args()
root=Path(args.root)
version=re.search(r'LUNORYX_VERSION "([^"]+)"',Path('engine_version.h').read_text())[1]
components=[]
for license_file in sorted(Path('docs/licenses').glob('*.txt')):
    components.append(dict(type='library',name=license_file.stem,properties=[
        dict(name='lunoryx:license-notice-sha256',value=hashlib.sha256(license_file.read_bytes()).hexdigest())]))
lock=json.loads(Path('extensions/csharp/packages.lock.json').read_text())
seen=set()
for packages in lock['dependencies'].values():
    for name,package in packages.items():
        key=(name,package.get('resolved'))
        if not key[1] or key in seen:continue
        seen.add(key)
        item=dict(type='library',name=name,version=key[1],purl=f'pkg:nuget/{name}@{key[1]}')
        if package.get('contentHash'):
            item['properties']=[dict(name='lunoryx:nuget-content-sha512-base64',value=package['contentHash'])]
        components.append(item)
for file in sorted(root.rglob('*')):
    if file.is_file() and file.suffix.lower() in ('.exe','.dll','.dylib','.so','.a') or file.is_file() and file.name in ('Lunoryx','Luaryx.Worker'):
        components.append(dict(type='file',name=str(file.relative_to(root)),hashes=[dict(alg='SHA-256',content=hashlib.sha256(file.read_bytes()).hexdigest())]))
document=dict(bomFormat='CycloneDX',specVersion='1.5',version=1,
    metadata=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),component=dict(type='application',name='Lunoryx',version=version,licenses=[dict(license=dict(id='GPL-3.0-only'))]),properties=[dict(name='lunoryx:source-commit',value=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip())]),components=components)
output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(document,indent=2)+'\n')
print(f'Wrote {len(components)} inventory entries: {output}')
