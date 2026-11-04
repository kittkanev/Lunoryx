# Lunoryx Studio 1.2.1

Incremental patch on the existing editor. 1.2.0 is now a normal stable release.

- Free Fly camera: RMB look, WASD fly, Q/E vertical movement, Shift fast, Ctrl slow,
  wheel speed, Escape release. Rotation keeps the eye fixed. Orbit remains optional.
- Six signed gizmo arrows: +X/-X, +Y/-Y, +Z/-Z. Both ends support signed translation.
- Object/Edit toolbar and Tab; vertex/edge/face modes via 1/2/3; box (B), circle (C),
  Shift add, Ctrl subtract, A/Alt+A and X-ray (Alt+Z). Hidden mesh elements are
  filtered through viewport depth; selection overlays are clipped to the viewport.
- GGX metallic/roughness PBR shading, linear RGBA16F scene targets, exposure and
  ACES-style tone mapping, FXAA and directional PCF shadows. Wireframe, Solid,
  Material PBR and Rendered PBR modes. Unsupported floating targets retain LDR/FXAA.
- EN/RU documentation covers the new controls and rendering modes.

This is a realtime raster renderer with a sun and hemisphere ambient approximation;
path tracing and environment-map reflections are not implemented.

Validation includes actual mouse drags on each signed axis, production box/circle
and mesh-mode gestures, fixed-eye free-look math, normalized flight, HDR readback
above 1.0, tone mapping and framebuffer fallback, plus existing game/GLB/MCP checks.

## Game creation

- Build Game window: custom title and PNG icon, window/fullscreen startup, resolution, resizing, VSync and optional built-in HUD. Windows file pickers select icon/output; EXE resources and SDL window use the chosen icon. No Studio logo is added to game windows.
- GUI Designer: draggable labels/buttons/panels/progress, colors, layers, normalized positions, save/load, Undo/Redo and Lua updates/click callbacks.
- Building presets for killzones, checkpoints and script trigger volumes. Swept enter/stay/exit callbacks, overlap queries, safe event dispatch and debugger tracebacks.
- EN/RU Docs include an end-to-end build, GUI and trigger scripting guide.
- Gem Trail uses its own window title and fullscreen startup.
