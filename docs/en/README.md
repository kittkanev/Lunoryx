# Lunoryx — Creator Handbook

## 01 · Start creating

Lunoryx keeps your docked Workspace, Explorer, Properties, Tools and Output. Open Window > API Docs at any time. All text, including Russian, uses the embedded DejaVu Sans font. Change Settings > Text size; no font installation is required.
### First scene
- Tools > Build adds primitives. Select in Explorer or in Scene.
- Q selects; W moves; E rotates; R scales. Drag a colored axis or plane.
- Free Fly: hold RMB + WASD, Q/E vertical movement. Disable Free Fly for orbit. Local changes the transform frame, Snap enables quantization.
- Ctrl+S saves .lunoryx. Project files contain scene metadata; GLB exports geometry and materials.
- Edit > Undo/Redo restores completed scene edits. F11 expands the code editor; F5 runs the active script.
### Play safely
Play Simulation / F6 starts the runtime and startup Lua script. Stop restores the editor scene, resources and environment. Saving/building a game is available while stopped. Loaded Lua scripts run only when explicitly started or assigned as the game startup script.

## 02 · Animation timeline

Open Window > Animation and select an object. Each object has one clip with position, rotation, scale and color keys. The sampler is written in C11 and runs in both Studio and standalone games.
### Author a clip
- Set Duration, Loop and Speed. At time 0, edit the pose in Properties and press Capture current pose.
- Move the Time slider, then Stop / restore pose to leave preview. Edit the new pose and capture it at that time. Existing timestamps are replaced.
- Choose Linear, Smooth (ease in/out) or Step for the segment after each key. Rotation follows the shortest angular path; use intermediate keys for a full revolution.
- Preview plays the selected clip; scrubbing pauses playback. Stop / restore pose restores the initial editor pose.
- Enable Autoplay in game for automatic runtime playback. Keep Dynamic off for animated props and platforms: animation writes absolute transforms.
### Lua example
```lua
local box = add_object('Animated box', 'cube')
animation_config(box, 2, true, 1, true) -- seconds, loop, speed, autoplay
set_position(box, 0, 1, 0)
animation_key(box, 0, 1) -- time, easing: 0 linear / 1 smooth / 2 step
set_position(box, 4, 1, 0)
animation_key(box, 2, 1)
animation_play(box)
-- animation_stop(box) freezes the current pose
```
Limits: 256 keys per object; duration 0.01..3600 seconds; speed 0.01..100. Clips are stored in .lunoryx and packed Game.exe. This release animates objects, not skeletal skinning; GLB export remains a static scene.

## 03 · Killzones and checkpoints

Properties > Physics / Character now includes Solid collider and Trigger. A trigger uses the object's world AABB, including its transform and mesh bounds. It never pushes an actor, even when Solid is checked. Visibility does not disable a trigger.
- None: ordinary object, collision follows Solid.
- Killzone: respawns dynamic actors at their initial spawn or latest checkpoint; clears velocity.
- Checkpoint: updates the character's spawn to above the checkpoint.
- Falling below Y=-50 is a fallback death condition. Disable Ground at Y=0 for bottomless levels.
### Lua
```lua
local lava = add_object('Lava', 'cube')
set_position(lava, 5, 0.1, 0)
set_scale(lava, 2, 0.2, 2)
set_trigger(lava, 1) -- 0 none / 1 killzone / 2 checkpoint / 3 script
set_solid(lava, false)
-- respawn(player) returns true during Play for a known dynamic actor
```
Place spawn points clear of killzones. Killzones win over overlapping checkpoints. Swept trigger checks catch fast actors crossing a thin volume. Checkpoints do not reset score; Stop/Restart resets the entire game. Gem Trail includes a lava tile, a green checkpoint and a hidden fall zone.

## 04 · Debugging

Open Window > Debugger. Start Play, then Pause to freeze Lua, animation and physics. Step 1/60 s advances those systems by one fixed frame while remaining paused. Audio already playing continues independently. This is a runtime inspector, not a line-breakpoint debugger.
- Collision / trigger bounds shows collider AABBs: blue ordinary, red killzones, green checkpoints. It is an editor overlay, excluded from standalone rendering.
- Inspect selected object ID, position, velocity and grounded state.
- Frame time, draw calls, triangles, culling and geometry cache counters help isolate slow scenes.
- Last Lua traceback keeps the latest script error with stack frames. Output retains earlier messages. Clear errors clears the inspector entry.
- Runtime callback errors pause Lua and Play; Stop or fix the script and Resume.
Lua is trusted code with standard file/process libraries. Each call retains its instruction budget. Infinite loops stop with an error instead of hanging Studio.

