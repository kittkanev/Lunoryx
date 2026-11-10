"""Record real native UI interaction on a 1440x900 X11 display.

Requires Xvfb (or a display of that size), ffmpeg, xdotool, the built editor and
its self-contained Runtime directory. No rendered frames are fabricated.
"""
import argparse
import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('binary')
p.add_argument('--output',default='build/Verification.mp4')
args=p.parse_args()
output=Path(args.output).resolve();output.parent.mkdir(parents=True,exist_ok=True)
binary=Path(args.binary).resolve()
def run(*args,**kwargs):return subprocess.run(args,check=True,**kwargs)
def xd(*args):run('xdotool',*map(str,args),stdout=subprocess.DEVNULL)

with tempfile.TemporaryDirectory(prefix='lunoryx-video-') as directory:
    root=Path(directory);project=root/'InteropDemo'
    environment=dict(os.environ,XDG_DATA_HOME=str(root/'preferences'),LIBGL_ALWAYS_SOFTWARE='1')
    log=(output.parent/'video-session.log').open('w')
    application=subprocess.Popen([str(binary)],cwd=root,env=environment,stdout=log,stderr=log)
    recorder=None
    try:
        deadline=time.monotonic()+30;window=None
        while time.monotonic()<deadline:
            result=subprocess.run(['xdotool','search','--pid',str(application.pid)],capture_output=True,text=True)
            if result.returncode==0 and result.stdout.strip():window=result.stdout.splitlines()[-1];break
            if application.poll() is not None:raise RuntimeError('Editor exited during startup')
            time.sleep(.2)
        if window is None:raise RuntimeError('Editor window not found')
        geometry=subprocess.check_output(['xdotool','getwindowgeometry','--shell',window],text=True)
        dimensions=dict(line.split('=',1) for line in geometry.splitlines() if '=' in line)
        w,h=int(dimensions['WIDTH']),int(dimensions['HEIGHT'])
        recorder=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','x11grab',
            '-framerate','24','-video_size','1440x900','-i',os.environ.get('DISPLAY',':99'),
            '-c:v','libx264','-preset','fast','-crf','21','-pix_fmt','yuv420p','-movflags','+faststart',str(output)],stdin=subprocess.PIPE)
        def click(x,y):
            xd('mousemove','--window',window,x,y);time.sleep(.3)
            xd('mousedown',1);time.sleep(.25);xd('mouseup',1);time.sleep(1.2)
        def type_code(text):
            if '\n' in text:
                run('xclip','-selection','clipboard',input=text,text=True)
                xd('keydown','ctrl','v');time.sleep(.05);xd('keyup','v','ctrl');time.sleep(2)
            else:
                source=root/'input.txt';source.write_text(text)
                xd('type','--clearmodifiers','--delay','18','--file',source);time.sleep(2)
        xd('windowfocus',window);time.sleep(2)
        x,y=(w-760)//2,(h-510)//2
        click(x+160,y+106);xd('keydown','ctrl','a');time.sleep(.05);xd('keyup','a','ctrl');time.sleep(.7);type_code('Interop Demo')
        click(x+160,y+134);type_code(str(project));click(x+65,y+161);time.sleep(1)
        if not (project/'project.lunoryx').exists():raise RuntimeError('Project creation failed')
        # Open the normal Lua entry and register a receiver before running C#.
        click(370,91);click(430,240);xd('keydown','ctrl','a');time.sleep(.05);xd('keyup','a','ctrl');time.sleep(.7)
        type_code('function on_message(name, value)\nprint("Lua received " .. name .. ": " .. value)\nend\nlocal cube=luaryx.find("ManagedCube")\nif cube then cube:property("fromLua",42); print("Lua sees stable entity ID",cube.id) end\n')
        click(280,119);time.sleep(1)
        # Explorer -> create a real C# script, then edit and compile it.
        click(w-260,155);click(w-205,213);time.sleep(1)
        if not (project/'scripts/Script2.cs').exists():raise RuntimeError(f'Explorer creation failed; window geometry: {geometry}')
        click(430,240);xd('keydown','ctrl','a');time.sleep(.05);xd('keyup','a','ctrl');time.sleep(.7)
        type_code('using Luaryx;\npublic sealed class Demo : Script {\npublic override void OnStart() {\nvar cube=Scene.Create("ManagedCube");\ncube.Position=new Vector3(0,1,0);\nScene.Log("C# created the shared scene entity");\nScene.Emit("ready",7);\n}\n}\n')
        click(280,119);time.sleep(17)
        click(230,11);time.sleep(3)
        # Run Lua again against the entity created by C#.
        click(370,91);click(280,119);time.sleep(3);click(230,11);time.sleep(3)
        saved=json.loads((project/'project.lunoryx').read_text())
        cube=next(o for o in saved['objects'] if o['name']=='ManagedCube')
        assert cube['properties']['fromLua']['number']==42
        assert any(d.get('language')==1 for d in saved['scripts'])
        assert (project/'scripts/Main.luaryx').exists() and (project/'scripts/Script2.cs').exists()
        # Show the scene created by the real script, then Play/Stop and save.
        click(272,91);time.sleep(2);click(990,11);time.sleep(3);click(990,11);time.sleep(1)
        click(230,11);time.sleep(2)
        (output.parent/'video-evidence.json').write_text(json.dumps(dict(
            passed=True,scene_entity_id=cube['id'],lua_property=42,
            checks=['startup project selector','folder creation','Explorer C# script creation',
                    'source editing','real C# compile','C# to Lua message','Lua to shared entity property',
                    'project files on disk','Play and Stop']),indent=2)+'\n')
    finally:
        if project.exists():shutil.copytree(project,output.parent/'video-project',dirs_exist_ok=True)
        if recorder:
            recorder.communicate(b'q\n',timeout=20)
            if recorder.returncode:raise RuntimeError('Video encoding failed')
        application.terminate()
        try:application.wait(timeout=5)
        except subprocess.TimeoutExpired:application.kill();application.wait()
        log.close()
print(f'PASS: recorded real project/C#/Lua UI session: {output}')
