# Project scripting in 1.3

Choose a folder at project startup. The scene is `project.lunoryx`, code is in `scripts/`, imported resources are copied to `assets/`, and recovery files sit beside the manifest. Save the project with Ctrl+S. Opening `project.lunoryx` reads its script files; opening a recovery snapshot restores the embedded source from that snapshot. Scripts are UTF-8 (optional BOM); CRLF is normalized to LF. A project has up to 32 script documents, each at most 65,535 bytes. Filenames must be unique, including case aliases on Windows/macOS.

In Explorer, use Scripts → Create… for a normal or module Lua/Luaryx or C# script. Names may include subfolders, which are created when saving. Lua modules return a value and are loaded with `require("subfolder.Module")`. Module lookup is limited to allocated module documents in this project; native modules and arbitrary filesystem require are unavailable. C# modules compile alongside normal C# scripts and provide helper types; their Script subclasses are not automatically started.

The editor has syntax highlighting, line numbers, clipboard operations, bounded code undo/redo and C# error markers. F5 runs the active normal Lua script in a fresh Lua state or recompiles normal C# scripts and their modules. Module documents are loaded by ordinary scripts. F6 enters/stops Play; Stop restores the editor scene. Saving never writes the disposable Play world over the project. Recovery captures that edit world together with current script source, by default every five seconds and after completed scene edits.

## Shared scene API

Luaryx is a Lua 5.4 API, not a separately invented or incompatible Lua parser. Older `add_object`, `set_position`, `get_property` and other functions remain available. Older index functions retain their original behavior; use the stable entity interface in new code.

```lua
local cube = luaryx.create("Shared", "cube")
cube:position(0, 1, 0)
cube:property("score", 42)
local saved_id = cube.id
-- Other objects can be removed without invalidating this ID.
local same = luaryx.get(saved_id)
luaryx.emit("from_lua", '{"score":42}')

function on_message(name, json_text)
    print(name, json_text)
end
```

`luaryx.get(id)` returns an entity or nil; `luaryx.find(name)` rejects ambiguous names. Entity methods: `position`, `rotation`, `scale`, `velocity`, `property` (getter without extra arguments; setter otherwise), and `destroy`. Deleted entities fail explicitly. Properties hold a number, boolean or string. The second argument to `luaryx.emit` is a JSON string; Lua's `on_message` receives a JSON string.

```csharp
using Luaryx;
using System.Text.Json.Nodes;

public sealed class Main : Script {
    private Entity cube = null!;
    public override void OnStart() {
        cube = Scene.Find("Shared") ?? Scene.Create("Shared");
        cube.SetProperty("score", 42);
        Scene.Emit("from_csharp", JsonValue.Create(42));
    }
    public override void OnUpdate(float deltaTime) {
        cube.Position = cube.Position + new Vector3(deltaTime, 0, 0);
    }
    public override void OnMessage(string name, JsonNode? value) {
        Scene.Log($"{name}: {value}");
    }
    public override void OnTrigger(string kind, ulong zone, ulong actor) { }
}
```

C# offers `Scene.Get(id)`, `Find(name)`, `Create(name, primitive)`, `All`, `Emit` and `Log`. Entity exposes `Id`, `Name`, `Position`, `Rotation`, `Scale`, `Velocity`, `GetProperty`, `SetProperty` and `Destroy`. Entity references survive scene refreshes and index changes; accessing a removed entity fails. Shared changes/messages cross the process boundary in bounded batches. A fast trigger pass-through may emit enter and exit in the same tick.

## Trust and execution limits

C# is real .NET/Roslyn code with your OS account's file, process and network access. It is **not a sandbox**. Loaded projects require an explicit trust choice for the current session. New projects created by you are trusted for that session. The worker is terminated on a compile/update deadline; these deadlines protect responsiveness, not OS confidentiality. Runtime errors pause simulation and show diagnostics.

Lua has memory/instruction limits, restricted libraries, project-only modules and confined file access. Uncooperative mesh/export operations use bounded child processes. MCP starts read-only; mutations require `--allow-write`, and Lua additionally requires `--allow-lua`.

Release packages carry a self-contained runtime. The .NET SDK is needed to build the worker, not to run the packaged editor. On Windows keep `Runtime/` beside `Lunoryx.exe`; on macOS retain the `.app` bundle intact. Standalone games package the managed runtime when C# is used. Engine distribution, including the engine portion of generated games, remains subject to GPL-3.0-only; preserve notices and provide corresponding source under the GPL.