## 05 · Rendering and assets

Flat shading is preserved when reloading projects and building games, preventing unintended diagonal highlights on block characters. Textures now include a complete mip chain and trilinear minification to reduce distant checkerboard shimmer. Editor grid and world axes are hidden during Play to prevent coplanar flicker over level floors. Rendering quality presets and FXAA remain available in Settings; F8 cycles presets in the player.
- Import static GLB through File or drag/drop. Retain textures with the project or embed them.
- Audio supports WAV/WAVE/W64/RF64, MP3, FLAC and OGG/OGA Vorbis; Audio manages preview, loop, volume, pitch and 3D position.
- GLB export includes geometry, UVs, normals, colors and materials. Runtime logic, triggers and animation clips remain in .lunoryx / Game.exe.
- Join combines geometry; boolean operations use the existing modeling tools. Sculpt, UV and geometry nodes keep their existing workflow.
If an artifact persists, compare Office/Balanced/Quality, note the GPU/driver, and capture the same camera view. The renderer targets OpenGL 2.1 compatibility and is not a full modern PBR pipeline.

## 06 · Lua reference and shipping

Object indices are strictly zero-based. Deleting an object shifts later indices; use find_object(name) after changing membership. MCP uses stable IDs instead.
### Core API
```lua
local i = add_object('Box', 'cube') -- cube / pyramid / sphere / cylinder / plane
set_position(i, 0, 1, 0); set_rotation(i, 0, 45, 0) -- degrees
set_scale(i, 1, 2, 1); set_color(i, 0.2, 0.7, 1)
set_property(i, 'Health', 100)
set_physics(i, true, 3, 0) -- dynamic, mass, bounce
set_velocity(i, 0, 5, 0)
local player = add_character('Player', 2, 0.65, 0.45)
follow_camera(player, 7, 24)
function on_update(dt) -- seconds
    hud_text('Running'); game_score(0)
end
```
Getters: get_position, get_rotation, get_scale, get_color, get_material, get_velocity, get_property. Object tools: duplicate_object, remove_object, find_object, get_object_count, set_visible, set_wireframe, set_name, clear_scene. Mesh tools: make_editable, sculpt, merge_objects, mesh_operation. Audio: add_sound, play_sound, stop_sound.
### Standalone
Tools > Game / Build assigns the startup script and builds Game.exe with scene/resources. Test the relocated EXE with the source files unavailable. The runtime contains the same animation and trigger systems.
MCP: Lunoryx.exe --mcp, stdio, separate headless scene. Save a project from MCP and open it in Studio; it does not attach to an already running editor. Detailed legacy API topics remain available under API reference in this window.

## 07 · Fly, select and render

### Six directions
The transform gizmo has positive and negative handles for X, Y and Z. Drag either end in either direction; the center supports plane/free translation.
### Free camera
Free Fly is enabled in the Scene toolbar. Hold RMB to look and fly: WASD moves relative to view, Q/E down/up, Shift accelerates, Ctrl slows down. The wheel while holding RMB changes speed; Escape releases the mouse. Camera rotation keeps the camera position fixed. Disable Free Fly for the orbit controls.
### Selection
- Tab switches Object / Edit for a selected editable object.
- 1 / 2 / 3 choose vertices / edges / faces in Edit mode.
- B selects with a rectangle; C paints with a circle. Wheel changes the circle radius. Escape returns to click selection.
- Shift adds; Ctrl subtracts for region selection. A selects all, Alt+A deselects.
- X-ray / Alt+Z enables selection through geometry in Edit mode. With X-ray off, the viewport depth buffer filters hidden elements.
### Rendering
Wireframe, Solid, Material PBR and Rendered PBR are available in the Scene toolbar. Material and Rendered use GGX metallic/roughness lighting. Balanced/Quality render to a linear RGBA16F target where supported, then apply exposure, tone mapping and FXAA. Quality adds directional PCF shadows. Office uses the direct compatible path.
Lighting is realtime rasterization with a directional sun and an approximate hemisphere ambient term. It does not include path tracing or environment-map reflections. The display texture and UI stay at the requested output size.

## 08 · Build your game EXE

