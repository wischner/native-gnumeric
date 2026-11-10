#!/usr/bin/env python3
"""Build isaac and run its desktop test in each backend's Native SDK.

usage: platforms.py [all|BACKEND] [--mode release|debug] [--build-only]
       platforms.py --list
"""
from pathlib import Path
import json
import os
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
NATIVE = Path(os.environ.get('NATIVE_ROOT') or subprocess.run(
    ['sh', str(ROOT / 'scripts/build/fetch_native.sh')], check=True,
    stdout=subprocess.PIPE, text=True).stdout.strip()).resolve()
PLATFORMS = json.loads((ROOT / 'scripts/build/platforms.json').read_text())
TIMEOUT = 1800
BUILD_TYPES = {'release': 'Release', 'debug': 'Debug'}


def build_dir(name, mode):
    """Return where a backend builds; launch.json uses the same path."""
    return ROOT / 'build/platforms' / name / mode


def ensure_image(spec):
    """Build an SDK image that is not on Docker Hub from Native's Dockerfile."""
    if 'dockerfile' not in spec or 'image' not in spec:
        return True
    image = spec['image']
    if subprocess.run(['docker', 'image', 'inspect', image],
                      capture_output=True).returncode == 0:
        return True
    context = spec['dockerfile'].format(native=NATIVE)
    return subprocess.run(['docker', 'build', '-t', image, context]
                          ).returncode == 0


def run_one(name, mode, build_only, resources_only=False):
    """Return 'passed', or the failing stage."""
    spec = PLATFORMS[name]
    # Host backends (macOS) build with the machine's own toolchain.
    if 'host' in spec and platform.system() != spec['host']:
        return f'skipped (needs {spec["host"]})'
    if not ensure_image(spec):
        return 'image build failed'
    build = build_dir(name, mode)
    resource_args = []
    if any('CMAKE_TOOLCHAIN_FILE' in part for part in spec['cmake']):
        outcome = run_one('linux-sdl2', 'release', True, resources_only=True)
        if outcome != 'passed':
            return f'host resources: {outcome}'
        resource_cpp = build_dir('linux-sdl2', 'release') / 'resources/isaac.cpp'
        resource_args = [f'-DISAAC_RESOURCE_CPP={resource_cpp}']
    if os.environ.get('ISAAC_VERSION'):
        resource_args.append(f'-DISAAC_VERSION={os.environ["ISAAC_VERSION"]}')
    docker = [] if 'host' in spec else [
        'docker', 'run', '--rm', '--init', '-u',
        f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp',
        '-e', 'ASAN_OPTIONS=detect_leaks=0', '-v', f'{ROOT}:{ROOT}',
        '-v', f'{NATIVE}:{NATIVE}', '-w', str(ROOT), spec['image']]
    stages = {
        'configure': ['cmake', '-S', '.', '-B', str(build),
                      f'-DNATIVE_ROOT={NATIVE}',
                      f'-DCMAKE_BUILD_TYPE={BUILD_TYPES[mode]}',
                      *resource_args,
                      *[part.format(native=NATIVE) for part in spec['cmake']]],
        'build': ['cmake', '--build', str(build), f'-j{os.cpu_count() or 8}',
                  '--target',
                  *(['isaac_resources'] if resources_only else
                    ['isaac', 'test_desktop'])],
    }
    # Cross builds have no 'run': their test runs on the target machine.
    if not build_only and 'run' in spec:
        run = [part.format(native=NATIVE) for part in spec['run']]
        stages['test'] = run + [str(build / 'bin/test_desktop')]
    build.mkdir(parents=True, exist_ok=True)
    for stage, command in stages.items():
        with (build / f'{stage}.log').open('w') as log:
            result = subprocess.run(docker + command, stdout=log,
                                    stderr=subprocess.STDOUT,
                                    timeout=TIMEOUT)
        if result.returncode != 0:
            print((build / f'{stage}.log').read_text()[-4000:],
                  file=sys.stderr, flush=True)
            return f'{stage} failed ({result.returncode})'
    return 'passed'


def main():
    arguments = sys.argv[1:]
    if '--list' in arguments:
        print('\n'.join(PLATFORMS))
        return 0
    build_only = '--build-only' in arguments
    mode = 'release'
    if '--mode' in arguments:
        mode = arguments[arguments.index('--mode') + 1]
    wanted = arguments[0] if arguments and arguments[0] != '--mode' else 'all'
    if wanted != 'all' and wanted not in PLATFORMS:
        sys.exit(f'unknown backend {wanted}')
    names = list(PLATFORMS) if wanted == 'all' else [wanted]
    failed = False
    for name in names:
        outcome = run_one(name, mode, build_only)
        print(f'{name}: {outcome}', flush=True)
        failed |= outcome != 'passed' and not outcome.startswith('skipped')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
