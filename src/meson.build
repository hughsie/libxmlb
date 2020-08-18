libxmlb_version_h = configure_file(
  input : 'xb-version.h.in',
  output : 'xb-version.h',
  configuration : conf
)

install_headers(
  'xmlb.h',
  subdir : 'libxmlb-2',
)

xb_headers = files(
  'xb-builder.h',
  'xb-builder-fixup.h',
  'xb-builder-node.h',
  'xb-builder-source.h',
  'xb-builder-source-ctx.h',
  'xb-machine.h',
  'xb-node.h',
  'xb-node-query.h',
  'xb-node-silo.h',
  'xb-opcode.h',
  'xb-query.h',
  'xb-silo-export.h',
  'xb-silo.h',
  'xb-silo-query.h',
  'xb-stack.h',
  'xb-string.h',
) + [libxmlb_version_h]

install_headers(
  xb_headers,
  subdir : 'libxmlb-2/libxmlb',
)

subdir('libxmlb')

mapfile = 'libxmlb.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
libxmlb = library(
  'xmlb',
  sources : [
    'xb-builder.c',
    'xb-builder-fixup.c',
    'xb-builder-node.c',
    'xb-builder-source.c',
    'xb-builder-source-ctx.c',
    'xb-common.c',
    'xb-machine.c',
    'xb-opcode.c',
    'xb-node.c',
    'xb-node-query.c',
    'xb-query.c',
    'xb-silo.c',
    'xb-silo-export.c',
    'xb-silo-query.c',
    'xb-stack.c',
    'xb-string.c',
  ],
  soversion : lt_current,
  version : lt_version,
  include_directories : [
    configinc,
  ],
  dependencies : libxmlb_deps,
  link_args : vflag,
  link_depends : mapfile,
  install : true
)

libxmlb_dep = declare_dependency(
  link_with : libxmlb,
  include_directories : include_directories('.'),
  dependencies : libxmlb_deps
)

xb_tool = executable(
  'xb-tool',
  sources : [
    'xb-tool.c',
  ],
  include_directories : [
    configinc,
  ],
  dependencies : [
    gio,
  ],
  link_with : [
    libxmlb,
  ],
  install : true,
  install_dir : libexecdir
)

pkgg = import('pkgconfig')
pkgg.generate(
  libraries : libxmlb,
  requires : [ 'gio-2.0' ],
  subdirs : 'libxmlb-2',
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
      'xb-builder-fixup.c',
      'xb-builder-fixup.h',
      'xb-builder-node.c',
      'xb-builder-node.h',
      'xb-builder-source.c',
      'xb-builder-source.h',
      'xb-builder-source-ctx.c',
      'xb-builder-source-ctx.h',
      'xb-common.c',
      'xb-machine.c',
      'xb-machine.h',
      'xb-node.c',
      'xb-node.h',
      'xb-node-query.c',
      'xb-node-query.h',
      'xb-node-silo.h',
      'xb-opcode.c',
      'xb-opcode.h',
      'xb-query.c',
      'xb-query.h',
      'xb-silo.c',
      'xb-silo.h',
      'xb-silo-export.c',
      'xb-silo-export.h',
      'xb-silo-query.c',
      'xb-silo-query.h',
      'xb-stack.c',
      'xb-stack.h',
      'xb-string.c',
      'xb-string.h',
    ],
    nsversion : '2.0',
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
    link_with : [
      libxmlb,
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
  # accidentally actually check in a version of the version script to git.
  mapfile_target = custom_target('libxmlb-mapfile',
    input: gir[0],
    output: 'libxmlb.map',
    command: [
      python3,
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
  testdatadirs = environment()
  testdatadirs.set('G_TEST_SRCDIR', join_paths(meson.source_root(), 'data'))
  testdatadirs.set('G_TEST_BUILDDIR', meson.current_build_dir())
  e = executable(
    'xb-self-test',
    sources : [
      'xb-builder.c',
      'xb-builder-fixup.c',
      'xb-builder-fixup.c',
      'xb-builder-node.c',
      'xb-builder-source.c',
      'xb-builder-source-ctx.c',
      'xb-common.c',
      'xb-machine.c',
      'xb-node.c',
      'xb-node-query.c',
      'xb-opcode.c',
      'xb-self-test.c',
      'xb-query.c',
      'xb-silo.c',
      'xb-silo-export.c',
      'xb-silo-query.c',
      'xb-stack.c',
      'xb-string.c',
    ],
    include_directories : [
      configinc,
    ],
    dependencies : [
      gio,
      libxmlb_dep,
    ],
    install : true,
    install_dir : installed_test_bindir
  )
  test('xb-self-test', e, env : testdatadirs)
endif
