dsp_simd_deps = []
dsp_simd_libs = []

if target_machine.cpu_family() == 'x86_64'
    x86_dsp_libs = []
    x86_dsp_defs = []
    xsimd_arch_list = []

    if cpp.get_id() == 'msvc'
        archs = [
            {
                'name': 'avx512bw',
                'flags': ['/arch:AVX512'],
                'xsimd_name': 'avx512bw',
            },
            {
                'name': 'avx2',
                'flags': ['/arch:AVX2'],
                'xsimd_name': 'avx2',
            },
            {
                'name': 'sse2',
                'flags': ['/arch:SSE2'],
                'xsimd_name': 'sse2',
            },
        ]
    else
        archs = [
            {
                'name': 'avx512bw',
                'flags': ['-mavx512bw', '-mavx512cd', '-mavx512dq', '-mavx512vl'],
                'xsimd_name': 'avx512bw',
            },
            {
                'name': 'avx2',
                'flags': ['-mavx2'],
                'xsimd_name': 'avx2',
            },
            {
                'name': 'sse2',
                'flags': ['-msse2'],
                'xsimd_name': 'sse2',
            },
        ]
    endif

    foreach arch : archs
        enable_arch_def = '-DSQP_DSP_ENABLE_' + arch['name'].to_upper() + '_ARCH'
        x86_dsp_libs += [
            static_library('sqp_dsp_x86_' + arch['name'],
                files('kernels_arch.cpp'),
                include_directories: dsp_includes,
                cpp_args: arch['flags'] + [enable_arch_def, '-DSQP_DSP_ARCH=' + arch['xsimd_name']],
                dependencies: [xsimd_dep],
            ),
        ]
        x86_dsp_defs += [enable_arch_def]
        xsimd_arch_list += ['xsimd::' + arch['xsimd_name']]
    endforeach

    xsimd_arch_list_str = 'xsimd::arch_list<' + ', '.join(xsimd_arch_list) + '>'

    x86_dsp_dep = declare_dependency(
        sources: files('kernels_dispatch.cpp'),
        link_with: x86_dsp_libs,
        compile_args: x86_dsp_defs + ['-DSQP_DSP_XSIMD_ARCH_LIST=' + xsimd_arch_list_str],
        dependencies: [xsimd_dep],
    )
    dsp_simd_deps += [x86_dsp_dep]

elif target_machine.cpu_family() == 'aarch64'
    # NEON is always available on AArch64 — single arch, no runtime dispatch needed
    arm_dsp_lib = static_library('sqp_dsp_arm_neon64',
        files('kernels_arch.cpp'),
        include_directories: dsp_includes,
        cpp_args: ['-DSQP_DSP_ENABLE_NEON64_ARCH', '-DSQP_DSP_ARCH=neon64'],
        dependencies: [xsimd_dep],
    )

    arm_dsp_dep = declare_dependency(
        sources: files('kernels_dispatch.cpp'),
        link_with: [arm_dsp_lib],
        compile_args: ['-DSQP_DSP_ENABLE_NEON64_ARCH',
                       '-DSQP_DSP_XSIMD_ARCH_LIST=xsimd::arch_list<xsimd::neon64>'],
        dependencies: [xsimd_dep],
    )
    dsp_simd_deps += [arm_dsp_dep]

else
    add_project_arguments('-DSQP_DSP_NO_SIMD', language: ['cpp'])

endif
