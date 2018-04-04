api_version = '0.1'

maxwell_sources = [
  'maxwell.c',
  'maxwell-web-view.c',
  'js-utils.c',
]

maxwell_headers = [
  'maxwell.h',
  'maxwell-web-view.h',
]

version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0]
MINOR_VERSION = version_split[1]
MICRO_VERSION = version_split[2]

version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', MAJOR_VERSION)
version_conf.set('MINOR_VERSION', MINOR_VERSION)
version_conf.set('MICRO_VERSION', MICRO_VERSION)

configure_file(
  input: 'maxwell-version.h.in',
  output: 'maxwell-version.h',
  configuration: version_conf,
  install: true,
  install_dir: join_paths(get_option('includedir'), 'maxwell')
)

maxwell_deps = [
  dependency('webkit2gtk-4.0', version: '>= 2.18'),
]

gnome = import('gnome')

maxwell_sources += gnome.compile_resources(
    'maxwell-resources', 'maxwell.gresource.xml',
    c_name: 'maxwell'
)

maxwell_lib = shared_library('maxwell-' + api_version,
  maxwell_sources,
  dependencies: maxwell_deps,
  install: true,
)

install_headers(maxwell_headers, subdir: 'maxwell')

# Internal dependency, for examples
maxwell_inc = include_directories('.')
maxwell_dep = declare_dependency(link_with: maxwell_lib,
                                 include_directories: [ maxwell_inc ],
                                 dependencies: [ maxwell_deps ])

# PkgConfig file
pkg = import('pkgconfig')

pkg.generate(
  description: 'A WebKitWebView derived class that lets you embed GtkWidget',
    libraries: maxwell_lib,
         name: 'maxwell',
     filebase: 'maxwell-' + api_version,
      version: meson.project_version(),
      subdirs: 'maxwell',
     requires: 'webkit2gtk-4.0',
  install_dir: join_paths(get_option('libdir'), 'pkgconfig')
)

# Introspection
gir_extra_args = [
  '--c-include=maxwell.h',
]

gnome.generate_gir(maxwell_lib,
                   sources: maxwell_headers + maxwell_sources,
                   namespace: 'Maxwell',
                   nsversion: api_version,
                   identifier_prefix: 'Maxwell',
                   symbol_prefix: 'maxwell',
                   export_packages: 'maxwell-' + api_version,
                   includes: [ 'WebKit2-4.0' ],
                   install: true,
                   extra_args: gir_extra_args)
