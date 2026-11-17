# Lunoryx — Этап 0: реестр крупного офлайн-обновления

Исходная версия: **1.3.1**, коммит `2212256ec5537f1fb850a1bb8298c8d323f0820b`. Реестр различает реализованные части разработки и запланированные функции; готовность и доказательства указаны для каждого пункта.

Первичные данные: [машиночитаемый реестр](../validation/major-update-register.json), [исходный аудит](../audit-v1.3.0.json), [исправления 1.3.1](../validation/v1.3.1-fixes.md), [локальные проверки Этапа 0](../validation/major-update-baseline.md).

## Порядок и границы

0. Реестр, сверка кода, базовые проверки и актуальная EN/RU справка — эта часть.
1. F0: безопасность/краши/ресурсы → корректность: доверие паков, budget stress, подготовка signing CI, сохранение всех текущих границ.
2. P1: сначала scene/format/API contracts и миграции; затем skeleton/physics, material/render/modeling, game saves/input. Каждая вертикальная часть: runtime + editor + persistence + Lua/C# + тесты; изолированная математическая функция не считается готовой фичей.
3. P2: audio/GUI/script/asset/UX и стабилизация экспериментального поверх работающего P1.
4. P3 не начинать до завершения согласованных P1/P2. Major-релиз только после согласованного объёма и полного gate.

`tools/check_major_release.py` отклоняет публикацию 1.4.0, пока любой пункт P1/P2 не завершён или не содержит ссылок на тесты и валидацию. Это дополнение к нативному CI, graphics, санитайзерам и видео точного коммита. Разработка пока не завершена.

## Решения владельца — подтверждены

- Целевая версия: **1.4.0**. Готовность версии определяется реализацией и проверками, не изменением строки версии.
- Новый формат проекта **разрешён**: обязательны миграция и отдельный backup до первой записи; старые проекты и .pre-1.3.bak сохраняются.
- HMAC — симметричный MAC, не публичная цифровая подпись. Секрет внутри распространяемого EXE не защищает от подделки. Внешний локальный ключ даёт проверку владельцам ключа; проверка всеми игроками без секрета требует другой модели подписи. Владелец выбрал **внешний локальный HMAC-ключ**; новые паки должны проходить аутентификацию, legacy не принимается молча.
- Приложение/игра: без аккаунтов, серверов, телеметрии, remote asset fetch и новых сетевых зависимостей. MCP — локальный stdio. CI на GitHub разрешён; signing/notarization подготовить только в CI. Новые вендорные зависимости требуют отдельного одобрения и совместимой лицензии.

Ожидает решения: [предложение Jolt Physics / ozz-animation](PHYSICS-ANIMATION-PROPOSAL.md), локальные MIT-библиотеки с закреплёнными исходниками для физики/анимации. Новые библиотеки не скачаны и не добавлены; требуется отдельное явное одобрение зависимостей.

## F0 — Действующие ограничения и обязательные защиты

Тесты ниже — критерии приёмки, если в журнале проверок не указан фактический запуск.

### LIMIT-01 — Аутентификация игровых паков

Статус: Частично: реализовано и проверено локально; нативный CI ожидается.

Код: [game_package.h](../../src/io/game_package.h), [sha256.h](../../src/io/sha256.h).

План: Согласовать доверие и хранение ключа; затем HMAC-SHA-256, аутентифицированный заголовок/манифест/данные, constant-time сравнение и явная политика старых неподписанных паков. Ключ не в EXE/паке/проекте.

Тест: RFC 4231; побитовые изменения заголовка/данных/тега; неверный/отсутствующий ключ; запрет downgrade; корректная игра после перемещения; ключ не попадает в архив/логи.

Реализация и выполненные проверки: [проверка аутентификации паков](../validation/v1.4.0-package-auth.md), [использование локального ключа](PACKAGE-AUTH.md).

### LIMIT-02 — Бюджеты пака 1 GiB / runtime 128 MiB / 4096 записей

Статус: Частично.

Код: [game_package.h](../../src/io/game_package.h), [v131_regressions.h](../../tests/v131_regressions.h).

План: Сохранить бюджеты, вынести общие проверяемые расчёты, добавить быстрые границы и отдельный потоковый стресс. Не увеличивать лимиты молча.

Тест: N−1/N/N+1, UINT64_MAX, сумма размеров, большой манифест, пустые/дублирующиеся/перекрывающиеся ranges; отсутствие частичного результата; sparse >3 GiB остаётся.

### LIMIT-03 — Windows без подписи; macOS ad-hoc без notarization

Статус: Внешнее условие.

Код: [tests.yml](../../.github/workflows/tests.yml), [release.yml](../../.github/workflows/release.yml), [package_macos.py](../../tools/package_macos.py), [merge_macos.py](../../tools/merge_macos.py).

План: Подготовить opt-in CI signing после нативных тестов, до финальных хешей/аттестации. Секреты только GitHub Secrets; без сертификатов честный unsigned/ad-hoc статус. Apple notarization только в разрешённом CI, не в приложении.

Тест: Ветки CI без/с неполным набором secrets; Get-AuthenticodeSignature/signtool verify и codesign/spctl/stapler при настоящих сертификатах; подпись не ломает footer.

