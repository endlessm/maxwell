api_version = '0.1'

maxwell_web_view_sources = [
  'maxwell-web-view.c',
  'js-utils.c',
]

maxwell_web_view_headers = [
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
  input: 'maxwell-web-view-version.h.in',
  output: 'maxwell-web-view-version.h',
  configuration: version_conf,
  install: true,
  install_dir: join_paths(get_option('includedir'), 'maxwell-web-view')
)

maxwell_web_view_deps = [
  dependency('webkit2gtk-4.0', version: '>= 2.18'),
]

gnome = import('gnome')

maxwell_web_view_sources += gnome.compile_resources(
    'maxwell-web-view-resources', 'maxwell-web-view.gresource.xml',
    c_name: 'maxwell_web_view'
)

maxwell_web_view_lib = shared_library('maxwell-web-view-' + api_version,
  maxwell_web_view_sources,
  dependencies: maxwell_web_view_deps,
  install: true,
)

install_headers(maxwell_web_view_headers, subdir: 'maxwell-web-view')

# Internal dependency, for examples
maxwell_web_view_inc = include_directories('.')
maxwell_web_view_dep = declare_dependency(link_with: maxwell_web_view_lib,
                                          include_directories: [ maxwell_web_view_inc ],
                                          dependencies: [ maxwell_web_view_deps ])

pkg = import('pkgconfig')

pkg.generate(
  description: 'A WebKitWebView derived class that lets you embed GtkWidget',
    libraries: maxwell_web_view_lib,
         name: 'maxwell-web-view',
     filebase: 'maxwell-web-view-' + api_version,
      version: meson.project_version(),
      subdirs: 'maxwell-web-view',
     requires: 'webkit2gtk-4.0',
  install_dir: join_paths(get_option('libdir'), 'pkgconfig')
)
