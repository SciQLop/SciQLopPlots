qttest = dependency('qt6', modules: ['Core', 'Test'])

dsp_bench_includes = include_directories('../../include')

bench_segments_moc = qtmod.compile_moc(
    sources: 'bench_segments.cpp',
    dependencies: [qttest],
    include_directories: dsp_bench_includes,
)

bench_segments = executable('bench_segments',
    'bench_segments.cpp', bench_segments_moc,
    include_directories: dsp_bench_includes,
    dependencies: [qttest, dsp_dep],
)

benchmark('bench_segments', bench_segments)