Подготовка: [опциональная подпись в CI](RELEASE-SIGNING.md); policy-тесты прошли локально. Настоящая подпись и нотарификация с сертификатами не проверены.

### LIMIT-04 — C# с правами пользователя

Статус: Сохранить явный контракт.

Код: [csharp_bridge.h](../../src/scripting/csharp_bridge.h), [project_workspace.h](../../src/editor/project_workspace.h), [Program.cs](../../extensions/csharp/Program.cs).

План: Сохранить trust на сеанс и deadlines. Не называть worker песочницей. Новые API движка офлайн; произвольный доверенный C# сохраняет OS-доступ, включая потенциальную сеть.

Тест: Нет запуска до trust; новый проект/загрузка сбрасывает доверие по контракту; timeout, crash, kill/reap, повторный запуск; секреты пака не передавать скриптам.

### LIMIT-05 — Нет полного аудита сторонних зависимостей

Статус: Частично.

Код: [dependency-runtime.json](../../docs/validation/dependency-runtime.json), [NOTICE](../../NOTICE), [packages.lock.json](../../extensions/csharp/packages.lock.json), [generate_sbom.py](../../tools/generate_sbom.py).

План: Сохранить pinned/locked зависимости и GPL/NOTICE. Снимок .NET от 2026-11-10 не считать свежим аудитом. Любую новую вендорную зависимость согласовать; не скачивать новые пакеты в этой части работы.

Тест: SBOM/хеши/лицензии/lockfile; повторяемая сборка из имеющегося кэша; обновление по конкретному подтверждённому advisory отдельно.

### LIMIT-06 — SAX-бюджеты — оценка, не жёсткий allocator

Статус: Частично.

Код: [json_budget.h](../../src/io/json_budget.h), [scene_assets.h](../../src/io/scene_assets.h).

План: Сохранить preflight до DOM; проверить memory amplification и общие бюджеты новых skins/каналов/карт до выделений.

Тест: Глубина 95/96/97, число узлов, короткие поля, декодированные изображения 128 MiB ±1, ASan/UBSan и ограничение RSS в стрессе.

### LIMIT-07 — macOS memory limit воркеров best-effort

Статус: Частично.

Код: [worker_process.h](../../src/scripting/worker_process.h), [model_worker.h](../../src/scripting/model_worker.h), [export_worker.h](../../src/scripting/export_worker.h).

План: Сохранить CPU/deadline и лимиты обмена; новые тяжёлые операции проводить через этот boundary. Не обещать жёсткий RLIMIT_DATA на macOS.

Тест: Незавершающийся worker, 32 MiB обмен, слишком длинный ответ, pipe close, crash; Windows Job/Linux limits и нативный Mac evidence.

### LIMIT-08 — Fuzz/soak не покрывает все форматы и состояния

Статус: Частично.

Код: [soak_project_parser.py](../../tools/soak_project_parser.py), [endurance.yml](../../.github/workflows/endurance.yml).

План: Сохранить 1000-cycle gate; добавить кампании для паков/GLB/скриптов и новых схем. Часовой parser CI 43703840148 успешен, но это не coverage-guided fuzzing и не GPU-тест.

Тест: Фиксированные seed/corpus, минимизированные regressions, RSS/FD, timeout, transactional load; публиковать длительность и фактическое покрытие.

### LIMIT-09 — Ресурсы GPU/аудио и драйверы

Статус: Ограниченное evidence.

Код: [viewport_postprocess_tests.cpp](../../tests/viewport_postprocess_tests.cpp), [render_parity_tests.cpp](../../tests/render_parity_tests.cpp), [v131_regressions.h](../../tests/v131_regressions.h), [PERFORMANCE.md](../../docs/PERFORMANCE.md).

План: Не терять lifecycle 1.3.1; расширить длительную смену проектов/ассетов, счётчики живых ресурсов и аппаратную матрицу. Старые isolated benchmarks не считать FPS всей сцены.

Тест: 100 resize, 50 HDR/post switches, failed upload cleanup, 10 WAV + orphan; длительный цикл на конкретных GPU/драйверах с замерами; не обещать zero leaks.

### LIMIT-10 — Schema 4 с миграцией schemas 1–3 и резервными копиями

Статус: Частично; миграция schema 4 реализована, нативный CI ожидается.

Код: [editor_project.h](../../src/editor/editor_project.h), [project_workspace.h](../../src/editor/project_workspace.h).

План: Согласованная основа schema 4; до первой записи манифеста/скрипта сохранять исходный манифест и восстанавливаемый текст внешних скриптов. Сохранять прежние копии, отклонять конфликты, открывать recovery для чтения и добавлять новые поля с валидацией.

Тест: Golden проекты 1/2/3, идемпотентная миграция, byte-identical backup, отказ без изменения исходника, обратное открытие по явно объявленному контракту.

Реализация: [проверка миграции проекта](../validation/v1.4.0-project-migration.md).

### LIMIT-11 — Ограничения документа, скриптов, undo и ресурсов

Статус: Сохранить явный контракт.

