import os
import sys

llvm_flags = [
        "-g",
        "-Os",
        "-ffunction-sections",
        "-fdata-sections",
        #"-flto",
    ]

env = Environment(
    CPPPATH=["include"],
    CCFLAGS=llvm_flags,
    CFLAGS=["-std=c99"],
    LINKFLAGS=llvm_flags,
)

# for color terminal output when available
if "TERM" in os.environ:
    env["ENV"]["TERM"] = os.environ["TERM"]

if "darwin" in sys.platform:
    env["CC"] = "clang"
    env.Append(
        CCFLAGS=[
            "-Weverything",
            "-Werror",
            "-Wno-padded",
            "-Wno-poison-system-directories",
        ],
        LINKFLAGS=["-dead_strip"],
    )
elif "linux" in sys.platform:
    env["CC"] = "gcc"
    env.Append(CPPDEFINES=[("_POSIX_C_SOURCE", "200809L")])
    env.Append(LINKFLAGS=["-Wl,--gc-sections"])

librt = env.StaticLibrary(
    target="rt",
    source=[
        "src/rt.c",
        "src/tick.c",
        "src/critical.c",
        "src/delay.c",
        "src/queue.c",
        "src/sem.c",
    ],
)

posix_env = env.Clone()
posix_env.Append(
    CCFLAGS="-pthread",
    LINKFLAGS="-pthread",
)

posix_port = posix_env.Object("src/port/posix.c")

posix_simple = posix_env.Object(target="posix_simple", source="examples/simple.c")
posix_delay = posix_env.Object(target="posix_delay", source="examples/delay.c")
posix_queue = posix_env.Object(target="posix_queue", source="examples/queue.c")
posix_env.Program([posix_simple, librt, posix_port])
posix_env.Program([posix_delay, librt, posix_port])
posix_env.Program([posix_queue, librt, posix_port])
