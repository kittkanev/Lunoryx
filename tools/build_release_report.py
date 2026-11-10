"""Create a source-linked HTML acceptance report from the successful CI run."""
import argparse
import collections
import hashlib
import html
import json
import subprocess
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--run',required=True)
p.add_argument('--jobs',required=True)
p.add_argument('--evidence',required=True)
p.add_argument('--release-dir',required=True)
a=p.parse_args();run=json.loads(Path(a.run).read_text());jobs=json.loads(Path(a.jobs).read_text())['jobs']
commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
assert run['head_sha']==commit and run['conclusion']=='success'
assert jobs and all(j['conclusion']=='success' for j in jobs)
evidence=Path(a.evidence);video=json.loads((evidence/'video-evidence.json').read_text());soak=json.loads((evidence/'parser-soak.json').read_text())
assert video['passed'] and soak['passed'] and soak['cycles']>=1000
matrix=json.loads(Path('docs/audit-v1.3.0.json').read_text());assert len(matrix)==40
release=Path(a.release_dir)
assets=['Lunoryx-windows-x64.zip','Lunoryx-macos-universal.zip','Verification.mp4']
hashes={name:hashlib.sha256((release/name).read_bytes()).hexdigest() for name in assets}
esc=lambda x:html.escape(str(x),quote=True)
labels=dict(fixed='Исправлено',contract='Явный контракт',partial='Частичная проверка',external_requirement='Нужен сертификат',limited_evidence='Ограниченные доказательства')
counts=collections.Counter(row['status'] for row in matrix)
rows=''.join(f'<tr><td><strong>{esc(row["id"])}</strong></td><td class="{esc(row["status"])}">{esc(labels[row["status"]])}</td><td>{esc(row["resolution"])}</td><td><code>{esc(row["evidence"])}</code></td></tr>' for row in matrix)
jobrows=''.join(f'<li><a href="{esc(job["html_url"])}">{esc(job["name"])}</a>: success</li>' for job in jobs)
hashrows=''.join(f'<tr><td>{esc(name)}</td><td><code>{digest}</code></td></tr>' for name,digest in hashes.items())
samples=soak.get('samples',[])
memory=''
if samples:memory=f'RSS: {samples[0]["rss_kib"]/1024:.1f} → {samples[-1]["rss_kib"]/1024:.1f} MiB; открытые дескрипторы: {samples[0]["descriptors"]} → {samples[-1]["descriptors"]}.'
content=f'''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lunoryx 1.3.0 — verification report</title><style>
body{{font:16px/1.6 system-ui,sans-serif;margin:0;background:#10151e;color:#e4ecf7}}main{{max-width:1260px;margin:auto;padding:40px 24px}}h1,h2{{line-height:1.2}}h1{{font-size:40px}}a{{color:#66c9ff}}code,pre{{font:13px/1.5 ui-monospace,monospace;overflow-wrap:anywhere}}pre{{white-space:pre-wrap}}table{{border-collapse:collapse;width:100%;font-size:14px}}th,td{{text-align:left;vertical-align:top;border:1px solid #344053;padding:12px}}th{{background:#202c3d}}.fixed{{color:#81e8ac}}.contract{{color:#7acfff}}.partial,.external_requirement,.limited_evidence{{color:#ffd18d}}.note{{background:#202c3d;padding:20px;border-left:4px solid #66c9ff}}video{{max-width:100%;border-radius:8px}}details{{padding:14px 0}}.scroll{{overflow:auto}}small{{color:#a4b7d0}}</style>
<main><small>Lunoryx Studio · English / Русский</small><h1>Проверка релиза 1.3.0</h1>
<p>Проверенный коммит: <a href="https://github.com/kittkanev/Lunoryx/commit/{commit}"><code>{commit}</code></a>.
<a href="{esc(run['html_url'])}">Успешный полный CI</a>. Лицензия движка: GPL-3.0-only.</p>
<div class="note">Выполнены проверки перечисленных сценариев. Конечный набор тестов не доказывает отсутствие всех возможных ошибок, уязвимостей, утечек или проблем драйверов. Ниже отдельно указаны ограничения и вопросы, требующие внешних сертификатов или оборудования.</div>
<h2>Нативные сборки</h2><ul>{jobrows}</ul>
<p>Windows x64 включает редактор и настоящую среду C#; macOS ZIP содержит универсальный .app для Apple Silicon и Intel и обе среды C#. В архивах находятся SHA-256 и SBOM. C# выполняется в отдельном процессе с правами пользователя и требует доверия загруженному проекту на текущий сеанс.</p>
<h2>Видеопроверка</h2><video controls preload="metadata" src="Verification.mp4"></video>
<p><a href="https://github.com/kittkanev/Lunoryx/releases/download/v1.3.0/Verification.mp4">Скачать MP4</a>. Запись реального окна через X11/ffmpeg; действия выполняются мышью, клавиатурой и вставкой текста. Исходники не подменяют кадры. Проверено: выбор и создание папки проекта, создание C# из Explorer, редактирование, Roslyn-компиляция, сообщение C# → Lua, свойство Lua → общий объект, сохранение и Play/Stop.</p>
<h2>Стресс и память</h2><p>{soak['cycles']} циклов мутаций и загрузки проекта, Undo/Redo: {soak['accepted']} принято, {soak['rejected']} отклонено. Время: {soak['seconds']} с. {esc(memory)} Это проверка CPU-парсера и состояния документа, не доказательство отсутствия утечек GPU за несколько часов.</p>
<p>Дополнительно проверяются 10 000 одновременных пересечений триггеров, тонкая стена при высокой скорости, стабильные ID, модули, восстановление во время Play, таймаут C# и передача 32 MiB через нативный worker. В CI включены AddressSanitizer/UndefinedBehaviorSanitizer и графические сравнения.</p>
<h2>Все 40 пунктов аудита</h2><p>{esc(', '.join(labels[k]+': '+str(v) for k,v in counts.items()))}.</p><div class="scroll"><table><thead><tr><th>ID</th><th>Статус</th><th>Результат и пределы</th><th>Доказательства / исходник</th></tr></thead><tbody>{rows}</tbody></table></div>
<h2>Подписи и доверие</h2><p>Сертификаты Authenticode и Apple Developer ID не предоставлены. Windows не подписан издателем; macOS подписан ad-hoc и не нотарифицирован Apple. Не выдавайте эти подписи за подтверждение издателя. CRC32 игровых пакетов проверяет повреждения и не подтверждает происхождение. Для загрузок доступны происхождение сборки GitHub и хеши архивов.</p>
<h2>Ровно четыре вложения релиза</h2><p>Два ZIP, Verification.mp4 и этот Audit-report.html. Лицензии, инвентаризация, документация и контрольные суммы находятся внутри архивов. Автоматические архивы исходников GitHub не являются загруженными assets.</p><div class="scroll"><table><tr><th>Файл</th><th>SHA-256</th></tr>{hashrows}</table></div>
<details><summary>Исходные результаты видео</summary><pre>{esc(json.dumps(video,ensure_ascii=False,indent=2))}</pre></details>
<details><summary>Исходные результаты стресс-проверки</summary><pre>{esc(json.dumps(soak,ensure_ascii=False,indent=2))}</pre></details>
<p><a href="https://github.com/kittkanev/Lunoryx/tree/{commit}">Соответствующие исходники</a> · <a href="https://github.com/kittkanev/Lunoryx/actions/runs/{run['id']}">Полные журналы проверок</a>.</p></main></html>'''
(release/'Audit-report.html').write_text(content)
print('Generated source-bound Audit-report.html from successful native CI evidence')
