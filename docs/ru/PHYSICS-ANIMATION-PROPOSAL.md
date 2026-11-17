# Предложение зависимостей P1 — требуется одобрение

Ни одна из библиотек ниже не скачана, не завендорена и не добавлена в сборку. По правилу владельца новые зависимости требуют явного одобрения.

| Компонент | Предлагаемый pin исходников | Лицензия | Применение |
| --- | --- | --- | --- |
| Jolt Physics 5.7.0 | `jrouwe/JoltPhysics`, `cf16769bb0ede9c45d57d06068ff0689e9702f5c` | MIT | Rigid bodies, контакты/констрейнты, формы, broad phase, ray/overlap и поддержка character controller |
| ozz-animation 0.17.0 | `guillaumeblanc/ozz-animation`, `744eb9d99f606eda849acb0b1204f7a3dc20bca1` | MIT | Скелетные позы, sampling/blending, reference skinning и примитивы IK |

Pins и upstream-лицензии проверены чтением метаданных GitHub. `Jolt/Jolt.cmake` явно требует C++17. Нативная интеграция Lunoryx, производительность, безопасность и совместимость платформ **ещё не проверены**.

После одобрения:

1. Поместить закреплённые исходники и неизменённые лицензии в репозиторий; обновить NOTICE/SBOM. Не вводить runtime-downloader, облачный API, аккаунт, телеметрию или сервис.
2. Собирать static libraries локально в существующей матрице GCC/MinGW/Apple Clang. Отключить samples/viewers/tools, внешний FBX SDK, тесты с загрузкой чужих frameworks и configure-time downloads. Импорт остаётся за существующим tinygltf.
3. Явно отключить необязательные AVX/AVX2/AVX-512/FMA/F16C и host-native tuning для переносимого x64. На Apple Silicon использовать ARM-путь. Не обещать битовую детерминированность между платформами без реальных проверок.
4. Сохранить явный legacy AABB backend для старых проектов и differential tests. Новой физике нужны протестированная миграция, stable IDs, бюджеты и editor/script API, а не только linked library.
5. Реализовать skeleton/editor/glTF/state machine/root motion/events/shape keys отдельными вертикальными частями с тестами. ozz не является готовым редактором и не закрывает весь P1 автоматически.
6. До релизных заявлений нужны native CI, санитайзеры, lifecycle, interop, persistence/undo/migration и packaged gameplay.

Альтернативы: одобрить только Jolt и написать анимацию локально; либо не добавлять обе библиотеки и реализовать оба ядра самостоятельно. Последнее исключает зависимости, но заметно увеличивает объём нового solver/animation-кода и его проверки. Существующие Manifold/xatlas не предоставляют запрошенные rigid-body solver и skeleton runtime.

Источники: [релиз Jolt](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.7.0), [лицензия Jolt](https://github.com/jrouwe/JoltPhysics/blob/cf16769bb0ede9c45d57d06068ff0689e9702f5c/LICENSE), [релиз ozz](https://github.com/guillaumeblanc/ozz-animation/releases/tag/0.17.0), [лицензия ozz](https://github.com/guillaumeblanc/ozz-animation/blob/744eb9d99f606eda849acb0b1204f7a3dc20bca1/LICENSE.md).
