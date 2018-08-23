import os

env = Environment(
    CC='clang',
    CCFLAGS=[
        '-Os',
        '-std=c99',
        '-Weverything', '-Werror', '-Wno-padded',
        '-pthread',
        '-ffunction-sections',
        '-fdata-sections',
    ],
    CPPPATH=['include', 'pthread'],
    LINKFLAGS=['-pthread'],
)

if env['PLATFORM'] in 'darwin':
    env['CC'] = 'clang'
    env.Append(LINKFLAGS='-dead_strip')
elif env['PLATFORM'] in 'linux':
    env['CC'] = 'gcc'
    env.Append(LINKFLAGS='--gc-sections')


librt = env.StaticLibrary(['rt.c'])
libcontext_pthread = env.StaticLibrary(['pthread/context_port.c'])

env.Program(['examples/simple.c', librt, libcontext_pthread])

# for color terminal output when available
if 'TERM' in os.environ:
    env['ENV']['TERM'] = os.environ['TERM']
