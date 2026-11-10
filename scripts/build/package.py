#!/usr/bin/env python3
"""Build one backend in its Docker SDK and zip it with everything it needs.

usage: package.py BACKEND [--version X.Y.Z] [--skip-build]

Writes dist/isaac-<version>-<backend>.zip containing the executable, the
libraries its SDK built itself (Linux: lib/, Windows: beside the .exe), a
README.md for that backend, LICENSE and THIRD-PARTY-NOTICES.md. Libraries an
Ubuntu package provides stay system dependencies and the README names them.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import platforms  # noqa: E402  (also fetches Native)

ROOT = platforms.ROOT
DIST = ROOT / 'dist'

# Runs inside a Linux SDK image. Prints "BUNDLE <soname> <file>" for every
# library in the closure that no dpkg package owns, then "PACKAGE <name>" for
# the packages behind the libraries the executable or a bundled library
# names directly.
LINUX_PROBE = r'''
set -eu
bin=$1
map=$(mktemp)
ldd "$bin" | awk '/=>/ && $3 ~ /^\// {print $1, $3}' > "$map"
owner() { dpkg -S "$1" 2>/dev/null || dpkg -S "$(readlink -f "$1")" 2>/dev/null || true; }
direct="$bin"
while read -r so path; do
    if test -z "$(owner "$path")"; then
        echo "BUNDLE $so $(readlink -f "$path")"
        direct="$direct $path"
    fi
done < "$map"
for object in $direct; do
    objdump -p "$object" | awk '/NEEDED/ {print $2}' | while read -r so; do
        path=$(awk -v so="$so" '$1 == so {print $2}' "$map")
        test -n "$path" || continue
        owner "$path" | head -n 1 | cut -d: -f1 | sed 's/^/PACKAGE /'
    done
done
grep PRETTY_NAME /etc/os-release | cut -d= -f2 | tr -d '"' | sed 's/^/OS /'
'''

# Runs inside the MinGW SDK image: prints the DLLs to ship beside the .exe.
WINDOWS_PROBE = r'''
set -eu
queue=$1
seen=
while test -n "$queue"; do
    set -- $queue; object=$1; shift; queue="$*"
    for dll in $(x86_64-w64-mingw32-objdump -p "$object" | awk '/DLL Name:/ {print $3}'); do
        path=$(x86_64-w64-mingw32-g++ -print-file-name="$dll")
        case "$path" in /*) ;; *) continue ;; esac
        case " $seen " in *" $path "*) continue ;; esac
        seen="$seen $path"; queue="$queue $path"
        echo "BUNDLE $dll $(readlink -f "$path")"
    done
done
for path in /opt/native-winui/runtime/*.dll; do
    echo "BUNDLE $(basename "$path") $path"
done
'''

# Packages that every glibc system has; leaving them out keeps the apt line short.
BASE_PACKAGES = {'libc6', 'libgcc-s1', 'libstdc++6'}


def docker(image, script, *args, mounts=()):
    command = ['docker', 'run', '--rm', '-u', f'{os.getuid()}:{os.getgid()}',
               '-v', f'{ROOT}:{ROOT}:ro']
    for mount in mounts:
        command += ['-v', mount]
    command += [image, 'sh', '-c', script, 'probe', *args]
    return subprocess.run(command, check=True, capture_output=True,
                          text=True).stdout.splitlines()


def copy_out(image, source, destination):
    """Copy a file that exists only inside the SDK image."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    mount = f'{destination.parent}:/out'
    docker(image, 'cp "$1" "/out/$2"', source, destination.name, mounts=[mount])


def stage_linux(spec, executable, stage):
    lines = docker(spec['image'], LINUX_PROBE, str(executable))
    bundled, packages, system = [], [], ''
    for line in lines:
        kind, _, rest = line.partition(' ')
        if kind == 'BUNDLE':
            soname, source = rest.split(' ', 1)
            target = stage / 'lib' / soname
            copy_out(spec['image'], source, target)
            target.chmod(0o755)
            subprocess.run(['patchelf', '--set-rpath', '$ORIGIN', str(target)],
                           check=True)
            bundled.append(f'lib/{soname}')
        elif kind == 'PACKAGE' and rest not in BASE_PACKAGES:
            packages.append(rest.split(':')[0])
        elif kind == 'OS':
            system = rest
    if bundled:
        subprocess.run(['patchelf', '--set-rpath', '$ORIGIN/lib',
                        str(stage / executable.name)], check=True)
    return sorted(set(bundled)), sorted(set(packages)), system


def stage_windows(spec, executable, stage):
    bundled = []
    for line in docker(spec['image'], WINDOWS_PROBE, str(executable)):
        _, dll, source = line.split(' ', 2)
        copy_out(spec['image'], source, stage / dll)
        bundled.append(dll)
    docker(spec['image'], 'cp -r /opt/native-winui/licenses /out/',
           mounts=[f'{stage}:/out'])
    return sorted(bundled), [], ''


