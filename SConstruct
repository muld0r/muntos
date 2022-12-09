import os
import sys

llvm_flags = [
    "-g3",
    "-Og",
    "-ffunction-sections",
    "-fdata-sections",
    "-fsanitize=address,undefined",
    "-flto",
]

env = Environment(
    CPPPATH=[Dir("include").srcnode()],
    CCFLAGS=llvm_flags,
    CFLAGS=["-std=c17"],
    LINKFLAGS=llvm_flags,
)

# for color terminal output when available
if "TERM" in os.environ:
    env["ENV"]["TERM"] = os.environ["TERM"]

env["CC"] = "clang"
env.Append(
    CCFLAGS=[
        "-Weverything",
        "-Werror",
        "-Wno-padded",
        "-Wno-poison-system-directories",
        "-Wno-declaration-after-statement",
        "-Wno-disabled-macro-expansion",
        "-Wno-bad-function-cast",
        "-Wno-gcc-compat",
        "-Wno-missing-noreturn",
    ],
)

if "darwin" in sys.platform:
    env.Append(
        LINKFLAGS=["-dead_strip"],
    )
elif "linux" in sys.platform:
    env["RANLIB"] = "llvm-ranlib"
    env["AR"] = "llvm-ar"
    env["LINKCOM"] = env["LINKCOM"].replace(
        "$_LIBFLAGS", "-Wl,--start-group $_LIBFLAGS -Wl,--end-group"
    )
    env.Append(
        CPPDEFINES={"_POSIX_C_SOURCE": "200809L"}, LINKFLAGS=["-Wl,--gc-sections"]
    )

librt = SConscript(
    dirs="src", variant_dir="build/lib", duplicate=False, exports=["env"]
)

pthread_env = env.Clone()
pthread_env.Append(
    CCFLAGS="-pthread",
    LINKFLAGS="-pthread",
)

libpthread = SConscript(
    "arch/pthread/SConscript",
    variant_dir="build/lib/pthread",
    duplicate=False,
    exports={"env": pthread_env},
)

example_env = pthread_env.Clone()
example_env.Append(
    CPPDEFINES={"STACK_ALIGN": 4096, "TASK_STACK_SIZE": 32768},
    LIBS=[librt, libpthread],
)

SConscript(
    dirs="examples", variant_dir="build", duplicate=False, exports={"env": example_env}
)