Код: [script_document.h](../../src/editor/script_document.h), [editor_history.h](../../src/editor/editor_history.h), [editor_project.h](../../src/editor/editor_project.h), [scene_assets.h](../../src/io/scene_assets.h), [material_render.h](../../src/render/material_render.h), [audio_backend.c](../../extensions/audio/audio_backend.c).

План: Учесть 32 MiB проект, 32 слота × 65535 байт кода, 16 MiB encoded asset, 128 MiB/64 scene history, 128 MiB/256 textures, 64 голоса/64 MiB encoded audio; новые данные считать в общих бюджетах.

Тест: Границы UTF-8/CRLF, все слоты, превышение агрегата до копирования, warning undo, успешный повтор после отказа; отсутствие silent truncation.

### LIMIT-12 — Триггеры и время симуляции имеют явные границы

Статус: Сохранить явный контракт.

Код: [gameplay_runtime.h](../../src/runtime/gameplay_runtime.h), [scene_tools.h](../../src/core/scene_tools.h).

План: Сохранить 32768 actor×zone pairs и pause/reject без потери событий; swept enter+exit в одном tick; backlog до 2 секунд с видимым DroppedTime.

Тест: 10000 overlaps, граница 32768, превышение во время Play, enter/stay/exit и respawn order; длительный кадр/детерминированный replay.

### LIMIT-15 — Нативные платформы, стандарты и MSVC

Статус: Частично.

Код: [Makefile](../../Makefile), [Makefile.win](../../Makefile.win), [Makefile.mac](../../Makefile.mac), [tests.yml](../../.github/workflows/tests.yml).

План: Правила production C++17 и C17 настроены; сохранить ABI и compatibility fixtures. Проверить нативно Linux GCC, Windows MinGW и Apple Clang/macOS 15+; MSVC не заявлять без поддерживаемой сборки.

Тест: Нативный CI каждой платформы, C/C++ header ABI и строгие warnings; compile на C17; обновление матрицы только с реальным evidence.

### LIMIT-16 — Устаревшие справка и исторические отчёты

Статус: Документация исправлена в Этапе 0.

Код: [topics.json](../../docs/manual/topics.json), [AUDIT-REMEDIATION.md](../../docs/AUDIT-REMEDIATION.md), [1.3.0-plan.md](../../docs/work/1.3.0-plan.md).

План: Исправить утверждения о Lua file/process libraries и старых 8192/4096 trigger limits; пометить исторический отчёт как superseded. EN/RU и embedded help генерировать синхронно.

Тест: test_release_safety.py, build_manual.py, повторная генерация без diff; сверка утверждений с lua_sandbox/gameplay_runtime.

### GUARD-01 — Lua/MCP/anchored I/O не ослаблять

Статус: Обязательная защита.

Код: [lua_sandbox.h](../../src/scripting/lua_sandbox.h), [studio_mcp.h](../../src/scripting/studio_mcp.h), [secure_asset_io.h](../../src/io/secure_asset_io.h).

План: Lua 256 MiB/instructions и project-only require; локальный stdio MCP read-only + раздельные write/lua флаги; no-follow/handle-relative/atomic rename. Новые сервисы/аккаунты/телеметрия запрещены.

Тест: test_audit_security.py, test_mcp_capabilities.py, v13 reparse/symlink tests и новые API через тот же boundary.

### GUARD-02 — Регрессии 1.3.1 и release gate

Статус: Обязательная защита.

Код: [v131_regressions.h](../../tests/v131_regressions.h), [core_tests.cpp](../../tests/core_tests.cpp), [viewport_postprocess_tests.cpp](../../tests/viewport_postprocess_tests.cpp), [render_parity_tests.cpp](../../tests/render_parity_tests.cpp), [test_release_safety.py](../../tools/test_release_safety.py), [release.yml](../../.github/workflows/release.yml).

План: Девять групп 1.3.1 сохранять обязательно. Публикация только exact-SHA native CI + sanitizers + graphics + packaged C# game + свежее UI-видео; четыре assets, SBOM/checksums в ZIP, теги immutable.

Тест: Полный gate; несовпадающий SHA/неуспешный job/старое видео/лишний asset отклоняются; no-credential signing не выдаётся за verified.


## P1 — Ядро обновления и связанные ограничения

Тесты ниже — критерии приёмки, если в журнале проверок не указан фактический запуск.

### LIMIT-13 — Legacy индексы, одинаковые имена и Join

Статус: Сохранить явный контракт.

Код: [scene_tools.h](../../src/core/scene_tools.h), [luaryx_runtime.h](../../src/scripting/luaryx_runtime.h), [Scene.cs](../../extensions/csharp/Scene.cs).

План: Новые API только stable IDs; legacy find_object сохраняет первый результат, новые find отклоняют неоднозначность. Join пока отвергает данные, которые не умеет сохранять.

Тест: Удаление/перестановка объектов, одинаковые имена, round-trip ID; Join не теряет материалы/анимацию/свойства при отказе.

### P1-ANIM-01 — Скелет и скиннинг

Статус: Не реализовано.

