"""Deterministic project mutation/load/undo soak in one native process (POSIX).

This measures the CPU parser/editor document path, RSS and open descriptors.
It is not a GPU leak proof or a coverage-guided fuzzer.
"""
import argparse
import copy
import json
import os
import random
import select
import subprocess
import tempfile
import time
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('binary')
p.add_argument('--cycles',type=int,default=1000)
p.add_argument('--seconds',type=int,default=0)
p.add_argument('--output',default='build/evidence/parser-soak.json')
args=p.parse_args();binary=str(Path(args.binary).resolve());rng=random.Random(1222026)
started=time.monotonic();samples=[];accepted=rejected=0
with tempfile.TemporaryDirectory(prefix='lunoryx-parser-') as folder:
    root=Path(folder);seed=root/'seed.lunoryx';probe=root/'probe.lunoryx'
    subprocess.run([binary,'--create-example',str(seed)],check=True,timeout=15)
    original=json.loads(seed.read_text());pending=b'';counter=0
    process=subprocess.Popen([binary,'--mcp','--allow-write'],cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
    def call(name,arguments):
        global counter,pending
        counter+=1;request=dict(jsonrpc='2.0',id=counter,method='tools/call',params=dict(name=name,arguments=arguments))
        process.stdin.write((json.dumps(request)+'\n').encode());process.stdin.flush();deadline=time.monotonic()+5
        while b'\n' not in pending:
            remaining=deadline-time.monotonic()
            if remaining<=0 or not select.select([process.stdout],[],[],remaining)[0]:raise TimeoutError(f'Parser stalled at case {counter}; seed={probe}')
            block=os.read(process.stdout.fileno(),8192)
            if not block:raise RuntimeError(f'Parser exited at case {counter}')
            pending+=block
            if len(pending)>1024*1024:raise RuntimeError('Parser response budget exceeded')
        line,pending=pending.split(b'\n',1);return json.loads(line)['result']
    try:
        # Small serialized fields can otherwise create a very large DOM.
        amplification=json.dumps(original).encode()[:-1]+b',"ignored":['+b'{"":0},'*499999+b'{"":0}]}'
        probe.write_bytes(amplification);large=call('project_load',dict(path=str(probe)))
        assert large.get('isError') and 'budget' in json.dumps(large)
        amplification_size=len(amplification);del amplification
        i=0
        while i<args.cycles or time.monotonic()-started<args.seconds:
            document=copy.deepcopy(original);kind=i%8
            if kind==0:document['objects'][0]['position']=[rng.uniform(-10,10),0,0]
            elif kind==1:document['schema']=2**64-1
            elif kind==2:document['objects'][0]['position']=[1e300,0,0]
            elif kind==3:document['objects'][0]['properties']={'x':{'kind':4}}
            elif kind==4:document['scripts'][0]['code']='-- UTF-8 Привет\n'+('x'*rng.randrange(32,2048))
            elif kind==5:document['ignored']=[{'tiny':j} for j in range(rng.randrange(1,300))]
            elif kind==6:document['objects'][0]['indices']=[2**32,1,2]
            encoded=json.dumps(document,ensure_ascii=False).encode()
            if kind==7:encoded=encoded[:rng.randrange(len(encoded))]
            probe.write_bytes(encoded)
            result=call('project_load',dict(path=str(probe)))
            if result.get('isError'):rejected+=1
            else:accepted+=1
            if i%10==0:
                call('add_object',dict(name='soak',type='cube'));call('undo_objects',{});call('redo_objects',{})
            if i%50==0:
                status=Path(f'/proc/{process.pid}/status')
                if status.exists():
                    rss=next(int(line.split()[1]) for line in status.read_text().splitlines() if line.startswith('VmRSS:'))
                    samples.append(dict(cycle=i,rss_kib=rss,descriptors=len(list(Path(f'/proc/{process.pid}/fd').iterdir()))))
            i+=1
        process.stdin.close();process.wait(timeout=10)
        assert process.returncode==0
        if len(samples)>3:
            assert samples[-1]['rss_kib']-samples[2]['rss_kib']<128*1024,samples
            assert samples[-1]['descriptors']<=samples[2]['descriptors']+4,samples
    finally:
        if process.poll() is None:process.kill();process.wait()
report=dict(passed=True,cycles=i,accepted=accepted,rejected=rejected,
    amplification_bytes=amplification_size,seconds=round(time.monotonic()-started,3),seed=1222026,samples=samples,
    scope='One native process: project parser, document replacement and undo/redo; no GPU or hour-long UI claim')
output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
print(f'PASS: {i} mutation/load cycles, {accepted} accepted, {rejected} rejected; {output}')
