qttest = dependency('qt6', modules: ['Core', 'Test'])

tracing_test_includes = include_directories('../../include')

test_tracing_moc = qtmod.compile_moc(
    sources: 'test_tracing.cpp',
    dependencies: [qttest],
    include_directories: tracing_test_includes,
)

test_tracing_exe = executable('test_tracing',
    'test_tracing.cpp', test_tracing_moc,
    '../../src/Tracing.cpp',
    include_directories: tracing_test_includes,
    dependencies: [qttest],
)

test('tracing_engine', test_tracing_exe, suite: 'unit')
