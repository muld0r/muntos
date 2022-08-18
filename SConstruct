import os
import sys

llvm_flags = [
    "-g",
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
    ],
)

if "darwin" in sys.platform:
    env.Append(
        LINKFLAGS=["-dead_strip"],
    )
elif "linux" in sys.platform:
    env["RANLIB"] = "llvm-ranlib"
    env["AR"] = "llvm-ar"
    env["LINKCOM"] = env["LINKCOM"].replace("$_LIBFLAGS", "-Wl,--start-group $_LIBFLAGS -Wl,--end-group")
    env.Append(
        CPPDEFINES={"_POSIX_C_SOURCE": "200809L"}, LINKFLAGS=["-Wl,--gc-sections"]
    )

librt = SConscript(
    dirs="src", variant_dir="build/lib", duplicate=False, exports=["env"]
)

posix_env = env.Clone()
posix_env.Append(
    CCFLAGS="-pthread",
    LINKFLAGS="-pthread",
)

libposix = SConscript(
    "arch/posix/SConscript",
    variant_dir="build/lib/posix",
    duplicate=False,
    exports={"env": posix_env},
)

example_env = posix_env.Clone()
example_env.Append(LIBS=[librt, libposix])

SConscript(
    dirs="examples", variant_dir="build", duplicate=False, exports={"env": example_env}
)
