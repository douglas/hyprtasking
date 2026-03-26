project('hyprtasking', 'cpp',
  version: '0.1',
  default_options: ['buildtype=debug'],
  meson_version: '>= 1.1'
)

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.has_argument('-std=c++23')
  add_global_arguments('-std=c++23', language: 'cpp')
elif cpp_compiler.has_argument('-std=c++2b')
  add_global_arguments('-std=c++2b', language: 'cpp')
else
  error('Could not configure current C++ compiler (' + cpp_compiler.get_id() + ' ' + cpp_compiler.version() + ') with required C++ standard (C++23)')
endif

add_project_arguments(
  [
    '-Wno-narrowing', '-shared', '-fPIC', '--no-gnu-unique'
  ],
  language: 'cpp')

globber = run_command('find', 'src', '-name', '*.cpp', check: true)
src = globber.stdout().strip().split('\n')

incdir = []
dependencies = [
  dependency('pixman-1'),
  dependency('libdrm'),
  dependency('pangocairo'),
  dependency('libinput'),
  dependency('libudev'),
  dependency('wayland-server'),
  dependency('xkbcommon'),
]

if get_option('local_include')
  incdir += include_directories('include')
  incdir += include_directories('include/hyprland/protocols')
else
  dependencies += dependency('hyprland')
endif

shared_module(meson.project_name(), src,
  dependencies: dependencies,
  install: true,
  include_directories: incdir
)

logic_tests = executable('logic-tests',
  [
    'tests/logic_tests.cpp',
    'src/logic/controller_state.cpp',
    'src/logic/dispatch_args.cpp',
    'src/logic/gesture_model.cpp',
    'src/logic/layout_model.cpp',
    'src/logic/reload_model.cpp',
  ],
  include_directories: include_directories('.'),
)

test('logic-tests', logic_tests)
