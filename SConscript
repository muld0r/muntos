import os

env = Environment(
    CC='clang',
    CCFLAGS=[
        '-Os',
        '-Weverything', '-Werror', '-Wno-padded',
        '-pthread',
        '-ffunction-sections',
        '-fdata-sections',
    ],
    CFLAGS=['-std=c99'],
    CPPPATH=['include', 'pthread'],
)

# for color terminal output when available
if 'TERM' in os.environ:
    env['ENV']['TERM'] = os.environ['TERM']

if env['PLATFORM'] in 'darwin':
    env['CC'] = 'clang'
    env.Append(LINKFLAGS='-dead_strip')
elif env['PLATFORM'] in 'linux':
    env['CC'] = 'gcc'
    env.Append(LINKFLAGS='--gc-sections')

pthread_env = env.Clone()
pthread_env.Append(
    CPPPATH='pthread',
    CCFLAGS='-pthread',
    LINKFLAGS='-pthread',
)

pthread_rt = pthread_env.Object(target='pthread_rt', source='rt.c')
pthread_simple = pthread_env.Object(target='pthread_simple', source='examples/simple.c')
pthread_env.Program(
    [pthread_simple, pthread_rt, 'pthread/rt_port.c'],
)
