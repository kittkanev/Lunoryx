Lunoryx Studio 1.3.1 fixes issues reported in the Gemini Argon audit:

- Audio buffers are unloaded on Stop, including previews and orphaned voices; edit-world snapshots are released.
- Viewport targets own their GL resources, release post attachments immediately, and retain the tested HDR → LDR fallback. Texture uploads clean up after allocation failures and reuse mip buffers; project changes clear GPU caches.
- Non-finite vertices invalidate bounds. Signed zero no longer marks game settings as changed.
- Package reads use 64-bit file offsets, with a sparse file beyond 3 GiB covered by native regression tests. Existing 1 GiB payload and 128 MiB runtime budgets remain.
- Lua sound paths/names are checked with their complete byte lengths; NUL/control bytes are rejected before path resolution.
- Manual generation rejects C++ raw-string terminators and unclosed fences before writing output; HTML lists have proper containers.
- macOS packaging validates every bundled Mach-O dependency/rpath, rejects external paths, cleans icon staging, and validates the version macro.

Native CI, sanitizers, graphics lifecycle tests, packaged C# gameplay and a fresh actual UI recording gate publication. Exactly four uploaded files: Windows ZIP, universal macOS ZIP, Verification.mp4 and Audit-report.html.

Windows has no publisher signature; macOS 15+ is ad-hoc signed, without Apple notarization. C# runs with user OS permissions. Finite tests do not establish the absence of all bugs or vulnerabilities.

---

Исправления по аудиту Gemini Argon: очистка аудио при Stop и GPU при смене проекта, RAII viewport, безопасная загрузка текстур, проверка NaN/Inf, корректный флаг несохранённых изменений, 64-битные смещения пакетов, полная проверка Lua-строк звуков, защита генератора справки и проверка зависимостей macOS.

Нативные проверки Windows/Linux/macOS, санитайзеры, создание самостоятельной C#-игры и новая видеозапись выполняются перед публикацией. В релизе ровно четыре вложения; подробности и пределы проверки — в HTML-отчёте. Подписи издателя и нотарификации Apple нет.
