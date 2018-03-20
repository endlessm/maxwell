project('eos-web-view','c',
        version: '0.1.0',
        default_options: [
          'buildtype=debugoptimized',
          'c_std=c99',
          'warning_level=1'
        ],
        license: 'LGPLv2.1+',
        meson_version: '>= 0.40.0',
)

config_h = configuration_data()

configure_file(
  output: 'eos-web-view-config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.build_root(),
], language: 'c')

subdir('src')
subdir('examples')

