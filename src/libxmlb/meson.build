# FIXME: We need to copy the headers to ${build_root}/libxmlb
# in order for including the headers from parent projects to work
# until https://github.com/mesonbuild/meson/issues/2546 is fixed
foreach header : xb_headers
  configure_file(
    configuration : configuration_data(),
    input : header,
    output : '@PLAINNAME@',
  )
endforeach