def stage_macos(spec, bundle, stage):
    """Copy the .app, refuse non-system dylibs, and sign it ad hoc."""
    app = stage / spec['bundle']
    subprocess.run(['ditto', str(bundle), str(app)], check=True)
    binary = app / 'Contents/MacOS/isaac'
    # A universal binary lists "<file> (architecture X):" headers, one per
    # slice; only the tab-indented lines under them are libraries.
    links = subprocess.run(['otool', '-L', '-arch', 'all', str(binary)],
                           check=True, capture_output=True,
                           text=True).stdout.splitlines()
    libraries = {line.split()[0] for line in links if line.startswith('\t')}
    foreign = sorted(path for path in libraries
                     if not path.startswith(('/System/', '/usr/lib/')))
    if foreign:
        sys.exit(f'macos: non-system libraries must be embedded: {foreign}')
    archs = subprocess.run(['lipo', '-archs', str(binary)], check=True,
                           capture_output=True, text=True).stdout.split()
    print(f'macos: {spec["bundle"]} architectures: {" ".join(archs)}')
    # Apple silicon refuses unsigned code; an ad hoc signature is the minimum.
    # A Developer ID signature and notarization would replace it.
    subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)],
                   check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)],
                   check=True)
    return [], [], ''


def readme(name, spec, version, executable, bundled, packages, system):
    info = spec['package']
    lines = [f'# Isaac {version} for {info["title"]}', '',
             'Isaac is a spreadsheet rebuilt from',
             '[Gnumeric](https://gitlab.gnome.org/GNOME/gnumeric) on the',
             '[Native](https://github.com/wischner/native) UI library. It reads',
             "and writes Gnumeric's `.gnumeric` files. This is an early port:",
             'charts, conditional formats and printing are not supported yet.', '',
             '## Requirements', '']
    lines += [f'- {item}' for item in info['requires']]
    if packages:
        built = f' (built on {system})' if system else ''
        lines += ['', f'On Debian or Ubuntu{built}, install the libraries with:',
                  '', '```sh', 'sudo apt install ' + ' '.join(packages), '```']
    lines += ['', '## Run', '', *info['run'], '']
    lines += ['## Contents', '', f'- `{executable}`: the application']
    lines += [f'- `{item}`: shipped with Isaac, not a system package'
              for item in bundled]
    lines += ['- `ISAAC.PNG`: 32x32 desktop icon']
    lines += ['- `LICENSE`, `THIRD-PARTY-NOTICES.md`: license terms']
    if name == 'windows':
        lines += ['- `licenses/`: Windows App SDK license terms']
    lines += ['', 'Isaac is free software under the GNU General Public License,',
              'version 2 or later. Source: https://github.com/wischner/native-gnumeric', '']
    return '\n'.join(lines)


def main():
    arguments = sys.argv[1:]
    if not arguments or arguments[0] not in platforms.PLATFORMS:
        sys.exit(f'usage: package.py BACKEND [--version X.Y.Z] [--skip-build]\n'
                 f'backends: {" ".join(platforms.PLATFORMS)}')
    name = arguments[0]
    version = os.environ.get('ISAAC_VERSION', '0.1.0')
    if '--version' in arguments:
        version = arguments[arguments.index('--version') + 1]
    os.environ['ISAAC_VERSION'] = version
    spec = platforms.PLATFORMS[name]
    if '--skip-build' not in arguments:
        outcome = platforms.run_one(name, 'release', True)
        if outcome != 'passed':
            sys.exit(f'{name}: {outcome}')
    else:
        platforms.ensure_image(spec)

    binaries = platforms.build_dir(name, 'release') / 'bin'
    executable = binaries / ('isaac' + spec.get('suffix', ''))
    package = f'isaac-{version}-{name}'
    stage = DIST / package
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)

    if name == 'macos':
        bundled, packages, system = stage_macos(
            spec, binaries / 'isaac.app', stage)
        executable = Path(spec['bundle'])
    else:
        shutil.copy2(executable, stage / executable.name)
        if name == 'windows':
            bundled, packages, system = stage_windows(spec, executable, stage)
        elif name == 'haiku':
            bundled, packages, system = [], [], ''
        else:
            bundled, packages, system = stage_linux(spec, executable, stage)

    shutil.copy2(binaries / 'ISAAC.PNG', stage / 'ISAAC.PNG')
    for document in ('LICENSE', 'THIRD-PARTY-NOTICES.md'):
        shutil.copy2(ROOT / document, stage / document)
    (stage / 'README.md').write_text(
        readme(name, spec, version, executable.name, bundled, packages, system))

    archive = DIST / f'{package}.zip'
    archive.unlink(missing_ok=True)
    if name == 'macos':
        # ditto keeps the bundle's signature, symlinks and metadata intact.
        subprocess.run(['ditto', '-c', '-k', '--keepParent', package,
                        archive.name], cwd=DIST, check=True)
    else:
        # zip records Unix permissions, so isaac stays executable after unzip.
        subprocess.run(['zip', '-qr', '-y', archive.name, package], cwd=DIST,
                       check=True)
    print(archive)


if __name__ == '__main__':
    main()
