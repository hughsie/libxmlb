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
  'xb-compile.h',
  'xb-machine.h',
  'xb-node.h',
  'xb-node-query.h',
  'xb-node-silo.h',
  'xb-opcode.h',
  'xb-query.h',
  'xb-query-context.h',
  'xb-silo-export.h',
  'xb-silo.h',
  'xb-silo-query.h',
  'xb-stack.h',
  'xb-string.h',
  'xb-value-bindings.h',
) + [libxmlb_version_h]

install_headers(
  xb_headers,
  subdir : 'libxmlb-2/libxmlb',
)

subdir('libxmlb')

mapfile = 'libxmlb.map'
vflag = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), mapfile)
extra_sources = []
if zstd.found()
  extra_sources += ['xb-zstd-decompressor.c']
endif
if lzma.found()
  extra_sources += ['xb-lzma-decompressor.c']
endif

def_file_target = custom_target(
  'libxmlb-defile',
  input: mapfile,
  output: 'libxmlb.def',
  command: [
    python3,
    join_paths(meson.current_source_dir(), 'gen-def-from-map.py'),
    '@INPUT@',
    'LIBXMLB',
    '@OUTPUT@',
  ],
)

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
    'xb-query-context.c',
    'xb-silo.c',
    'xb-silo-export.c',
    'xb-silo-node.c',
    'xb-silo-query.c',
    'xb-stack.c',
    'xb-string.c',
    'xb-value-bindings.c',
    'xb-version.c',
  ] + extra_sources,
  soversion : lt_current,
  version : lt_version,
  include_directories : [
    configinc,
  ],
  dependencies : libxmlb_deps,
  c_args : release_args,
  link_args : cc.get_supported_link_arguments([vflag]),
  vs_module_defs: def_file_target,
  link_depends : mapfile,
  install : true
)

libxmlb_dep = declare_dependency(
  link_with : libxmlb,
  include_directories : include_directories('.'),
  dependencies : libxmlb_deps,
  variables : {
    'lzma': '@0@'.format(lzma.found()),
    'zstd': '@0@'.format(zstd.found()),
  },
)

if get_option('cli')
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
  install_dir : bindir
)
configure_file(
  input : 'xb-tool.1',
  output : 'xb-tool.1',
  configuration : conf,
  install: true,
  install_dir: join_paths(mandir, 'man1'),
)
endif

pkgg = import('pkgconfig')
pkgg.generate(libxmlb,
  requires : [ 'gio-2.0' ],
  subdirs : 'libxmlb-2',
  version : meson.project_version(),
  name : 'libxmlb',
  filebase : 'xmlb',
  description : 'libxmlb is a library to create or query compressed XML files',
  variables : {
    'lzma': '@0@'.format(lzma.found()),
    'zstd': '@0@'.format(zstd.found()),
  },
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
      'xb-query-context.c',
      'xb-query-context.h',
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
      'xb-value-bindings.c',
      'xb-value-bindings.h',
      'xb-version.c',
      libxmlb_version_h,
    ],
    nsversion : '2.0',
    namespace : 'Xmlb',
    symbol_prefix : [
      'xb',
      'xmlb',
      'libxmlb',
    ],
    identifier_prefix : 'Xb',
    export_packages : 'xmlb',
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
  testdatadirs.set('G_TEST_SRCDIR', join_paths(meson.project_source_root(), 'data'))
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
      'xb-query-context.c',
      'xb-silo.c',
      'xb-silo-export.c',
      'xb-silo-node.c',
      'xb-silo-query.c',
      'xb-stack.c',
      'xb-string.c',
      'xb-value-bindings.c',
    ] + extra_sources,
    include_directories : [
      configinc,
    ],
    dependencies : [
      gio,
      libxmlb_dep,
    ],
    c_args: [
      '-DSRCDIR="' + join_paths(meson.project_source_root(), 'data') + '"',
    ],
    install : true,
    install_dir : installed_test_bindir
  )
  test('xb-self-test', e, env : testdatadirs)
endif
