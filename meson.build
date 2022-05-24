project('libxmlb', 'c',
  version : '0.3.10',
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
lt_current = '2'
lt_revision = '0'
lt_age = '0'
lt_version = '@0@.@1@.@2@'.format(lt_current, lt_age, lt_revision)

configinc = include_directories('.')

# get supported warning flags
warning_flags = [
  '-Wno-aggregate-return',
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

if not meson.is_cross_build()
  add_project_arguments('-fstack-protector-strong', language : 'c')
endif

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

if host_machine.system() == 'windows'
  bindir = get_option('bindir')
  installed_test_bindir = get_option('libexecdir')
  installed_test_datadir = get_option('datadir')
else
  prefix = get_option('prefix')
  datadir = join_paths(prefix, get_option('datadir'))
  bindir = join_paths(prefix, get_option('bindir'))
  libexecdir = join_paths(prefix, get_option('libexecdir'))
  installed_test_bindir = join_paths(libexecdir, 'installed-tests', meson.project_name())
  installed_test_datadir = join_paths(datadir, 'installed-tests', meson.project_name())
endif
mandir = join_paths(prefix, get_option('mandir'))

gio = dependency('gio-2.0', version : '>= 2.45.8')
giounix = dependency('gio-unix-2.0', version : '>= 2.45.8', required: false)
lzma = dependency('liblzma')
if giounix.found()
  conf.set('HAVE_GIO_UNIX', '1')
endif

# Limit our use of GLib API to our minimum version requirement, and what’s
# available in Debian Stable. Use of more modern API has to be optional and
# protected by GLIB_CHECK_VERSION.
add_project_arguments('-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_46', language: 'c')
add_project_arguments('-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_58', language: 'c')

libxmlb_deps = [
  gio,
  lzma,
]

# support stemming of search tokens
if get_option('stemmer')
  cc = meson.get_compiler('c')
  stemmer = cc.find_library('stemmer')
  libxmlb_deps += stemmer
  conf.set('HAVE_LIBSTEMMER', 1)
endif

gnome = import('gnome')

conf.set('installed_test_bindir', installed_test_bindir)
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())
configure_file(
  output : 'config.h',
  configuration : conf
)

python3 = find_program('python3')

subdir('data')
subdir('src')
if get_option('gtkdoc')
  gtkdocscan = find_program('gtkdoc-scan', required : true)
  subdir('docs')
endif
