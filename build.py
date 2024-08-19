import argparse
import subprocess
import shlex
import shutil
import json
from pathlib import Path

CFLAGS_WARNINGS = [
    '-Wall', '-Werror-implicit-function-declaration', '-Wno-logical-op-parentheses',
    '-Wno-missing-braces', '-Wsizeof-array-decay', '-Wno-assume', '-Wno-unused-command-line-argument',
    '-Wno-int-to-void-pointer-cast', '-Wno-void-pointer-to-int-cast', '-Wno-unsafe-buffer-usage',
    '-Wno-static-in-inline', '-Wsign-conversion',
]
CFLAGS_DEBUG = ['-DCONFIG_DEBUG']
CFLAGS_OPT   = ['-fno-math-errno', '-fno-strict-aliasing']

CC = 'clang'
CXX = 'clang++'

ANDROID_SDK = 'C:/b/programs/android/sdk'
ANDROID_NDK = 'C:/b/programs/android/ndk-r23c'
ANDROID_JAVA_JDK = 'C:/b/programs/jdk-17.0.7+7'
ANDROID_BUILD_TOOLS = 'C:/b/programs/android/sdk/build-tools/33.0.1'
ANDROID_PLATFORM = 'C:/b/programs/android/sdk/platforms/android-26'

commands_ran = []

def make_outpath(input, outdir):
    filename = input.split('/')[-1]
    return outdir + '/' + filename

def remove_ext_if(path, ext):
    if path.endswith(ext):
        path = path[:-len(ext)]
    return path

def is_input_older_than_output(input, output):
    file1 = Path(input)
    file2 = Path(output)
    if not file1.is_file() or not file2.is_file():
        return False
    stat1 = file1.stat()
    stat2 = file2.stat()
    if stat1.st_mtime < stat2.st_mtime:
        return True
    return False

def is_all_deps_older_than_output(output, depfile):
    depfile_path = Path(depfile)
    output_path = Path(depfile)
    result = False
    
    if not depfile_path.is_file() or not output_path.is_file():
        return False
    
    output_stat = output_path.stat()

    with open(depfile, encoding='utf-8') as file:
        data = file.read()
        if data.startswith(output):
            data = data[len(output):]
        if data[0] != ':':
            # TODO: handle MSVC-style /showIncludes
            return False
        else:
            i = 1
            while True:
                while i < len(data) and data[i] in [' ', '\t', '\n', '\r']:
                    i += 1
                if i+1 < len(data) and data[i] == '\\' and data[i+1] == '\n':
                    i += 2
                    continue
                if i >= len(data):
                    break
                
                depname_start = i
                while i < len(data) and data[i] not in [' ', '\n', '\r']:
                    if data[i] == '\\':
                        if i+1 < len(data) and data[i+1] in ['\r', '\n']:
                            break
                        i += 2
                    else:
                        i += 1
                depname_end = i

                depname = data[depname_start:depname_end].replace('\\ ', ' ')
                depname = Path(depname)
                if not depname.is_file():
                    return False
                depname_stat = depname.stat()
                if output_stat.st_mtime < depname_stat.st_mtime:
                    return False

    return True

def run_and_log(argv):
    global commands_ran
    commands_ran.append(argv)
    print('[CMD] ' + ' '.join([arg if ' ' not in arg else '"'+arg+'"' for arg in argv]))
    return subprocess.run(argv, shell=True)

def build_type_has_depfile(type):
    return type in ['c', 'cxx']

def write_file_if_changed(path, contents):
    if read_entire_file(path) != contents:
        with open(path, 'w') as f:
            f.write(contents)

def read_entire_file(path):
    path = Path(path)
    if not path.is_file():
        return None
    with open(path, 'r') as f:
        result = f.read()
    return result

project = None
is_debug = False
force_rebuild = False
target = None
outdir = None
middir = None
output_name = None
windows_subsystem = "console"

