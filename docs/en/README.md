# Lunoryx — Creator Handbook

## 01 · Start creating

Lunoryx keeps your docked Workspace, Explorer, Properties, Tools and Output. Open Window > API Docs at any time. All text, including Russian, uses the embedded DejaVu Sans font. Change Settings > Text size; no font installation is required.
### First scene
- Tools > Build adds primitives. Select in Explorer or in Scene.
- Q selects; W moves; E rotates; R scales. Drag a colored axis or plane.
- Right mouse orbits; middle mouse pans; wheel zooms. Local changes the transform frame, Snap enables quantization.
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
set_trigger(lava, 1) -- 0 none / 1 killzone / 2 checkpoint
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
