api_version = '0.1'

eos_web_view_sources = [
  'eos-web-view.c',
]

eos_web_view_headers = [
  'eos-web-view.h',
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
  input: 'eos-web-view-version.h.in',
  output: 'eos-web-view-version.h',
  configuration: version_conf,
  install: true,
  install_dir: join_paths(get_option('includedir'), 'eos-web-view')
)

eos_web_view_deps = [
  dependency('webkit2gtk-4.0', version: '>= 2.18'),
]

gnome = import('gnome')

eos_web_view_sources += gnome.compile_resources(
    'eos-web-view-resources', 'eos-web-view.gresource.xml',
    c_name: 'eos_web_view'
)

eos_web_view_lib = shared_library('eos-web-view-' + api_version,
  eos_web_view_sources,
  dependencies: eos_web_view_deps,
  install: true,
)

install_headers(eos_web_view_headers, subdir: 'eos-web-view')

# Internal dependency, for examples
eos_web_view_inc = include_directories('.')
eos_web_view_dep = declare_dependency(link_with: eos_web_view_lib,
                                      include_directories: [ eos_web_view_inc ],
                                      dependencies: [ eos_web_view_deps ])

pkg = import('pkgconfig')

pkg.generate(
  description: 'A WebKitWebView derived class that lets you embed GtkWidget',
    libraries: eos_web_view_lib,
         name: 'eos-web-view',
     filebase: 'eos-web-view-' + api_version,
      version: meson.project_version(),
      subdirs: 'eos-web-view',
     requires: 'webkit2gtk-4.0',
  install_dir: join_paths(get_option('libdir'), 'pkgconfig')
)
