
common_inc  = include_directories('common')
thread_dep = dependency('threads')

tests = [
    {
        'name':'SimplePlot',
        'sources': ['SimplePlot/main.cpp'],
        'deps': [qttest, SciQLopPlots_dep, QtGuiTestUtils_dep, thread_dep]
    }
]

foreach unit_test : tests
  test_moc_files = qtmod.preprocess(moc_sources : unit_test['sources'])
  test_exe = executable(unit_test['name'],unit_test['sources'] , test_moc_files,
                          include_directories : common_inc,
                          dependencies : unit_test['deps'])

  test('Test-' + unit_test['name'], test_exe)
endforeach

subdir('manual-tests')
