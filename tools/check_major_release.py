"""Reject a major release while the agreed P1/P2 register remains unfinished."""
import argparse
import json
from pathlib import Path
import re


def check(version,register,root):
    if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise ValueError('Expected a numeric release version')
    if tuple(map(int,version.split('.')))<(1,4,0):return
    if version!=register.get('target_version'):raise ValueError('Release version differs from the agreed update register')
    features=[item for item in register['items'] if item['id'].startswith(('P1-','P2-'))]
    if not features or len({item['id'] for item in features})!=len(features):raise ValueError('Missing or duplicated feature register')
    unfinished=[]
    for item in features:
        evidence=item.get('evidence',{})
        tests=evidence.get('tests',[]);validation=evidence.get('validation')
        if item['status']!='fixed' or not isinstance(tests,list) or not tests or not validation:
            unfinished.append(item['id']);continue
        for name in tests+[validation]:
            if not isinstance(name,str) or not name or not (root/name).is_file() or not (root/name).resolve().is_relative_to(root.resolve()):
                raise ValueError('Missing local feature evidence: '+item['id'])
    if unfinished:raise ValueError('Major update is incomplete: '+', '.join(unfinished))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=Path('.'));args=parser.parse_args()
    try:
        version=re.search(r'LUNORYX_VERSION "([^"]+)"',(args.root/'engine_version.h').read_text())[1]
        check(version,json.loads((args.root/'docs/validation/major-update-register.json').read_text()),args.root)
    except (ValueError,KeyError,TypeError) as error:parser.exit(1,str(error)+'\n')
    print('PASS: release scope gate (legacy release or completed major register)')
