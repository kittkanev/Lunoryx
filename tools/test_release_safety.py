"""Regression tests for generated C++ and relocatable macOS dependencies."""
import copy
import json
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path

from build_manual import build, markup, raw_literal
from macos_bundle_checks import read_version, validate_dependencies


class ManualTests(unittest.TestCase):
    def test_cpp_terminators_rejected_before_output(self):
        for lang in ('en', 'ru'):
            for body in (')GUIDE"', 'prefix)GUIDE"; static_assert(false); R"GUIDE(suffix', '```lua\nunfinished'):
                with self.subTest(lang=lang, body=body), tempfile.TemporaryDirectory() as folder:
                    root=Path(folder)
                    for directory in ('src/editor', 'docs/manual', 'docs/en', 'docs/ru'):
                        (root/directory).mkdir(parents=True)
                    header=root/'src/editor/editor_guide_data.h'
                    header.write_text('unchanged')
                    topic={language:dict(title='Title',body='Safe «text»') for language in ('en','ru')}
                    topics=[copy.deepcopy(topic),copy.deepcopy(topic)]
                    topics[-1][lang]['body']=body
                    (root/'docs/manual/topics.json').write_text(json.dumps(topics))
                    with self.assertRaises(ValueError):build(root)
                    self.assertEqual(header.read_text(),'unchanged')
                    self.assertFalse((root/'docs/en/index.html').exists())
        self.assertEqual(raw_literal('Quotes " and \\ and Русский\n)GUIDE'), 'R"GUIDE(Quotes " and \\ and Русский\n)GUIDE)GUIDE"')

    def test_lists_and_code_remain_structured_and_escaped(self):
        class Structure(HTMLParser):
            stack=[]
            def handle_starttag(self,tag,attrs):
                if tag=='li':assert self.stack[-1]=='ul'
                self.stack.append(tag)
            def handle_endtag(self,tag):
                assert self.stack.pop()==tag
        rendered=markup('- one\n- <two>\n\nParagraph\n### Heading\n- three\n```lua\nprint("<tag>")\n```')
        parser=Structure();parser.feed(rendered);self.assertEqual(parser.stack,[])
        self.assertIn('&lt;two&gt;',rendered);self.assertIn('&lt;tag&gt;',rendered)
        self.assertEqual(rendered.count('<li>'),3)


class MacDependencies(unittest.TestCase):
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.app=Path(self.folder.name)/'Lunoryx.app'
        self.native=self.app/'Contents/MacOS/Lunoryx'
        self.library=self.app/'Contents/Frameworks/lib sample.dylib'
        for path in (self.native,self.library):path.parent.mkdir(parents=True,exist_ok=True);path.touch()

    def check(self,dependency,commands='',native=None):
        validate_dependencies(native or self.native,self.app,f'native:\n\t{dependency} (compatibility version 1.0.0, current version 1.0.0)\n',commands)

    def test_external_absolute_and_relative_paths_rejected(self):
        for path in ('/opt/homebrew/lib/a.dylib','/usr/local/lib/a.dylib','/opt/local/lib/a.dylib',
                     '/Users/alice/lib a.dylib','/tmp/a.dylib','/private/tmp/a.dylib','/other/a.dylib',
                     '/usr/lib/../../tmp/a.dylib','relative.dylib','@loader_path/../../../escape.dylib'):
            with self.subTest(path=path),self.assertRaises(ValueError):self.check(path)

    def test_system_and_actual_bundle_dependencies_allowed(self):
        for path in ('/usr/lib/libSystem.B.dylib','/System/Library/Frameworks/Cocoa.framework/Versions/A/Cocoa',
                     '@loader_path/../Frameworks/lib sample.dylib','@executable_path/../Frameworks/lib sample.dylib'):
            self.check(path)
        commands='cmd LC_RPATH\n cmdsize 40\n path @loader_path/../Frameworks (offset 12)\n'
        self.check('@rpath/lib sample.dylib',commands)
        with self.assertRaises(ValueError):self.check('@rpath/missing.dylib',commands)
        with self.assertRaises(ValueError):self.check('/usr/lib/libSystem.B.dylib',commands.replace('@loader_path/../Frameworks','/tmp'))

    def test_dylib_identity_is_not_a_dependency(self):
        self.check('@rpath/lib sample.dylib','cmd LC_ID_DYLIB\n name @rpath/lib sample.dylib (offset 24)',self.library)

    def test_worker_executable_paths_resolve_from_worker(self):
        worker=self.app/'Contents/Resources/CSharp/arm64/Luaryx.Worker'
        worker.parent.mkdir(parents=True);worker.touch()
        library=worker.parent/'libhostfxr.dylib';library.touch()
        libraries='worker:\n @executable_path/libhostfxr.dylib (compatibility version 1.0.0)'
        validate_dependencies(worker,self.app,libraries,'',executable_path=worker)
        with self.assertRaises(ValueError):validate_dependencies(worker,self.app,libraries,'')

    def test_version_errors_are_explicit(self):
        self.assertEqual(read_version('#define LUNORYX_VERSION "1.3.1"\n'),'1.3.1')
        for text in ('','#define OTHER "1.3.1"','#define LUNORYX_VERSION "bad"'):
            with self.assertRaises(ValueError):read_version(text)


if __name__=='__main__':unittest.main()
