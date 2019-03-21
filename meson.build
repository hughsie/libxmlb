project('libxmlb', 'c',
  version : '0.1.8',
  license : 'LGPL-2.1+',
  meson_version : '>=0.47.0',
  default_options : ['warning_level=2', 'c_std=c99'],
)

libxmlb_version = meson.project_version()
varr = libxmlb_version.split('.')
libxmlb_major_version = varr[0]
libxmlb_minor_version = varr[1]
libxmlb_micro_version = varr[2]

conf = configuration_data()
conf.set('XMLB_MAJOR_VERSION', libxmlb_major_version)
conf.set('XMLB_MINOR_VERSION', libxmlb_minor_version)
conf.set('XMLB_MICRO_VERSION', libxmlb_micro_version)
conf.set_quoted('PACKAGE_VERSION', libxmlb_version)

# libtool versioning - this applies to libxmlb
lt_current = '1'
lt_revision = '0'
lt_age = '0'
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

configinc = include_directories('.')

# get suported warning flags
warning_flags = [
  '-fstack-protector-strong',
  '-Waggregate-return',
  '-Wunused',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wclobbered',
  '-Wdeclaration-after-statement',
  '-Wduplicated-branches',
  '-Wduplicated-cond',
  '-Wempty-body',
  '-Wformat=2',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat-signedness',
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wincompatible-pointer-types-discards-qualifiers',
  '-Winit-self',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-error=cpp',
  '-Wno-unknown-pragmas',
  '-Wno-discarded-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-strict-aliasing',
  '-Wno-suggest-attribute=format',
  '-Wno-unused-parameter',
  '-Wnull-dereference',
  '-Wold-style-definition',
  '-Woverride-init',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wtype-limits',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused-but-set-variable',
  '-Wunused-variable',
  '-Wwrite-strings'
]
cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments(warning_flags), language : 'c')

# enable full RELRO where possible
# FIXME: until https://github.com/mesonbuild/meson/issues/1140 is fixed
global_link_args = []
test_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]
foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    global_link_args += link_arg
  endif
endforeach
add_project_link_arguments(
  global_link_args,
  language: 'c'
)

prefix = get_option('prefix')

libexecdir = join_paths(prefix, get_option('libexecdir'))

gio = dependency('gio-2.0', version : '>= 2.45.8')
uuid = dependency('uuid')
if cc.has_function('uuid_generate_sha1', args : '-luuid')
  conf.set('HAVE_UUID_GENERATE_SHA1', '1')
endif

libxmlb_deps = [
  gio,
  uuid,
]

# support stemming of search tokens
if get_option('stemmer')
  cc = meson.get_compiler('c')
  stemmer = cc.find_library('stemmer')
  libxmlb_deps += stemmer
  conf.set('HAVE_LIBSTEMMER', 1)
endif

gnome = import('gnome')

conf.set('libexecdir', libexecdir)
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
configure_file(
  output : 'config.h',
  configuration : conf
)

python = import('python')
python3 = python.find_installation('python3')

subdir('data')
subdir('src')
if get_option('gtkdoc')
  gtkdocscan = find_program('gtkdoc-scan', required : true)
  subdir('docs')
endif