### Your own game identity
Open Build Game in the top menu (also Tools > Character > Game / Build settings). Stop simulation before building. Give the game its own title; this becomes its window title. Studio branding is not added to the game window.
Choose a square PNG icon, 16–256 px (256×256 recommended). On Windows, Choose icon opens a file picker; Clear icon uses the system default, not the Lunoryx logo. The PNG is embedded in the package and Windows EXE resources, so both Explorer and the running game use it. Windows may cache an old Explorer icon: try a new output filename.
### Display and startup
- Startup display: Windowed or Fullscreen (desktop). Fullscreen uses the current desktop resolution without changing the monitor mode.
- Window width/height apply to windowed mode: 320–7680 by 200–4320. Resizable window and Game VSync are independent settings.
- New projects and Gem Trail start fullscreen; old projects keep their saved/legacy windowed behavior until you choose Fullscreen.
- Built-in score / HUD can be disabled when you make your own GUI.
- Startup script: choose the Lua tab to execute at launch. Follow selected character and distance/pitch/yaw configure the game camera.
### Build and test
Choose output EXE (Windows file picker) or type its path, then Build standalone Game.exe. The builder packages the scene, GUI, scripts, PNG icon, textures and sounds into one executable without a separate compiler. Build errors appear in Output. Save the .lunoryx to keep the settings for the next build.
Move the game to an empty folder to test it independently. Esc exits; R restarts. Build on Windows to customize Windows EXE resources. Linux builds produce a native Linux game, not a Windows cross-compile.


## 09 · GUI Designer and trigger scripting

### Build a game interface
Window > GUI Designer opens a canvas. Add Label, Button, Panel or Progress. Select a widget in the list or canvas and drag it into place. Edit its unique ID, text, visibility, background/text/fill colors, X/Y, width/height and progress value. Move layer up places it above the next widget. Hidden widgets remain editable in the designer. Delete widget removes the selected one; Edit > Undo/Redo restores GUI edits while stopped.
Coordinates and sizes are fractions of the viewport (0–1). The GUI scales with the game window; content outside the viewport is clipped. A maximum of 128 widgets is supported, IDs are unique (1–63 bytes), text up to 1024 UTF-8 bytes. Labels/buttons support multiline text; progress values are 0–1. This is a flat 2D overlay, without HTML, nesting or text-input widgets.
### Lua GUI API
- gui_text(id, text): change a label/button's text.
- gui_visible(id, boolean): show or hide a widget.
- gui_value(id, number): set progress (clamped 0–1).
Each returns true if the ID exists, otherwise false. Define on_gui_click(id) for button clicks. Create the widgets first in GUI Designer. Clicks are delivered on the next simulation tick; callbacks stop while the debugger is paused. Stop restores the original GUI; restart resets it.
### Trigger building and events
Tools > Build has Killzone, Checkpoint and Script trigger presets. Move/scale them like other objects. Properties > Trigger can change any object into a zone; Debugger > Collision / trigger bounds displays their volumes. Triggers are world-axis-aligned, non-solid boxes even if Solid is checked.
set_trigger(objectIndex, kind): 0 none, 1 killzone, 2 checkpoint, 3 script trigger. Returns false for an invalid index/kind. A trigger is made static. Killzones respawn dynamic actors; checkpoints update character spawns; script triggers only emit events.
Define on_trigger(event, zoneIndex, actorIndex). Events are enter, stay (once per runtime tick, not per physics substep), exit. Sensors track dynamic non-trigger actors, including characters; trigger_contains(zoneIndex, actorIndex) directly tests current overlap for any object. A thin zone crossed in one frame emits enter then exit. Kill/respawn is applied before callbacks. Overlap limits: 8192 active pairs, 4096 queued events per tick. Keep scenes within these limits.
Lua indices are zero-based and shift after deletion; names need not be unique. Call find_object after scene membership changes. Events use stable IDs internally and resolve indices before each callback; events involving deleted objects are discarded. Callback errors include a traceback and pause the runtime in Debugger/Output.
```lua
-- Tools > Build > Script trigger creates a non-solid zone.
-- Assign this script in Build Game > Startup script; press Play.
function on_trigger(event, zone, actor)
    if event == 'enter' and zone == find_object('Script trigger') then
        gui_text('message', 'Welcome!')
        gui_visible('message', true)
    elseif event == 'exit' then
        gui_visible('message', false)
    end
end
function on_gui_click(id)
    if id == 'restart' then restart_game() end
end
```
