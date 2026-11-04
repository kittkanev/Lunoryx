# Lunoryx 1.2.0

This update extends the existing docked Studio without replacing its interface.

- C11 keyframe sampler and Animation editor: position, shortest-path Euler rotation,
  scale and color; linear/smooth/step; scrub, preview, loop, speed and autoplay.
  Tracks round-trip through projects and single-executable game packages.
- Solid collider, killzones, checkpoint spawns and swept AABB trigger detection.
  Gem Trail includes lava, a checkpoint and a hidden fall zone. Gems use clips.
- Runtime debugger: Pause, one-frame Step, collision/trigger bounds, object state,
  render counters and retained Lua stack tracebacks. Runtime errors pause Play.
- Embedded DejaVu Sans with Cyrillic, antialiasing, DPI-aware raster density and
  saved 13–26 px text size.
- EN/RU offline handbook in Studio and styled, searchable HTML in docs/en and docs/ru.
- Full mip chains and trilinear texture filtering reduce distant shimmer.
- Preserve flat mesh normals across project/game reloads and hard primitive faces,
  fixing diagonal highlights on the sample character.
  Editor grid/axes are hidden in Play, removing coplanar floor flicker.

New Lua: animation_config, animation_key, animation_play, animation_stop,
set_trigger, set_solid, respawn. Object indices remain zero-based.

Current limitations: object animation is not skeletal skinning; GLB remains a static
export. Triggers use world AABBs. Animated transforms should be kinematic
(Dynamic off). The debugger inspects frames and errors, without line breakpoints.
Already playing audio continues during a debug pause. Spawn outside hazards.

Руководство: docs/ru/index.html. English: docs/en/index.html. Both work offline.
