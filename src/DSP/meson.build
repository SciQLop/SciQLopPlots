dsp_includes = include_directories('../../include')

xsimd_dep = dependency('xsimd', fallback: ['xsimd', 'xsimd_dep'])
pocketfft_dep = dependency('pocketfft', fallback: ['pocketfft', 'pocketfft_dep'])
thread_pool_dep = dependency('bshoshany-thread-pool',
    fallback: ['thread-pool', 'bshoshany_thread_pool_dep'])

subdir('simd')

dsp_dep = declare_dependency(
    sources: files('dsp_compile_test.cpp'),
    include_directories: dsp_includes,
    dependencies: [xsimd_dep, pocketfft_dep, thread_pool_dep] + dsp_simd_deps,
)