def main():
    global project, is_debug, force_rebuild, target, outdir, middir, output_name, windows_subsystem
    
    ap = argparse.ArgumentParser()
    ap.add_argument('project')
    ap.add_argument('-debug', action='store_true')
    ap.add_argument('-rebuild', action='store_true')
    ap.add_argument('-target', choices=['win64', 'android'], default='win64')
    ap.add_argument('-no-compile-commands', action='store_true')
    ap.add_argument('-asan', action='store_true')
    ap.add_argument('-ubsan', action='store_true')
    args = ap.parse_args()

    project = args.project
    is_debug = args.debug
    force_rebuild = args.rebuild
    target = args.target
    gen_compile_commands = not args.no_compile_commands
    asan = args.asan
    ubsan = args.ubsan

    with open(f'{project}/build.txt', 'r') as file:
        lines = file.read().splitlines()
    
    outdir = 'build'
    middir = f'build/{project}-{target}'
    if is_debug:
        middir += '-debug'
    
    Path(middir).mkdir(parents=True, exist_ok=True)

    objs = []
    shaders = []
    cmds = []
    libs = []
    defines = []
    incdirs = []
    libdirs = []
    output_name = None
    allowed_tags = [target]
    odin = None
    is_root = False
    original_output_name = None
    for line in lines:
        tokens = shlex.split(line.strip())
        should_skip_line = False
        for tok in tokens:
            tok = tok.lower()
            if tok.startswith('@') and tok[1:] not in allowed_tags:
                should_skip_line = True
        if should_skip_line:
            continue

        tokens = [x for x in tokens if x != "" and not x.startswith('@')]
        match tokens:
            case ['#', *_]:
                pass
            case ['c', input]:
                objs.append({
                    'type': "c",
                    'source': input,
                    'output': make_outpath(input+'.obj', middir)
                })
            case ['cxx', input]:
                objs.append({
                    'type': "cxx",
                    'source': input,
                    'output': make_outpath(input+'.obj', middir)
                })
            case \
                ['fxc', input, profile, entry, *extra] | \
                ['dxc', input, profile, entry, *extra] | \
                ['hlsl2glsl', input, profile, entry, *extra] | \
                ['dxc-spirv', input, profile, entry, *extra]:
                output = remove_ext_if(input, '.hlsl')
                suffix = profile.split('_')[0]
                output += f'_{suffix}'
                if tokens[0] == 'fxc' or tokens[0] == 'dxc':
                    output += '.dxil'
                elif tokens[0] == 'dxc-spirv':
                    output += '.spirv'
                else:
                    output += '.glsl'
                output = make_outpath(output, middir)
                shaders.append({
                    'type': tokens[0],
                    'source': input,
                    'profile': profile,
                    'entry': entry,
                    'output': output,
                    'extra': extra,
                })
            case ['gamepad_db_gen', input]:
                output = remove_ext_if(input, '.txt')
                output += '.inc'
                output = make_outpath(output, middir)
                cmds.append({
                    'type': 'gamepad_db_gen',
                    'source': input,
                    'output': output
                })
            case ['rc', input]:
                output = remove_ext_if(input, '.rc')
                output += '.res'
                output = make_outpath(output, middir)
                objs.append({
                    'type': "rc",
                    'source': input,
                    'output': output
                })
            case ['output', name]:
                original_output_name = name
                output_name = make_outpath(name + ('-debug' if is_debug else ''), outdir)
            case ['lib', name]:
                libs.append(name)
            case ['define', name]:
                defines.append(name)
            case ['incdir', path]:
                incdirs.append(path)
            case ['libdir', path]:
                libdirs.append(path)
            case ['odin', *flags]:
                odin = {
                    'input': f'{project}',
                    'output': make_outpath(project+'.obj', middir),
                    'flags': flags,
                }
                objs.append({
                    'type': 'obj',
                    'output': odin['output'],
                })
            case ['subsystem', subsystem]:
                windows_subsystem = subsystem
            case ['root']:
                is_root = True
    
    retcode = build_cmds(cmds)
    match target:
        case 'win64':
            if retcode == 0: retcode = build_shaders(shaders)
            if retcode == 0: retcode = build_odin(odin)
            if retcode == 0: retcode = build_win64(objs, libs, defines, incdirs, libdirs, asan, ubsan)
        case 'android':
            if retcode == 0: retcode = build_shaders(shaders)
            if retcode == 0: retcode = build_odin(odin)
            if retcode == 0: retcode = build_android(objs, libs, defines, incdirs, libdirs)
        case _:
            retcode = 1

    if gen_compile_commands:
        commands_file = Path('compile_commands.json')
        output_json = read_entire_file(commands_file)
        if output_json:
            output_json = json.loads(output_json)
        else:
            output_json = []
            
        for command in commands_ran:
            if (command[0] == CC or command[0] == CXX) and command[1] == '-c':
                output_json.append({
                    'directory': str(Path.cwd()),
                    'arguments': command,
                    'file': command[4],
                    'output': command[3],
                })

        output_json = list(set(json.dumps(d) for d in output_json))
        write_file_if_changed(commands_file, f'[{",".join(output_json)}]')

    if retcode == 0 and is_root:
        if target == 'win64':
            output_name += '.exe'
            original_output_name += '.exe'
        shutil.copy(output_name, f'./{original_output_name}')

    return retcode

