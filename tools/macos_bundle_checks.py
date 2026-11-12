"""Validate Mach-O dependency paths before signing a relocatable app."""
import re
from pathlib import Path


def read_version(text):
    match = re.search(r'^\s*#\s*define\s+LUNORYX_VERSION\s+"(\d+\.\d+\.\d+)"\s*$', text, re.M)
    if not match:
        raise ValueError('Missing or invalid LUNORYX_VERSION macro')
    return match[1]


def load_paths(commands):
    rpaths, identity, command = [], None, None
    for line in commands.splitlines():
        words = line.split()
        if len(words) == 2 and words[0] == 'cmd':
            command = words[1]
        match = re.match(r'\s*(path|name) (.*?) \(offset \d+\)$', line)
        if match and command == 'LC_RPATH' and match[1] == 'path':
            rpaths.append(match[2])
        if match and command == 'LC_ID_DYLIB' and match[1] == 'name':
            identity = match[2]
    return rpaths, identity


def is_system(path):
    path = str(Path(path))
    return path.startswith(('/usr/lib/', '/System/Library/'))


def validate_dependencies(native, app, libraries, commands, executable_commands='', executable_path=None):
    native, app = Path(native).resolve(), Path(app).resolve()
    executable = Path(executable_path).resolve() if executable_path else app / 'Contents/MacOS/Lunoryx'
    rpaths, identity = load_paths(commands)
    main_rpaths, _ = load_paths(executable_commands)

    def expand(value, loader):
        for token, base in [('@loader_path', loader), ('@executable_path', executable.parent)]:
            if value == token or value.startswith(token + '/'):
                return (base / value[len(token):].lstrip('/')).resolve()
        if value.startswith('/'):
            return Path(value).resolve()
        raise ValueError(f'Unresolved Mach-O path: {value} in {native}')

    def allowed(path):
        return is_system(path) or path.is_relative_to(app)

    search = []
    for value, loader in [(p, native.parent) for p in rpaths] + [(p, executable.parent) for p in main_rpaths]:
        path = expand(value, loader)
        if not allowed(path) or (value.startswith('/') and not is_system(path)):
            raise ValueError(f'External Mach-O rpath: {value} in {native}')
        search.append(path)
    dependencies = [line.strip().split(' (', 1)[0] for line in libraries.splitlines()[1:] if line.strip()]
    # otool includes a dylib's own LC_ID_DYLIB first; it is not a loaded dependency.
    if dependencies and dependencies[0] == identity:
        dependencies.pop(0)
    for value in dependencies:
        if value.startswith('@rpath/'):
            candidates = [(base / value[len('@rpath/'):]).resolve() for base in search]
            if not any(allowed(p) and (is_system(p) or p.is_file()) for p in candidates):
                raise ValueError(f'Unresolved bundled dependency: {value} in {native}')
        else:
            path = expand(value, native.parent)
            if (not allowed(path) or (value.startswith('/') and not is_system(path)) or
                    (not is_system(path) and not path.is_file())):
                raise ValueError(f'External or missing Mach-O dependency: {value} in {native}')


def is_macho(path):
    with Path(path).open('rb') as file:
        return file.read(4) in {bytes.fromhex(m) for m in
                               ('feedface', 'cefaedfe', 'feedfacf', 'cffaedfe',
                                'cafebabe', 'bebafeca', 'cafebabf', 'bfbafeca')}
