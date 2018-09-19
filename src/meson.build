libxmlb_version_h = configure_file(
  input : 'xb-version.h.in',
  output : 'xb-version.h',
  configuration : conf
)

install_headers(
  'xmlb.h',
  subdir : 'libxmlb-1',
)

install_headers([
    'xb-builder.h',
    'xb-node.h',
    'xb-silo-export.h',
    'xb-silo.h',
    'xb-silo-query.h',
    libxmlb_version_h,
  ],
  subdir : 'libxmlb-1/libxmlb',
)

mapfile = 'libxmlb.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
libxmlb = shared_library(
  'xmlb',
  sources : [
    'xb-common.c',
    'xb-builder.c',
    'xb-builder-import.c',
    'xb-builder-node.c',
    'xb-node.c',
    'xb-silo.c',
    'xb-silo-export.c',
    'xb-silo-query.c',
  ],
  soversion : lt_current,
  version : lt_version,
  dependencies : [
    gio,
    uuid,
  ],
  include_directories : include_directories('..'),
  link_args : vflag,
  link_depends : mapfile,
  install : true
)

xb_tool = executable(
  'xb-tool',
  sources : [
    'xb-tool.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    gio,
  ],
  link_with : [
    libxmlb,
  ],
  install : true,
  install_dir : bindir
)

pkgg = import('pkgconfig')
pkgg.generate(
  libraries : libxmlb,
  requires : [ 'gio-2.0' ],
  subdirs : 'libxmlb-1',
  version : meson.project_version(),
  name : 'libxmlb',
  filebase : 'xmlb',
  description : 'libxmlb is a library to create or query compressed XML files',
)

if get_option('introspection')
  gir = gnome.generate_gir(libxmlb,
    sources : [
      'xb-builder.c',
      'xb-builder.h',
      'xb-node.c',
      'xb-node.h',
      'xb-silo.c',
      'xb-silo.h',
      'xb-silo-export.c',
      'xb-silo-export.h',
      'xb-silo-query.c',
      'xb-silo-query.h',
    ],
    nsversion : '1.0',
    namespace : 'Xmlb',
    symbol_prefix : 'xb',
    identifier_prefix : 'Xb',
    export_packages : 'libxmlb',
    header : 'xmlb.h',
    dependencies : [
      gio,
    ],
    includes : [
      'Gio-2.0',
      'GObject-2.0',
    ],
    install : true
  )

  # Verify the map file is correct -- note we can't actually use the generated
  # file for two reasons:
  #
  #  1. We don't hard depend on GObject Introspection
  #  2. The map file is required to build the lib that the GIR is built from
  #
  # To avoid the circular dep, and to ensure we don't change exported API
  # accidentaly actually check in a version of the version script to git.
  mapfile_target = custom_target('mapfile',
    input: gir[0],
    output: 'libxmlb.map',
    command: [
      join_paths(meson.current_source_dir(), 'generate-version-script.py'),
      'LIBXMLB',
      '@INPUT@',
      '@OUTPUT@',
    ],
  )
  diffcmd = find_program('diff')
  test('libxmlb-exported-api', diffcmd,
       args : [
        '-urNp',
        join_paths(meson.current_source_dir(), 'libxmlb.map'),
        mapfile_target,
        ],
      )
endif

if get_option('tests')
  testdatadir = join_paths(meson.source_root(), 'data')
  e = executable(
    'xb-self-test',
    sources : [
      'xb-self-test.c'
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      gio,
    ],
    link_with : libxmlb,
  )
  test('xb-self-test', e)
endif
