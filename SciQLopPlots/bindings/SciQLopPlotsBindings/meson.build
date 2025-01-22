
shiboken_generator_out = custom_target('sciqlopplots_bindings',
                                build_by_default:true,
                                output:generated_srcs,
                                input:'../bindings.xml',
                                depend_files:sciqlopplots_bindings_src+['../snippets.cpp'],
                                command:[
                                    python3, meson.current_source_dir()+'/../helper_scripts/shiboken-gen.py',
                                    '--input-header', meson.current_source_dir()+'/../bindings.h',
                                    '--input-xml', meson.current_source_dir()+'/../bindings.xml',
                                    '--shiboken', shiboken.full_path(),
                                    '--typesystem-paths', shiboken_typesystem,
                                    '--build-directory', meson.project_build_root(),
                                    '--ref-build-target', 'SciQLopPlotsBindings',
                                    '--output-directory',meson.current_build_dir()+'/../'
                                ]
                                )
