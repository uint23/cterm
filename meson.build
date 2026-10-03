project(
  'cterm',
  'c',
  meson_version: '>=1.1.0',
  default_options: [
    'c_std=c99',
    'warning_level=2',
    'optimization=2',
  ],
)

cc = meson.get_compiler('c')
fs = import('fs')

backend = get_option('window_backend')

maus_project = subproject(
  'libmaus',
  default_options: ['window_backend=' + backend],
)
maus_dep = maus_project.get_variable('maus_dep')

if not fs.exists('config.h')
  configure_file(input: 'config.def.h', output: 'config.h', copy: true,)
endif

inc = include_directories('.', 'include')

deps = [maus_dep]
c_args = []

if host_machine.system() == 'linux'
  c_args += ['-D_XOPEN_SOURCE=700']
elif host_machine.system() == 'freebsd' or host_machine.system() == 'openbsd'
  deps += cc.find_library('util')
endif

sources = [
  'source/cterm.c',
  'source/draw.c',
  'source/font.c',
  'source/term.c',
  'source/utils.c',
]

executable(
  'cterm',
  sources,
  include_directories: inc,
  dependencies: deps,
  c_args: c_args,
  install: true,
)

