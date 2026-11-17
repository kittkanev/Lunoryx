"""CI-only credential readiness. Never print credential values."""
import argparse
import json
import os
from pathlib import Path

FIELDS={
    'windows':('LUNORYX_WINDOWS_PFX_BASE64','LUNORYX_WINDOWS_PFX_PASSWORD','LUNORYX_WINDOWS_TIMESTAMP_URL'),
    'macos':('LUNORYX_MACOS_P12_BASE64','LUNORYX_MACOS_P12_PASSWORD','LUNORYX_MACOS_SIGN_IDENTITY',
             'LUNORYX_NOTARY_KEY_BASE64','LUNORYX_NOTARY_KEY_ID','LUNORYX_NOTARY_ISSUER_ID'),
}


def readiness(platform,environment):
    fields=FIELDS[platform];present=[name for name in fields if environment.get(name)]
    if not present:return dict(mode='unsigned' if platform=='windows' else 'ad_hoc',credentials_present=False,publisher_verified=False)
    if len(present)!=len(fields):raise ValueError('Incomplete signing configuration; missing: '+', '.join(name for name in fields if name not in present))
    if platform=='windows' and not environment['LUNORYX_WINDOWS_TIMESTAMP_URL'].startswith('https://'):
        raise ValueError('Windows timestamp URL must use HTTPS')
    if platform=='macos' and not environment['LUNORYX_MACOS_SIGN_IDENTITY'].startswith('Developer ID Application:'):
        raise ValueError('macOS identity must be a Developer ID Application certificate')
    return dict(mode='configured_unverified',credentials_present=True,publisher_verified=False)


def artifact_status(platform,path,source_sha):
    if not path or not Path(path).is_file():
        return dict(status='unsigned' if platform=='windows' else 'ad_hoc',publisher_verified=False)
    value=json.loads(Path(path).read_text(encoding='utf-8-sig'))
    if value.get('source_sha')!=source_sha or type(value.get('publisher_verified')) is not bool:
        raise ValueError('Signing metadata is not bound to the verified source')
    status=value.get('status')
    if not value['publisher_verified']:
        if status!=('unsigned' if platform=='windows' else 'ad_hoc'):raise ValueError('Unverified publisher metadata must not claim a signature')
    elif platform=='windows':
        expected={'engine.exe','Runtime/Luaryx.Worker.exe','Runtime/Luaryx.Worker.dll'}
        files=value.get('files',[])
        if status!='authenticode' or {f.get('name') for f in files}!=expected or not all(f.get('status')=='valid' and f.get('timestamped') is True for f in files):
            raise ValueError('Incomplete Authenticode verification metadata')
    elif status!='developer_id_notarized' or value.get('stapled') is not True or not value.get('submission_id') or not value.get('identity','').startswith('Developer ID Application:'):
        raise ValueError('Incomplete Developer ID/notarization metadata')
    return value


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('platform',choices=FIELDS)
    args=parser.parse_args()
    try:print(json.dumps(readiness(args.platform,os.environ)))
    except ValueError as error:parser.exit(2,str(error)+'\n')
