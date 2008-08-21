from distutils.core import setup, Extension

pyjscore = Extension(
    "jscore", ["src/jscore.c", "src/conversions.c", "src/jsobj.c"],
    depends=['src/conversions.h', 'src/jscore.h', 'src/jsobj.h'],
    # define_macros=[('TRACE_MALLOC', None)],
    # extra_compile_args=['-O0'],
    extra_link_args=['-framework', 'JavaScriptCore'],
)

setup(
    name="pyjscore",
    version="1.0",
    ext_modules=[pyjscore]
)