def build_shaders(shaders):
    global force_rebuild
    for shader in shaders:
        if not force_rebuild and is_input_older_than_output(shader['source'], shader['output']):
            continue
        extra = []
        if 'extra' in shader:
            extra = shader['extra']
        match shader['type']:
            case 'fxc' | 'dxc':
                p = run_and_log([shader['type'], '/nologo', '/O3', shader['source'], f'/Fo{shader["output"]}', f'/T{shader["profile"]}', f'/E{shader["entry"]}', *extra])
            case 'dxc-spirv':
                args = ['dxc', '/nologo', '/O3', shader['source'], f'/Fo{shader["output"]}', f'/T{shader["profile"]}', f'/E{shader["entry"]}', '-spirv', *extra]
                # if shader['profile'].startswith('vs_'):
                #     args.append('-fvk-invert-y')
                p = run_and_log(args)
            case 'hlsl2glsl':
                p = run_and_log(['hlsl2glsl', shader['source'], shader['output'], shader['profile'], shader['entry'], *extra])
        if p.returncode != 0:
            return 1
    return 0

def build_cmds(cmds):
    global force_rebuild
    for cmd in cmds:
        if not force_rebuild and is_input_older_than_output(cmd['source'], cmd['output']):
            continue
        match cmd['type']:
            case 'gamepad_db_gen':
                p = run_and_log(['gamepad_db_gen', cmd['source'], cmd['output']])
        if p.returncode != 0:
            return 1
    return 0

def build_win64(objs, libs, defines, incdirs, libdirs, asan, ubsan):
    global project, is_debug, force_rebuild, target, outdir, middir, output_name

    cflags_platform = ['-mlzcnt', '-march=x86-64-v2']
    cflags_debug    = [*CFLAGS_DEBUG, '-g']
    cflags_opt      = [*CFLAGS_OPT,   '-O2', '-flto']
    linkflags_debug = ['-g']

    cflags    = ['-Icommon', '-I.', '-Ithird_party_include', '-I'+middir, '-std=c2x',   *CFLAGS_WARNINGS, *cflags_platform]
    cxxflags  = ['-Icommon', '-I.', '-Ithird_party_include', '-I'+middir, '-std=c++14', *CFLAGS_WARNINGS, *cflags_platform]
    linkflags = ['-fuse-ld=lld', f'-Wl,/incremental:no,/opt:ref,/subsystem:{windows_subsystem}', '-Lthird_party_lib/x86_64-windows', *[f'-l{x}' for x in libs]]
    if is_debug:
        cflags.extend(cflags_debug)
        cxxflags.extend(cflags_debug)
        linkflags.extend(linkflags_debug)
    else:
        cflags.extend(cflags_opt)
        cxxflags.extend(cflags_opt)
        linkflags.extend(cflags_opt)
    cflags.extend([f'-D{x}' for x in defines])
    cflags.extend([f'-I{x}' for x in incdirs])
    cxxflags.extend([f'-D{x}' for x in defines])
    cxxflags.extend([f'-I{x}' for x in incdirs])
    linkflags.extend([f'-L{x}' for x in libdirs])
    if asan:
        cflags.append('-fsanitize=address')
        cxxflags.append('-fsanitize=address')
        linkflags.append('-fsanitize=address')
    if ubsan:
        cflags.append('-fsanitize=undefined')
        cxxflags.append('-fsanitize=undefined')
        linkflags.append('-fsanitize=undefined')

    for obj in objs:
        type = obj['type']
        if not force_rebuild and 'source' in obj:
            if is_input_older_than_output(obj['source'], obj['output']) and \
                (not build_type_has_depfile(type) or is_all_deps_older_than_output(obj['output'], obj['output']+'.d')):
                continue
        match type:
            case 'c':
                p = run_and_log([CC, '-c', '-o', obj['output'], obj['source'], '-MMD', '-MF', obj['output']+'.d', *cflags])
            case 'cxx':
                p = run_and_log([CXX, '-c', '-o', obj['output'], obj['source'], '-MMD', '-MF', obj['output']+'.d', *cxxflags])
            case 'rc':
                p = run_and_log(['llvm-rc', '/FO', obj['output'], obj['source']])
            case _:
                continue
        if p.returncode != 0:
            return 1
    
    p = run_and_log([
        'clang', '-o', output_name + '.exe',
        *(obj['output'] for obj in objs),
        *linkflags,
    ])
    if p.returncode != 0:
        return 1
    
    return 0

