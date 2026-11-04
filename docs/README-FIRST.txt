LUNORYX STUDIO 1.2.1

WINDOWS X64
Запустите Lunoryx.exe. Отдельные SDL/Lua/audio DLL и консоль не нужны.
Требуется OpenGL 2.1 compatibility с framebuffer objects.

ПЕРВАЯ ИГРА
Build Game > New third-person example; Play Simulation / F6.
WASD — движение, Space — прыжок; Stop восстанавливает редакторскую сцену.
Build Game: название, PNG-иконка, Fullscreen / Windowed, размер окна,
VSync, стартовый скрипт и путь EXE. Сохраните .lunoryx и нажмите Build.
Игра упаковывает сцену, GUI, скрипты, текстуры и звуки в один файл.

КАМЕРА И ВЫДЕЛЕНИЕ
Free Fly: ПКМ + WASD, Q/E вниз/вверх, Shift быстрее, Ctrl медленнее.
Tab — Object/Edit; 1/2/3 — вершины/рёбра/грани; B — рамка, C — круг.
Alt+Z — X-ray; Q/W/E/R — Select/Move/Rotate/Scale вне полёта.
Ctrl+S — сохранить; Ctrl+Z/Ctrl+Y — Undo/Redo.

GUI, ТРИГГЕРЫ И АНИМАЦИИ
Window > GUI Designer — текст, кнопки, панели и полосы прогресса.
Tools > Build — Killzone, Checkpoint, Script trigger.
Lua: on_trigger(event, zone, actor), on_gui_click(id), gui_text/visible/value.
Window > Animation — ключевые кадры объектов и предпросмотр.
Window > Debugger — пауза, шаг симуляции, границы коллизий и ошибки Lua.

КАЧЕСТВО
Settings: Render preset, Render scale, Exposure и размер текста.
F8 — Office / Balanced / Quality. Доступны PBR, HDR и тени солнца.
Производительность зависит от сцены, GPU и выбранных эффектов.

СПРАВКА EN/RU
Window > API Docs — встроенная справка, примеры и лицензии.
docs/ru/index.html и docs/en/index.html — оформленные офлайн-руководства.
docs/PERFORMANCE.md — методика измерений и ограничения замеров.

ПРОЕКТЫ
Перед сохранением и сборкой остановите Play Simulation.
File > Save Project / Save Project As сохраняет .lunoryx.
Загрузка в редактор не запускает Lua автоматически.
Lua-скрипты считаются доверенными; стандартные библиотеки доступны.

ГРАНИЦЫ
Физика AABB; статический GLB; объектные анимации без скелетного рига.
Нет трассировки лучей, HDRI-отражений и полной совместимости с Blender.
HDR использует LDR fallback, если GPU не поддерживает нужный framebuffer.

Релизы: https://github.com/kittkanev/Lunoryx/releases
Ошибки: https://github.com/kittkanev/Lunoryx/issues
