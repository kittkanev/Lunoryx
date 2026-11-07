"""Exercise hostile inputs and ordinary controls through the MCP boundary."""
import json, os, subprocess, sys, tempfile
from pathlib import Path
binary=str(Path(sys.argv[1] if len(sys.argv)>1 else './engine').resolve())
with tempfile.TemporaryDirectory(dir=Path.cwd()) as folder:
    root=Path(folder)
    def session(commands):
        requests=[dict(jsonrpc='2.0',id=i,method='tools/call',params=dict(name=name,arguments=args)) for i,(name,args) in enumerate(commands)]
        result=subprocess.run([binary,'--mcp'],cwd=root,input=''.join(json.dumps(x)+'\n' for x in requests),text=True,encoding="utf-8",capture_output=True,timeout=15)
        assert result.returncode==0,result.stderr
        responses=[json.loads(line)['result'] for line in result.stdout.splitlines()]
        assert len(responses)==len(commands),(result.stdout,result.stderr)
        return responses
    def run(code): return session([('run_lua',{'code':code})])[0]
    def text(response): return response['content'][0]['text']
    for code in [
        'while true do pcall(function() while true do end end) end',
        'while true do xpcall(function() while true do end end,function() return "caught" end) end',
        'while true do coroutine.resume(coroutine.create(function() while true do end end)) end',
        'local f=coroutine.wrap(function() while true do end end); while true do pcall(f) end',
    ]:
        reply=run(code);assert reply['isError'] and 'budget' in text(reply),reply
    result=session([('run_lua',{'code':'co=coroutine.create(function() coroutine.yield(); while true do end end); coroutine.resume(co)'}),('run_lua',{'code':'while true do coroutine.resume(co) end'})])
    assert not result[0]['isError'] and result[1]['isError']
    assert run('setmetatable({}, {__gc=function() while true do end end})')['isError'] # close must terminate
    reply=run("assert(io==nil and package==nil and debug==nil and os.execute==nil and load==nil and loadfile==nil and dofile==nil); assert(string.rep('ab',3)=='ababab'); assert(string.match('item42','%d+')=='42'); local c=coroutine.wrap(function() coroutine.yield(7); return 9 end); assert(c()==7 and c()==9); print('control')")
    assert not reply['isError'] and text(reply)=='control\n',reply
    for code in ["string.rep('x',1073741824)","string.find(string.rep('a',10000),'a*a*a*a*b')", "string.find(string.rep('(',60000),'%b()')"]:
        assert run(code)['isError']
    # Lua allocations stay bounded; recover and execute another ordinary command.
    replies=session([('run_lua',{'code':'local t={} for i=1,400 do t[i]=string.rep("x",1000000)..tostring(i) end; error("done")'}),('run_lua',{'code':'collectgarbage(); print("alive")'})])
    assert replies[0]['isError'] and not replies[1]['isError']
    assert '\ufffd' in text(run("print(string.char(255))"))
    replies=session([('run_lua',{'code':'print("first")'}),('run_lua',{'code':'print("second")'})]);assert text(replies[1])=='second\n'
    for path in ['../escape.glb','//attacker/share/x.glb','\\\\attacker\\x.glb']:
        assert session([('export_glb',{'path':path})])[0]['isError']
    if os.name!='nt':
        (root/'outside').symlink_to(root.parent,target_is_directory=True)
        assert session([('export_glb',{'path':'outside/escape.glb'})])[0]['isError']
    assert session([('run_lua',{'code':"add_sound('../outside.wav')"})])[0]['isError']
    # Own project -> save -> load, including negative (mirrored) scale.
    replies=session([('add_object',{'name':'control','type':'cube'}),('run_lua',{'code':"set_scale(0,-1,1,1); print('ok')"}),('project_save',{'path':'control.lunoryx'}),('project_load',{'path':'control.lunoryx'})]);assert all(not x['isError'] for x in replies),replies
    for field,value in [('texture','../secret.png'),('texture','//attacker/share/a.png')]:
        data=json.loads((root/'control.lunoryx').read_text());data['objects'][0]['materials']=[dict(name='x',image=[],texture=value,color=[1,1,1],metallic=0,roughness=1)]
        (root/'bad.lunoryx').write_text(json.dumps(data));assert session([('project_load',{'path':'bad.lunoryx'})])[0]['isError']
    assert session([('add_object',{'name':'typo','type':'spher'})])[0]['isError']
print('PASS: Lua sandbox, catch/coroutine/finalizer budgets, bounded strings, UTF-8, isolated output, confined files and project controls')
