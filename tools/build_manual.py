"""Build offline EN/RU HTML, Markdown and in-app guides from one source."""
import json, html
from pathlib import Path
root=Path(__file__).resolve().parents[1]
topics=json.loads((root/'docs/manual/topics.json').read_text())
css='''*{box-sizing:border-box}body{margin:0;background:#0b111b;color:#dce5ee;font:17px/1.7 system-ui,sans-serif}aside{position:fixed;inset:0 auto 0 0;width:265px;background:#111d2b;padding:32px 24px;overflow:auto;border-right:1px solid #293a4c}aside a{display:block;color:#a7becf;text-decoration:none;margin:12px 0}aside b{font-size:26px;color:#66dfd5}main{max-width:1080px;margin-left:265px;padding:48px 64px}h1{font-size:44px;line-height:1.2;color:#fff}h2{font-size:30px;color:#72e2d8;margin-top:46px}h3{font-size:21px;color:#fff}section{border-top:1px solid #28384a;padding:12px 0 32px}pre{background:#142130;border:1px solid #30485b;border-radius:10px;padding:22px;overflow:auto;color:#b9ede1;font:15px/1.65 ui-monospace,monospace}input{width:100%;padding:10px;background:#0b111b;border:1px solid #3c5568;border-radius:6px;color:white}a{color:#6ed6f1}.badge{color:#68d9ce;letter-spacing:2px;font-size:13px}li{margin:8px 0}footer{color:#92a6b5;border-top:1px solid #293a4c;padding:24px 0}@media(max-width:800px){aside{position:static;width:auto}main{margin:0;padding:24px}h1{font-size:32px}}@media print{aside{display:none}main{margin:0;padding:0}body{background:white;color:black}h1,h2,h3{color:black}pre{white-space:pre-wrap}}'''
def markup(text):
    out=[];code=[];inside=False
    for line in text.splitlines():
        if line.startswith('```'):
            if inside: out.append('<pre><code>'+html.escape('\n'.join(code))+'</code></pre>');code=[]
            inside=not inside;continue
        if inside:code.append(line);continue
        if line.startswith('### '):out.append('<h3>'+html.escape(line[4:])+'</h3>')
        elif line.startswith('- '):out.append('<li>'+html.escape(line[2:])+'</li>')
        elif line:out.append('<p>'+html.escape(line)+'</p>')
    return '\n'.join(out)
embedded=['#pragma once\nstruct StudioGuideTopic { const char* Title[2]; const char* Body[2]; };\nstatic const StudioGuideTopic kStudioGuide[]={']
for topic in topics:
    embedded.append('{{'+','.join(json.dumps(topic[x]['title'],ensure_ascii=False) for x in ('en','ru'))+'},{'+','.join('R"GUIDE('+topic[x]['body']+')GUIDE"' for x in ('en','ru'))+'}},')
embedded.append('};\n')
(root/'src/editor/editor_guide_data.h').write_text('\n'.join(embedded))
for lang in ('en','ru'):
    title='Creator Handbook' if lang=='en' else 'Руководство автора'
    nav=''.join(f'<a href="#s{i}">{html.escape(t[lang]["title"])}</a>' for i,t in enumerate(topics))
    sections=''.join(f'<section id="s{i}"><h2>{html.escape(t[lang]["title"])}</h2>{markup(t[lang]["body"])}</section>' for i,t in enumerate(topics))
    page=f'''<!doctype html><html lang="{lang}"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>Lunoryx — {title}</title><style>{css}</style><aside><b>Lunoryx</b><p>{title}</p><a href="../{'ru' if lang=='en' else 'en'}/index.html">English / Русский</a><input id="search" placeholder="Search / Поиск">{nav}</aside><main><div class="badge">STUDIO · OFFLINE DOCUMENTATION</div><h1>{title}</h1><p>{'Build, animate, test, ship.' if lang=='en' else 'Создавайте сцены, анимации и игры.'}</p>{sections}<footer>Lunoryx · EN / RU · No network required</footer></main><script>document.querySelector('#search').oninput=e=>{{let q=e.target.value.toLowerCase();document.querySelectorAll('section').forEach(s=>s.hidden=!s.textContent.toLowerCase().includes(q));}};</script></html>'''
    folder=root/'docs'/lang;folder.mkdir(exist_ok=True)
    (folder/'index.html').write_text(page)
    (folder/'README.md').write_text('# Lunoryx — '+title+'\n\n'+'\n\n'.join('## '+t[lang]['title']+'\n\n'+t[lang]['body'] for t in topics).rstrip()+'\n')
print('Generated EN/RU manuals and embedded guide')
