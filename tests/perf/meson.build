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

bench_search_moc = qtmod.compile_moc(
    sources: 'bench_search.cpp',
    dependencies: [qttest],
    include_directories: dsp_bench_includes,
)

bench_search = executable('bench_search',
    'bench_search.cpp', bench_search_moc,
    include_directories: dsp_bench_includes,
    dependencies: [qttest],
)

benchmark('bench_search', bench_search)
