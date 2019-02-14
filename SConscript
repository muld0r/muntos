import os
import sys

env = Environment(
    CPPPATH=['include'],
    CCFLAGS=[
        '-g',
        '-Os',
        '-ffunction-sections',
        '-fdata-sections',
#        '-flto',
    ],
    CFLAGS=['-std=c99'],
#    LINKFLAGS=['-flto'],
)

# for color terminal output when available
if 'TERM' in os.environ:
    env['ENV']['TERM'] = os.environ['TERM']

if 'darwin' in sys.platform:
    env['CC'] = 'clang'
    env.Append(
        CCFLAGS=['-Weverything', '-Werror', '-Wno-padded'],
        LINKFLAGS=['-dead_strip'],
    )
elif 'linux' in sys.platform:
    env['CC'] = 'gcc'
    env.Append(CPPDEFINES=[('_POSIX_C_SOURCE', '200809L')])
    env.Append(LINKFLAGS=['-Wl,--gc-sections'])

librt = env.StaticLibrary(target='rt', source=['src/rt.c', 'src/tick.c', 'src/critical.c', 'src/delay.c', 'src/queue.c', 'src/sem.c'])

posix_env = env.Clone()
posix_env.Append(
    CCFLAGS='-pthread',
    LINKFLAGS='-pthread',
)

posix_simple = posix_env.Object(target='posix_simple', source='examples/simple.c')
posix_delay = posix_env.Object(target='posix_delay', source='examples/delay.c')
posix_queue = posix_env.Object(target='posix_queue', source='examples/queue.c')
posix_env.Program([posix_simple, librt, 'arch/posix/posix.c'])
posix_env.Program([posix_delay, librt, 'arch/posix/posix.c'])
posix_env.Program([posix_queue, librt, 'arch/posix/posix.c'])
