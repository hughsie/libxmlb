if get_option('tests') and host_machine.system() != 'windows'
  configure_file(
    input : 'libxmlb.test.in',
    output : 'libxmlb.test',
    configuration : conf,
    install: true,
    install_dir: installed_test_datadir,
  )

  install_data([
    'test.desktop',
    'test.quirk',
    'test.xml',
    'test.xml.gz.gz.gz',
    'test.xml.xz',
    'test.xml.zst',
  ],
    install_dir: installed_test_bindir,
  )
endif
