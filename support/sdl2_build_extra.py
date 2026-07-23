Import("env", "projenv")

for e in [env, projenv]:
    if "-m32" in e.get("CCFLAGS", []):
        e.Append(LINKFLAGS=["-m32"])

exec_name = "${BUILD_DIR}/${PROGNAME}${PROGSUFFIX}"

from SCons.Script import AlwaysBuild

AlwaysBuild(env.Alias("upload", exec_name, exec_name))

env.AddTarget(
    name="execute",
    dependencies=exec_name,
    actions='"{}"'.format(exec_name),
    title="Execute",
    description="Build and execute the SDL simulator",
    group="General",
)