def build_android(objs, libs, defines, incdirs, libdirs):
    global project, is_debug, force_rebuild, target, outdir, middir, output_name
    output_name += '.apk'

    Path(f'{middir}/jni').mkdir(parents=True, exist_ok=True)

    sources = [f'../../../{x["source"]}' for x in objs]
    ldlibs = [f'-l{x}' for x in libs]
    cflags = []
    if is_debug:
        cflags.extend(CFLAGS_DEBUG)
        cflags.append('-g')
    cflags.extend([f'-D{x}' for x in defines])

    android_mk  = '# https://developer.android.com/ndk/guides/android_mk.html' + '\n'
    android_mk += '\n'
    android_mk += 'LOCAL_PATH := $(call my-dir)'                               + '\n'
    android_mk += 'include $(CLEAR_VARS)'                                      + '\n'
    android_mk += '\n'
    android_mk += 'LOCAL_MODULE := main'                                       + '\n'
    android_mk += 'LOCAL_STATIC_LIBRARIES := android_native_app_glue'          + '\n'
    android_mk += 'LOCAL_C_INCLUDES := ../../include ../../src . ../../'       + '\n'
    android_mk += 'LOCAL_LDLIBS := '    + ' '.join(ldlibs)                     + '\n'
    android_mk += 'LOCAL_SRC_FILES := ' + ' '.join(sources)                    + '\n'
    android_mk += 'LOCAL_CFLAGS := '    + ' '.join(cflags)                     + '\n'
    if is_debug:
        android_mk += 'LOCAL_STRIP_MODE := none'                               + '\n'
        android_mk += 'LOCAL_STRIP_MODULE := keep_symbols'                     + '\n'
    android_mk += '\n'
    android_mk += 'include $(BUILD_SHARED_LIBRARY)'                            + '\n'
    android_mk += '$(call import-module,android/native_app_glue)'              + '\n'
    
    application_mk  = '# https://developer.android.com/ndk/guides/application_mk.html' + '\n'
    application_mk += '\n'
    application_mk += 'NDK_TOOLCHAIN_VERSION := clang'                                 + '\n'
    application_mk += 'APP_PLATFORM := android-26'                                     + '\n'
    application_mk += 'APP_ABI := arm64-v8a'                                           + '\n'
    if is_debug:
        application_mk += 'APP_OPTIM := debug'     + '\n'
        application_mk += 'APP_STRIP_MODE := none' + '\n'
    else:
        application_mk += 'APP_OPTIM := release'   + '\n'

    android_mk_path = Path(f'{middir}/jni/Android.mk')
    write_file_if_changed(android_mk_path, android_mk)
    application_mk_path = Path(f'{middir}/jni/Application.mk')
    write_file_if_changed(application_mk_path, application_mk)
    
    p = run_and_log(['cd', middir, '&&', ANDROID_NDK+'/ndk-build', '-j8', 'NDK_LIBS_OUT=lib\\lib', 'NDK_PROJECT_PATH=.'])
    if p.returncode != 0:
        return 1
    p = run_and_log(['cd', middir, '&&', ANDROID_BUILD_TOOLS+'/aapt', 'package', '-f', '-M', f'../../src/{project}/AndroidManifest.xml', '-I', ANDROID_PLATFORM+'/android.jar', '-A', f'../../assets/{project}', '-F', f'../../{output_name}', 'lib'])
    if p.returncode != 0:
        return 1
    p = run_and_log(['cd', middir, '&&', ANDROID_JAVA_JDK+'/bin/jarsigner', '-storepass', 'android', '-keystore', f'../../src/{project}/.keystore', f'../../{output_name}.build', 'androiddebugkey', '&&', ANDROID_BUILD_TOOLS+'/zipalign', '-f', '4', f'../../{output_name}.build', f'../../{output_name}'])
    if p.returncode != 0:
        return 1
    
    return 0

def build_odin(odin):
    global project, is_debug, force_rebuild, target, outdir, middir, output_name
    if not odin:
        return 0
    
    target_triple = None
    match target:
        case 'win64':
            target_triple = 'windows_amd64'
        case 'android':
            target_triple = 'linux_arm64'
    if target_triple == None:
        print(f'[ERROR] cannot determine odin target from target platform "{target}"')
        return 1

    package = odin['input']
    output = odin['output']
    flags = [
        '-build-mode:obj',
        '-collection:src=.',
        f'-collection:builddir={middir}',
        '-export-dependencies:make',
        f'-export-dependencies-file:{output}.d',
        f'-target:{target_triple}'
    ]
    flags.extend(odin['flags'])
    if is_debug:
        flags.append('-debug')
        flags.append('-o:none')
    else:
        flags.append('-o:speed')

    p = run_and_log(['odin', 'build', package, f'-out:{output}', *flags])
    if p.returncode != 0:
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())
