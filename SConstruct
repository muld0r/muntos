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


def ld_emitter(target, source, env):
    map_file = os.path.splitext(str(target[0]))[0] + ".map"
    return target + [map_file], source


env = Environment(
    CPPPATH=[Dir("include").srcnode()],
    CCFLAGS=llvm_flags,
    CFLAGS=["-std=c17"],
    LINKFLAGS=llvm_flags,
    PROGEMITTER=ld_emitter,
)

if sys.platform == "darwin":
    env["LINKCOM"] = (
        "$LINK -o ${TARGETS[0]} -Wl,-map,${TARGETS[1]}"
        + " $LINKFLAGS $SOURCES $_LIBDIRFLAGS $_LIBFLAGS"
    )
else:
    env["LINKCOM"] = (
        "$LINK -o ${TARGETS[0]} -Wl,-Map,${TARGETS[1]}"
        + " $LINKFLAGS $SOURCES $_LIBDIRFLAGS"
        + " -Wl,--start-group $_LIBFLAGS -Wl,--end-group"
    )

# for color terminal output when available
if "TERM" in os.environ:
    env["ENV"]["TERM"] = os.environ["TERM"]

env["CC"] = "clang"
env.Append(
    CCFLAGS=[
        "-pthread",
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
    LINKFLAGS="-pthread",
    CPPPATH=[Dir("arch/pthread/include").srcnode()],
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

libpthread = SConscript(
    "arch/pthread/SConscript",
    variant_dir="build/lib/pthread",
    duplicate=False,
    exports={"env": env},
)

example_env = env.Clone()
example_env.Append(
    LIBS=[librt, libpthread],
)

SConscript(
    dirs="examples", variant_dir="build", duplicate=False, exports={"env": example_env}
)
