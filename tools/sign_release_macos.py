"""Optional CI-only Developer ID signing and Apple notarization."""
import argparse
import base64
import json
import os
from pathlib import Path
import plistlib
import re
import secrets
import subprocess
import tempfile

from macos_bundle_checks import is_macho
from signing_policy import readiness


def run(arguments,*,stdout_only=False):
    result=subprocess.run(arguments,capture_output=True,text=True)
    if result.returncode:
        # security commands carry secrets in arguments: never stringify the
        # command or blindly print its stdout/stderr on failure.
        raise RuntimeError(f'{Path(arguments[0]).name} signing/verification command failed ({result.returncode})')
    return result.stdout if stdout_only else result.stdout+result.stderr


def secret_file(path,value):
    descriptor=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(descriptor,'wb') as file:file.write(base64.b64decode(value,validate=True))


def sign(app,report):
    policy=readiness('macos',os.environ)
    result=dict(status='ad_hoc',publisher_verified=False,source_sha=os.environ.get('GITHUB_SHA'),reason='No Developer ID/notary credentials configured')
    if not policy['credentials_present']:
        run(['codesign','--verify','--deep','--strict',str(app)])
    else:
        if not os.environ.get('RUNNER_TEMP'):raise RuntimeError('Signing is a CI-only operation')
        with tempfile.TemporaryDirectory(prefix='lunoryx-sign-',dir=os.environ['RUNNER_TEMP']) as folder:
            root=Path(folder);p12=root/'identity.p12';p8=root/'notary.p8';chain=root/'signing.keychain-db';password=secrets.token_hex(32)
            secret_file(p12,os.environ['LUNORYX_MACOS_P12_BASE64']);secret_file(p8,os.environ['LUNORYX_NOTARY_KEY_BASE64'])
            try:
                run(['security','create-keychain','-p',password,str(chain)])
                run(['security','unlock-keychain','-p',password,str(chain)])
                run(['security','import',str(p12),'-k',str(chain),'-P',os.environ['LUNORYX_MACOS_P12_PASSWORD'],'-T','/usr/bin/codesign'])
                run(['security','set-key-partition-list','-S','apple-tool:,apple:,codesign:','-s','-k',password,str(chain)])
                identities=run(['security','find-identity','-v','-p','codesigning',str(chain)])
                identity=os.environ['LUNORYX_MACOS_SIGN_IDENTITY']
                matches=[match[1] for match in re.finditer(r'\b([A-Fa-f0-9]{40}) "([^"]+)"',identities) if match[2]==identity]
                if len(matches)!=1:raise RuntimeError('Expected exactly one matching Developer ID identity')
                signer=matches[0];entitlements=root/'worker-entitlements.plist'
                with entitlements.open('wb') as file:plistlib.dump({'com.apple.security.cs.allow-jit':True,'com.apple.security.cs.allow-unsigned-executable-memory':True,'com.apple.security.cs.disable-library-validation':True},file)
                for native in sorted((file for file in app.rglob('*') if file.is_file() and is_macho(file)),key=lambda file:len(file.parts),reverse=True):
                    command=['codesign','--force','--options','runtime','--timestamp','--sign',signer,'--keychain',str(chain)]
                    if native.name=='Luaryx.Worker':command+=['--entitlements',str(entitlements)]
                    run(command+[str(native)])
                run(['codesign','--force','--options','runtime','--timestamp','--sign',signer,'--keychain',str(chain),str(app)])
                run(['codesign','--verify','--deep','--strict',str(app)])
                description=run(['codesign','-dvv',str(app)])
                if 'Authority='+identity not in description or 'Signature=adhoc' in description:raise RuntimeError('Developer ID publisher verification failed')
                submission=root/'submission.zip';run(['ditto','-c','-k','--keepParent',str(app),str(submission)])
                answer=json.loads(run(['xcrun','notarytool','submit',str(submission),'--key',str(p8),'--key-id',os.environ['LUNORYX_NOTARY_KEY_ID'],'--issuer',os.environ['LUNORYX_NOTARY_ISSUER_ID'],'--wait','--timeout','30m','--output-format','json'],stdout_only=True))
                if answer.get('status')!='Accepted':raise RuntimeError('Apple notarization was not accepted')
                run(['xcrun','stapler','staple',str(app)]);run(['xcrun','stapler','validate',str(app)])
                run(['codesign','--verify','--deep','--strict',str(app)]);run(['spctl','--assess','--type','execute',str(app)])
                result=dict(status='developer_id_notarized',publisher_verified=True,source_sha=os.environ.get('GITHUB_SHA'),identity=identity,submission_id=answer.get('id'),stapled=True)
            finally:
                if chain.exists():run(['security','delete-keychain',str(chain)])
    report.write_text(json.dumps(result,indent=2)+'\n');print('Signing status: '+result['status'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('app',type=Path);parser.add_argument('--report',type=Path,required=True);args=parser.parse_args()
    try:sign(args.app,args.report)
    except Exception as error:parser.exit(1,type(error).__name__+': signing preparation/verification failed; credentials were not logged\n')
