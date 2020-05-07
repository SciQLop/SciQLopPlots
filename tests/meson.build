tests = [
    {
        'name':'SimplePlot',
        'sources': ['SimplePlot/main.cpp'],
        'deps': [qt5test, SciQLopPlots_dep]
    }
]

foreach unit_test : tests
  test_moc_files = qt5.preprocess(moc_sources : unit_test['sources'])
  test_exe = executable(unit_test['name'],unit_test['sources'] , test_moc_files,
                          dependencies : unit_test['deps'])

  test('Test-' + unit_test['name'], test_exe)
endforeach
