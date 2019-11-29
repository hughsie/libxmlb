if get_option('tests') and host_machine.system() != 'windows'
  configure_file(
    input : 'libxmlb.test.in',
    output : 'libxmlb.test',
    configuration : conf,
    install: true,
    install_dir: installed_test_datadir,
  )

  install_data(['test.xml.gz.gz.gz'],
    install_dir: installed_test_datadir,
  )
endif
