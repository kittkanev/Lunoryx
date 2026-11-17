"""Exercise actual authenticated readers at package limits without huge writers."""
import argparse
import hashlib
import hmac
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time

p=argparse.ArgumentParser()
p.add_argument('engine',type=Path)
p.add_argument('--large',action='store_true',help='Also authenticate/read sparse payloads of 1 GiB−1 and 1 GiB')
p.add_argument('--output',type=Path,default=Path('build/evidence/package-stress.json'))
a=p.parse_args();engine=a.engine.resolve()
limit=1024**3;domain=b'Lunoryx authenticated game package v2\0';results=[]
with tempfile.TemporaryDirectory(prefix='Lunoryx package stress ') as temporary:
    root=Path(temporary);key_file=root/'private.key'
    subprocess.run([engine,'--generate-package-key',key_file],check=True,timeout=30)
    key=key_file.read_bytes();assert len(key)==32
    env=dict(os.environ,LUNORYX_PACKAGE_KEY_FILE=str(key_file))

    def footer(length):return b'LNXPCK02'+struct.pack('<QQII',0,length,2,0)

    def check(name,path,valid,error=None):
        start=time.monotonic();process=subprocess.Popen([engine,'--verify-package',path],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        peak=0
        while process.poll() is None:
            if time.monotonic()-start>240:
                process.kill();process.communicate();raise AssertionError(f'{name}: verification deadline exceeded')
            try:
                for line in Path(f'/proc/{process.pid}/status').read_text().splitlines():
                    if line.startswith('VmRSS:'):peak=max(peak,int(line.split()[1]))
            except (FileNotFoundError,ProcessLookupError,PermissionError):pass
            time.sleep(.02)
        stdout,stderr=process.communicate()
        assert (process.returncode==0)==valid,(name,process.returncode,stderr.decode(errors='replace'))
        if error:assert error in stderr.decode(errors='replace'),(name,stderr)
        assert peak<=(limit+256*1024**2)//1024,(name,'RSS exceeded payload + 256 MiB',peak)
        results.append(dict(case=name,passed=True,returncode=process.returncode,seconds=round(time.monotonic()-start,3),peak_rss_kib=peak))

    def sparse_package(length,path):
        blob=length-128
        for _ in range(20):
            entries=[dict(name='project.lunoryx',offset=0,size=2),dict(name='assets/blob',offset=2,size=blob)]
            metadata=json.dumps(entries,separators=(',',':')).encode()
            new=length-8-len(metadata)-2
            if new==blob:break
            blob=new
        else:raise AssertionError('Manifest-length calculation failed to converge')
        header=footer(length);prefix=struct.pack('<Q',len(metadata))+metadata+b'{}'
        mac=hmac.new(key,domain+header,hashlib.sha256);mac.update(prefix)
        zeros=bytes(1024*1024);remaining=blob
        while remaining:
            count=min(remaining,len(zeros));mac.update(memoryview(zeros)[:count]);remaining-=count
        with path.open('wb') as file:
            file.write(prefix);file.seek(length);file.write(header+mac.digest())

    if a.large:
        for length in (limit-1,limit):
            path=root/'large.lnx';sparse_package(length,path);check(f'payload_{length}',path,True);path.unlink()
    path=root/'oversize.lnx'
    with path.open('wb') as file:file.seek(limit+1);file.write(footer(limit+1)+bytes(32))
    check('payload_cap_plus_one',path,False,'Unsupported or truncated')
    path.unlink()
    path=root/'manifest.lnx';size=4*1024*1024+1
    with path.open('wb') as file:file.write(struct.pack('<Q',size));file.seek(size+8);file.write(footer(size+8)+bytes(32))
    check('manifest_cap_plus_one',path,False,'manifest length')
    for count in (4096,4097):
        entries=[dict(name='project.lunoryx',offset=0,size=2)]+[dict(name=f'assets/empty{i}',offset=2,size=0) for i in range(count-1)]
        metadata=json.dumps(entries,separators=(',',':')).encode();payload=struct.pack('<Q',len(metadata))+metadata+b'{}';header=footer(len(payload))
        path=root/'entries.lnx';path.write_bytes(payload+header+hmac.new(key,domain+header+payload,hashlib.sha256).digest())
        check(f'entries_{count}',path,count==4096,None if count==4096 else 'Invalid game manifest')
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(dict(passed=True,large=a.large,cases=results,scope='Authenticated container reads only; no scene execution or driver coverage'),indent=2)+'\n')
print(f'PASS: {len(results)} actual authenticated package boundary cases; large={a.large}')
