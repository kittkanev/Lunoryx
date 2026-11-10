"""Exercise the shipped .NET/Roslyn worker without an installed SDK."""
import json
import subprocess
import sys
from pathlib import Path

worker=str(Path(sys.argv[1]).resolve())
source='''using Luaryx;
public class Smoke:Script {
public override void OnStart(){var cube=Scene.Create("Managed");cube.Position=new Vector3(1,2,3);Scene.Emit("ready",17);}
}'''
request=dict(action='compile',scene=[],nextId=0,scripts=[dict(name='Smoke.cs',code=source,module=False)])
result=subprocess.run([worker],input=json.dumps(request)+'\n',text=True,encoding='utf-8',capture_output=True,timeout=30)
assert result.returncode==0,(result.returncode,result.stderr)
reply=json.loads(result.stdout)
assert reply.get('ok'),reply
assert reply['commands'][0]['name']=='Managed',reply
assert reply['commands'][1]['position']==[1,2,3],reply
assert reply['messages'][0]['value']==17,reply
request['scripts'][0]['code']='public class Broken { INVALID }'
result=subprocess.run([worker],input=json.dumps(request)+'\n',text=True,encoding='utf-8',capture_output=True,timeout=30,check=True)
reply=json.loads(result.stdout)
assert not reply['ok'] and reply['diagnostics'][0]['file']=='Smoke.cs',reply
print('PASS: shipped C# worker, real Roslyn compile, scene commands, messages and source diagnostics')
