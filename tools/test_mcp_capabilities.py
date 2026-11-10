"""Verify default denials and explicit MCP write/script capability composition."""
import json
import subprocess
import sys
from pathlib import Path

binary = str(Path(sys.argv[1]).resolve())
for flags, write, lua in [([], False, False), (['--allow-lua'], False, False),
                           (['--allow-write'], True, False),
                           (['--allow-write', '--allow-lua'], True, True)]:
    calls = [dict(jsonrpc='2.0', id=1, method='tools/list'),
             dict(jsonrpc='2.0', id=2, method='tools/call',
                  params=dict(name='run_lua', arguments=dict(code='print(123)'))),
             dict(jsonrpc='2.0', id=3, method='tools/call',
                  params=dict(name='add_object', arguments=dict(name='Capability check',type='cube'))),
             dict(jsonrpc='2.0', id=4, method='tools/call',
                  params=dict(name='audit_log', arguments={}))]
    result = subprocess.run([binary, '--mcp', *flags], input=''.join(json.dumps(c)+'\n' for c in calls),
                            text=True, capture_output=True, timeout=15, check=True)
    replies = [json.loads(line)['result'] for line in result.stdout.splitlines()]
    names = {t['name'] for t in replies[0]['tools']}
    assert ('run_lua' in names) == lua
    assert ('add_object' in names) == write
    assert bool(replies[1].get('isError', False)) != lua
    assert bool(replies[2].get('isError', False)) != write
    assert ('run_lua:allowed' if lua else 'run_lua:denied') in json.dumps(replies[3])
print('PASS: read-only MCP default, hidden-tool denials, Lua/write composition and audit log')