Код: [scene_core.h](../../src/core/scene_core.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [material_render.h](../../src/render/material_render.h).

План: Добавить rig/joints/inverse-bind/weights и skinning с бюджетами; CPU reference перед GPU path.

Тест: Bind pose, 1–4 веса, индексы/NaN, transformed parents, CPU/GPU parity, save/undo/game.

### P1-ANIM-02 — IK

Статус: Не реализовано.

Код: [scene_animation.h](../../src/core/scene_animation.h).

План: Детерминированный bounded двухкостный IK, затем цепочки с лимитами суставов.

Тест: Достижимая/недостижимая цель, zero-length, joint limits, no NaN, лимит итераций.

### P1-ANIM-03 — Блендинг и state machine

Статус: Не реализовано.

Код: [scene_animation.h](../../src/core/scene_animation.h).

План: Клипы, параметры, переходы и blend masks; фиксировать семантику перехода/прерывания.

Тест: Переходы по границам, blend endpoints, quaternion shortest-path, цикл, deterministic replay.

### P1-ANIM-04 — Кривые easing

Статус: Частично.

Код: [animation_core.h](../../src/core/animation_core.h), [animation_tests.c](../../tests/animation_tests.c).

План: Десять ограниченных C17-кривых реализованы в таймлайне/Lua/schema 4/Undo/отдельной игре; ID 0–2 сохранены. Завершить нативный CI и санитайзеры.

Тест: Точки 0/1, монотонность выбранных кривых, NaN, zero span, legacy golden samples.

[Выполненные проверки easing](../validation/v1.4.0-easing.md).

### P1-ANIM-05 — Shape keys

Статус: Не реализовано.

Код: [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [scene_animation.h](../../src/core/scene_animation.h).

План: Ограниченные mesh deltas/веса, нормали и комбинация со skinning.

Тест: Топология/размеры, смешивание весов, negative/excessive values, round-trip и GL parity.

### P1-ANIM-06 — События анимации и root motion

Статус: Не реализовано.

Код: [scene_animation.h](../../src/core/scene_animation.h), [game_runtime.h](../../src/runtime/game_runtime.h).

План: События на пересечениях timeline, loop/wrap и root delta через character/physics.

Тест: Exactly-once events, большой dt, reverse/loop, Stop/restart, столкновение root motion.

### P1-PHYS-01 — Динамические rigid bodies и импульсы

Статус: Частично.

Код: [scene_tools.h](../../src/core/scene_tools.h), [scene_core.h](../../src/core/scene_core.h).

План: Есть Dynamic/Mass/Velocity и AABB response; добавить rotation/inertia/angular velocity/contact impulses, сохраняя legacy поведение по явно выбранному backend.

Тест: Conservation fixtures, stacking/resting/friction, fixed-step replay, старые 3840 steps/2000 bounds.

### P1-PHYS-02 — Констрейнты

Статус: Не реализовано.

Код: [scene_tools.h](../../src/core/scene_tools.h).

План: Distance/hinge constraints с stable IDs, итерационным бюджетом и корректным удалением body.

Тест: Drift, limits, inactive/deleted body, closed loops, serialization/undo.

### P1-PHYS-03 — Sphere/capsule/convex коллайдеры

Статус: Не реализовано.

Код: [scene_tools.h](../../src/core/scene_tools.h), [spatial_index.h](../../src/core/spatial_index.h).

План: Отдельная validated collider shape и narrow phase; AABB остаётся broad-phase bound.

Тест: Analytic contacts, rotated/degenerate convex, negative scales, swept/overlap consistency.

### P1-PHYS-04 — Raycast/overlap API, слои и маски

Статус: Частично.

Код: [game_bindings.h](../../src/scripting/game_bindings.h), [gameplay_runtime.h](../../src/runtime/gameplay_runtime.h).

План: Есть trigger_contains; добавить общие query результаты со stable IDs, normals/distance и filtering в Lua/C#.

Тест: Closest/all hits, inside/zero ray, нормализация, mask symmetry, deleted entities, API parity.

### P1-PHYS-05 — Character controller: склоны и ступени

Статус: Частично.

Код: [scene_tools.h](../../src/core/scene_tools.h), [gameplay_runtime.h](../../src/runtime/gameplay_runtime.h).

План: Есть WASD/jump/grounded AABB; добавить capsule sweeps, slope limit, step-up/down и контактные нормали.

Тест: Склон выше/ниже limit, step ±epsilon, потолок, moving platform, root motion, tunnelling.

### P1-PHYS-06 — BVH broad phase

Статус: Частично.

Код: [spatial_index.h](../../src/core/spatial_index.h).

План: Сейчас spatial grid с large-object fallback; добавить измеряемый BVH/refit, grid оставить reference/fallback.

Тест: Query differential против brute force, dense/large worlds, refit/removal, deterministic perf/memory.

### P1-RENDER-01 — PBR texture maps

Статус: Частично.

Код: [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [scene_shadows.h](../../src/render/scene_shadows.h), [material_render.h](../../src/render/material_render.h), [scene_assets.h](../../src/io/scene_assets.h).

План: Есть base texture и scalar metallic/roughness; добавить normal/roughness/metallic/AO/emissive, tangents и общие import/cache budgets.

Тест: Каналы glTF, sRGB/linear, missing/invalid maps, tangent orientation, GL fallback, round-trip/undo.

### P1-RENDER-02 — Точечный/прожекторный свет с тенями

Статус: Частично.

Код: [scene_shadows.h](../../src/render/scene_shadows.h).

План: Есть sun PCF shadows; добавить bounded point/spot lights, light culling и shadow atlas/cubemap с capability fallback.

Тест: Occlusion, spot cone, shadow bias, light limit, unsupported GL capabilities и lifecycle cleanup.

### P1-RENDER-03 — Прозрачность с сортировкой

Статус: Не реализовано.

Код: [material_render.h](../../src/render/material_render.h), [render_geometry.h](../../src/render/render_geometry.h).

План: Разделить opaque/masked/blended queues, depth-write и stable back-to-front sort.

Тест: Overlapping alpha meshes, equal depth, camera rotation, cached/legacy image parity.

### P1-RENDER-04 — Скайбокс и туман

Статус: Частично.

Код: [render_showcase.h](../../src/render/render_showcase.h), [editor_ui.h](../../src/editor/editor_ui.h).

План: Есть градиентные sky presets; добавить локальные cubemap assets и fog с linear HDR pipeline.

Тест: Cube faces/orientation, missing resources, fog distances, HDR/Office fallback и package relocation.

### P1-RENDER-05 — Инстансинг/батчинг

Статус: Частично.

Код: [render_geometry.h](../../src/render/render_geometry.h), [material_render.h](../../src/render/material_render.h).

План: Есть VBO/material batches; добавить instance groups и extension-gated instancing, сохранив OpenGL 2.1 fallback.

Тест: Image parity, changing transforms/materials, capability on/off, draw calls и cache budget under churn.

### P1-RENDER-06 — LOD

Статус: Не реализовано.

Код: [render_geometry.h](../../src/render/render_geometry.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h).

План: Явные mesh LOD levels, distance/error policy и hysteresis; generation только локально.

Тест: Threshold ±epsilon, no flicker, bounds/culling, invalid levels, memory and standalone parity.

### P1-MODEL-01 — Extrude/inset/bevel

Статус: Частично.

Код: [mesh_modeling.h](../../src/core/mesh_modeling.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

План: Extrude и bevel есть; bevel требует closed consistently-oriented mesh. Добавить inset, region/individual semantics и корректное сохранение attributes.

Тест: Manifold/boundary/concave faces, UV/normals/colors, invalid selection, budgets и atomic undo.

### P1-MODEL-02 — Loop cut и knife

Статус: Не реализовано.

Код: [mesh_modeling.h](../../src/core/mesh_modeling.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

План: Топологические операции с preview/commit, валидацией и переносом UV/color/seam.

Тест: Closed/open/non-manifold topology, vertex/edge hits, canceled preview, triangulation и undo.

### P1-MODEL-03 — Subdivision, mirror и array

Статус: Частично.

Код: [mesh_modeling.h](../../src/core/mesh_modeling.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h).

План: Уже есть; сохранить бюджеты levels 1–4/array 2–64, проверить border/UV/seam contracts и устранить конкретные дефекты вместо дублирования.

Тест: Существующие modeling tests + golden boundary meshes, mirror winding, array normals и apply/undo.

### P1-MODEL-04 — Snapping и proportional editing

Статус: Частично.

Код: [editor_transform.h](../../src/editor/editor_transform.h), [editor_gizmo.h](../../src/editor/editor_gizmo.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

План: Есть grid snap; добавить vertex/edge snap с visual target и bounded falloff для proportional editing.

Тест: Local/world, hidden geometry, large coordinates, precision, radius/falloff endpoints и undo.

### P1-MODEL-05 — Редактирование вершинных цветов

Статус: Частично.

Код: [studio_mesh_types.h](../../src/core/studio_mesh_types.h), [mesh_modeling.h](../../src/core/mesh_modeling.h), [editor_modeling.h](../../src/editor/editor_modeling.h).

План: Хранение/рендер/GLB vertex colors есть; добавить явные paint/fill/edit controls и перенос при новых операциях.

Тест: RGB color-space contract, selected vertices, interpolation после topology edits, round-trip и undo.

### P1-SAVE-01 — Слоты и версионирование игровых сохранений

Статус: Не реализовано.

Код: [game_project.h](../../src/runtime/game_project.h), [secure_asset_io.h](../../src/io/secure_asset_io.h).

План: Отдельный per-game local save root и bounded versioned slots; атомарные writes, миграции и recovery; scene editing recovery не выдавать за game saves.

Тест: Traversal/NUL/symlink/reparse, interrupted writes, old/new version, malformed state, слот после relocation; Lua/C# parity.

### P1-INPUT-01 — Ребинд ввода

Статус: Частично.

Код: [game_bindings.h](../../src/scripting/game_bindings.h), [editor_preferences.h](../../src/editor/editor_preferences.h).

План: Есть input_key/scancodes; добавить action map, press/release/held, persisted per-user overrides и editor conflicts.

Тест: Duplicate/unbound bindings, modifiers, focus/text capture, UTF-8 names, local save и old API compatibility.

### P1-INPUT-02 — SDL GameController

Статус: Не реализовано.

Код: [main.cpp](../../src/main.cpp), [game_bindings.h](../../src/scripting/game_bindings.h).

План: Использовать уже имеющийся SDL2: hotplug, оси/deadzones, player assignment и offline controller mappings.

Тест: Virtual controller fixtures, hotplug/unplug, axis extremes, focus, multiple devices, keyboard coexistence.

### P1-INPUT-03 — Локальный сплит-скрин

Статус: Не реализовано.

Код: [main.cpp](../../src/main.cpp), [game_gui.h](../../src/runtime/game_gui.h), [viewport_target.h](../../src/render/viewport_target.h).

План: Несколько local players/cameras/viewports/input contexts, independent GUI и shared simulation без networking.

Тест: 2/4 views, resize/HDR, GUI clipping, input isolation, camera aspect и resource cleanup.


## P2 — Офлайн-возможности и стабилизация

Тесты ниже — критерии приёмки, если в журнале проверок не указан фактический запуск.

### LIMIT-14 — Long-path support не снимает полевые лимиты

Статус: Частично.

Код: [asset_io.h](../../src/io/asset_io.h), [editor_project.h](../../src/editor/editor_project.h), [script_document.h](../../src/editor/script_document.h), [windows.manifest](../../assets/windows.manifest).

План: Сохранить wide/extended paths и anchored I/O; отдельно согласованно расширять поля UI/формата, а не отключать проверки длины.

Тест: Нативные Unicode-пути >260, пределы 511/512/8192, case aliases, reparse race; отказ без усечения.

### P2-AUDIO-01 — 3D-аудио, затухание и микшер

Статус: Частично.

Код: [audio_backend.c](../../extensions/audio/audio_backend.c), [studio_sounds.h](../../src/runtime/studio_sounds.h), [editor_audio.h](../../src/editor/editor_audio.h).

План: Spatial position/listener/loop уже есть; добавить exposed attenuation/cones и bounded buses/groups с gain/mute/ducking.

Тест: Null-device PCM, distance/pan/cones, bus hierarchy cycles/clipping, Stop/orphan cleanup и saved settings.

### P2-AUDIO-02 — Музыка с дисковым стримингом и loop points

Статус: Частично.

Код: [audio_backend.c](../../extensions/audio/audio_backend.c), [secure_asset_io.h](../../src/io/secure_asset_io.h).

План: Сейчас decoder стримит из удерживаемых encoded bytes; добавить bounded ring/file streaming из pinned handles и loop points, не re-open небезопасный путь.

Тест: Длинный файл при постоянном RAM, underrun/seek/loop/end, corrupt input, file swap и shutdown.

### P2-GUI-01 — Якоря, layout и новые виджеты

Статус: Частично.

Код: [game_gui.h](../../src/runtime/game_gui.h), [game_state.h](../../src/runtime/game_state.h), [editor_gui.h](../../src/editor/editor_gui.h).

План: Есть 128 flat label/button/panel/progress; добавить tree layout, anchors, slider/dropdown/scroll-view и bounded input handling.

Тест: DPI/aspect resize, nesting/cycle caps, clipping/hit order, keyboard/controller focus, serialization/undo.

### P2-GUI-02 — Темы, локализация и controller navigation

Статус: Не реализовано.

Код: [game_gui.h](../../src/runtime/game_gui.h), [editor_gui.h](../../src/editor/editor_gui.h).

План: Локальные таблицы string IDs/fallback, theme resources и focus navigation; docs EN/RU не считать локализацией игры.

Тест: Missing keys, UTF-8 text/font coverage, focus disabled/hidden widgets, no external font/service lookup.

### P2-SCRIPT-01 — Hot reload Lua/C#

Статус: Частично.

Код: [code_editor.h](../../src/editor/code_editor.h), [csharp_bridge.h](../../src/scripting/csharp_bridge.h), [luaryx_runtime.h](../../src/scripting/luaryx_runtime.h).

План: Есть явный Run/reinit; добавить validated compile-then-swap, state transfer contract и rollback, сохраняя trust/deadline/module confinement.

Тест: Syntax/type error сохраняет рабочий runtime; on_stop/on_start ровно раз, module cycles, trust на изменённый код.

### P2-SCRIPT-02 — Luaryx input/timers/tweens/raycast/save API

Статус: Частично.

Код: [luaryx_runtime.h](../../src/scripting/luaryx_runtime.h), [game_bindings.h](../../src/scripting/game_bindings.h), [Scene.cs](../../extensions/csharp/Scene.cs).

План: Связать новые input/physics/save services через stable IDs; timer/tween работают по simulation clock и очищаются при Stop/load.

Тест: Pause/step/restart, canceled callback, cap/recursion, destroyed entity, Lua/C# parity и sandbox enforcement.

### P2-SCRIPT-03 — Автодополнение, шаблоны и профайлер

Статус: Частично.

Код: [code_editor.h](../../src/editor/code_editor.h), [script_document.h](../../src/editor/script_document.h), [editor_debug.h](../../src/editor/editor_debug.h).

План: Есть базовые templates/highlighting/diagnostics; добавить offline API completion и bounded per-script timings/allocations без telemetry.

Тест: Unicode cursor ranges, cancel/no network, stale diagnostics, per-script budget, profiler disabled overhead.

### P2-ASSET-01 — GLB/GLTF анимации и скины

Статус: Частично.

Код: [scene_assets.h](../../src/io/scene_assets.h), [studio_mesh_types.h](../../src/core/studio_mesh_types.h).

План: Сейчас static GLB; animation/skin unsupported reported. Добавить .gltf + external buffers/images только в import root, joints/channels/weights и export contracts.

Тест: glTF fixtures/validator, path/URI/data bounds, sparse accessors, hierarchy cycles, key interpolation, round-trip и budget failure.

### P2-ASSET-02 — Браузер ассетов, hot reload и зависимости

Статус: Частично.

Код: [project_workspace.h](../../src/editor/project_workspace.h), [scene_assets.h](../../src/io/scene_assets.h), [material_render.h](../../src/render/material_render.h).

План: Есть drop GLB/audio и texture mtime keys; добавить локальный asset index, preview cache, dependency graph и debounced invalidation.

Тест: Rename/delete/change, dependency cycles, stale preview jobs, failed reload rollback, same-name different files, confined handles.

### P2-UX-01 — Outliner, parenting, selection и gizmo

Статус: Частично.

Код: [editor_ui.h](../../src/editor/editor_ui.h), [editor_selection.h](../../src/editor/editor_selection.h), [editor_gizmo.h](../../src/editor/editor_gizmo.h).

План: Search/multiselect/local-world gizmo уже есть. Добавить parenting с stable IDs, keep-world/reparent semantics и защиты от циклов.

Тест: Reparent mirrored/nonuniform transforms, recursive deletion/undo, filtered multiselect и local/world gizmo compatibility.

### P2-UX-02 — Ортографические виды, view cube, камеры и измеритель

Статус: Частично.

Код: [editor_camera_math.h](../../src/editor/editor_camera_math.h), [editor_gizmo.h](../../src/editor/editor_gizmo.h), [editor_transform.h](../../src/editor/editor_transform.h).

План: Есть perspective/free camera; добавить orthographic projection/picking, cube navigation, local bookmarks и measurement overlay.

Тест: Ortho/perspective picking, aspect/zoom extremes, matching gizmo matrices, bookmark round-trip, units/large coordinates.

### P2-UX-03 — Profiler overlay, консоль и hotkey editor

Статус: Частично.

Код: [editor_debug.h](../../src/editor/editor_debug.h), [editor_preferences.h](../../src/editor/editor_preferences.h), [editor_ui.h](../../src/editor/editor_ui.h).

План: FPS/render counters/console уже есть; добавить bounded overlay памяти/таймингов и редактор конфликтов shortcut с local prefs.

Тест: Counters reconcile, disabled overhead, bounded log, focus-aware shortcuts, restore defaults и no telemetry.

### P2-UX-04 — Шаблоны проектов и tutorial-сцены

Статус: Частично.

Код: [example_game.h](../../src/examples/example_game.h), [project_workspace.h](../../src/editor/project_workspace.h).

План: Есть sample Gem Trail; добавить shipped offline templates и пошаговые scenes с актуальными EN/RU инструкциями.

Тест: Каждый template открывается/сохраняется, script compile, packaged gameplay, нет missing/network assets.

### P2-NODES-01 — Стабилизация geometry nodes

Статус: Частично.

Код: [geometry_nodes.h](../../src/core/geometry_nodes.h), [editor_nodes.h](../../src/editor/editor_nodes.h).

План: Есть 8 типов, один input/geometry node, 32 nodes и preview/commit. Добавить typed sockets/nodes/undo и удобный editor без silent data loss.

Тест: Cycles, disconnected/invalid types, graph budgets, preview cancellation/stale workers, save/undo/bake parity.

### P2-CACHE-01 — Cached rendering и presets

Статус: Частично.

Код: [render_geometry.h](../../src/render/render_geometry.h), [material_render.h](../../src/render/material_render.h), [editor_preferences.h](../../src/editor/editor_preferences.h).

План: Есть cache/presets и oversubscription fallback; расширить invalidation на новые maps/skins/instances, сделать capability/result видимыми.

Тест: Cached/reference image parity, undo/skin/material edits, active-set overflow, driver fallback и resource counters.


## P3 — Только после P1/P2

Тесты ниже — критерии приёмки, если в журнале проверок не указан фактический запуск.

### P3-STRETCH-01 — Terrain / particles / decals / A* / 2D

Статус: После P1/P2.

Код: [scene_core.h](../../src/core/scene_core.h).

План: Не начинать до завершения согласованных P1/P2. После этого отдельные offline slices, бюджеты и format/API контракты.

Тест: Отдельный acceptance corpus и native gates для каждой подсистемы; сейчас не реализовано и не проверено.

## Все 40 пунктов исходного аудита

Исходные статусы относятся к выпущенной 1.3.0/1.3.1 и не являются результатами будущего обновления. Все записи сохранены в JSON; действующие ограничения вынесены выше. `fixed` не означает отсутствие других дефектов.

| ID | Статус источника | Evidence |
| --- | --- | --- |
| SEC-01 | fixed | tests/v13_regressions.h; tools/test_audit_security.py |
| SEC-02 | fixed | tools/soak_project_parser.py; src/io/json_budget.h |
| SEC-03 | fixed | tests/worker_process_tests.cpp; tools/test_audit_security.py |
| GAME-01 | fixed | tests/v13_regressions.h |
| GAME-02 | fixed | tests/v13_regressions.h |
| REL-01 | fixed | src/editor/editor_history.h; src/editor/code_editor.h |
| REL-02 | fixed | src/editor/editor_history.h; src/editor/editor_ui.h |
| REL-03 | fixed | src/main.cpp; src/editor/editor_preferences.h |
| MEM-01 | fixed | src/render/material_render.h; native rendering tests |
| MEM-02 | fixed | src/io/game_package.h; tools/test_game_package.py |
| MEM-03 | fixed | src/editor/editor_history.h |
| PERF-01 | fixed | tests/physics_differential.cpp; src/core/spatial_index.h |
| PERF-02 | fixed | tests/v13_regressions.h |
| SEC-04 | external_requirement | Release signing verification |
| SEC-05 | partial | SBOM.cdx.json; extensions/csharp/packages.lock.json |
| SEC-06 | partial | tools/soak_project_parser.py; .github/workflows/endurance.yml |
| SEC-07 | contract | src/io/game_package.h; release provenance |
| SEC-08 | fixed | tools/test_mcp_capabilities.py |
| GAME-03 | contract | tests/game_features_tests.h |
| REL-04 | fixed | src/io/game_package.h |
| REL-05 | fixed | src/editor/editor_project.h |
| REL-06 | fixed | tests/v13_regressions.h; src/scripting/luaryx_runtime.h |
| REL-07 | fixed | src/scripting/luaryx_runtime.h; extensions/csharp/Scene.cs |
| REL-08 | fixed | tests/engine_tests.cpp; src/core/scene_tools.h |
| REL-09 | fixed | tests/v13_regressions.h |
| MEM-04 | fixed | src/render/material_render.h |
| MEM-07 | fixed | src/render/render_geometry.h |
| MEM-05 | fixed | extensions/audio/audio_backend.c; native audio tests |
| MEM-06 | limited_evidence | parser-soak.json; sanitizer CI |
| PERF-05 | fixed | src/io/scene_assets.h |
| PERF-04 | fixed | src/runtime/gameplay_runtime.h |
| PERF-03 | fixed | src/core/scene_tools.h; src/editor/editor_debug.h |
| PERF-06 | fixed | src/scripting/export_worker.h |
| COMP-01 | fixed | assets/windows.manifest; src/io/asset_io.h |
| COMP-02 | contract | src/io/scene_assets.h; docs/en/README.md |
| COMP-03 | contract | src/io/game_package.h; tests/engine_tests.cpp |
| QA-01 | limited_evidence | CI and Verification.mp4 |
| QA-02 | limited_evidence | Native CI matrix |
| COMP-04 | fixed | LICENSE; NOTICE; CONTRIBUTING.md |
| QA-03 | fixed | tools/package_release.py; .github/workflows/release.yml |

## Обязательные регрессии 1.3.1

| ID | Случаи | Источник |
| --- | --- | --- |
| R131-GPU | 100 resize; 50 HDR/post switches; scoped GL deletion | [viewport_postprocess_tests.cpp](../../tests/viewport_postprocess_tests.cpp) |
| R131-TEX | bad_alloc injection; deleted texture ID; zero accounting; successful retry | [render_parity_tests.cpp](../../tests/render_parity_tests.cpp) |
| R131-AUDIO | 10 WAV clips; preview Stop; repeated Play/Stop; orphan voice; zero live voices | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-BOUNDS | NaN/+Inf/-Inf in every coordinate; mirrored finite control | [core_tests.cpp](../../tests/core_tests.cpp) |
| R131-DIRTY | camera/widget/color -0 equals +0; real text edit changes hash | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-OFFSETS | valid sparse >3 GiB; UINT64_MAX footer rejected transactionally; 128 MiB runtime cap | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-LUA | embedded NUL/control/oversize suffix rejected; UTF-8/spaces/default/empty names retained | [v131_regressions.h](../../tests/v131_regressions.h) |
| R131-GUIDE | GUIDE terminator and unclosed fences rejected before output; valid lists/Unicode | [test_release_safety.py](../../tools/test_release_safety.py) |
| R131-MAC | external paths and earlier external rpath candidate rejected; worker-relative valid control | [test_release_safety.py](../../tools/test_release_safety.py) |

## Evidence и релиз

Нативный baseline CI 1.3.1: https://github.com/kittkanev/Lunoryx/actions/runs/43246269279. Parser endurance: https://github.com/kittkanev/Lunoryx/actions/runs/43703840148. Это историческое evidence точного baseline SHA; Windows/macOS не запускались заново в текущей Linux-среде.

Релиз-гейт: native CI каждой заявленной платформы, ASan/UBSan/leaks, graphics lifecycle, relocated packaged C# gameplay, свежее настоящее UI-видео. Ровно четыре загруженных assets: Windows ZIP, universal macOS ZIP, Verification.mp4, Audit-report.html; SBOM/checksums и лицензии внутри архивов. Сертификаты не предоставлены: не заявлять Authenticode/Developer ID/notarization. Новые security-дефекты сообщать владельцу приватно; не публиковать issue/эксплойт.
