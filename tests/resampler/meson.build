qttest = dependency('qt6', modules: ['Core', 'Test'])

# Unlike the extension module, a standalone executable has to link libpython.
python3_embed_dep = python3.dependency(embed: true)

resampler_test_includes = include_directories('../../include')

test_resampler_moc = qtmod.compile_moc(
    sources: 'test_resampler_locking.cpp',
    dependencies: [qttest, qtdeps, python3_dep, NeoQCP_dep],
    include_directories: resampler_test_includes,
)

# PythonInterface.cpp backs SciQLopPyBuffer, a member of ResamplerData1d, and
# must stay out of a unity build (slots macro conflicts) like it does for the
# bindings library.
test_resampler_exe = executable('test_resampler_locking',
    'test_resampler_locking.cpp', test_resampler_moc,
    '../../src/PythonInterface.cpp',
    '../../src/Tracing.cpp',
    '../../src/Profiling.cpp',
    include_directories: resampler_test_includes,
    dependencies: [qttest, qtdeps, python3_embed_dep, NeoQCP_dep, cpp_utils_dep, fmt_dep,
                   magic_enum_dep],
    override_options: ['unity=off'],
)

test('resampler_locking', test_resampler_exe, suite: 'unit')
