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

hyprland_dep = dependency('hyprland')

build_contract = configuration_data()
build_contract.set('HT_BUILT_HYPRLAND_PKG_VERSION', hyprland_dep.version())
build_contract.set('HT_SUPPORTED_HYPRLAND_LINE', '0.54.x')
build_contract.set('HT_SUPPORTED_HYPRLAND_MINOR_PREFIX', '0.54.')

configure_file(
  input: 'src/build_contract.hpp.in',
  output: 'build_contract.hpp',
  configuration: build_contract,
)

core_src = [
  'src/compat/renderer_compat.cpp',
  'src/compat/runtime_compat.cpp',
  'src/input.cpp',
  'src/main.cpp',
  'src/manager.cpp',
  'src/overview.cpp',
  'src/render.cpp',
  'src/render_snapshot.cpp',
]

layout_src = [
  'src/layout/grid.cpp',
  'src/layout/layout_base.cpp',
  'src/layout/linear.cpp',
]

logic_src = [
  'src/logic/compat_model.cpp',
  'src/logic/controller_state.cpp',
  'src/logic/dispatch_args.cpp',
  'src/logic/gesture_model.cpp',
  'src/logic/geometry_model.cpp',
  'src/logic/interaction_model.cpp',
  'src/logic/layout_model.cpp',
  'src/logic/reload_model.cpp',
]

pass_src = [
  'src/pass/pass_element.cpp',
]

plugin_src = [
  'src/compat/profile.cpp',
  'src/plugin/dispatchers.cpp',
  'src/plugin/runtime.cpp',
]

src = core_src + layout_src + logic_src + plugin_src + pass_src

incdir = []
dependencies = [
  dependency('pixman-1'),
  dependency('libdrm'),
  dependency('pangocairo'),
  dependency('libinput'),
  dependency('libudev'),
  dependency('wayland-server'),
  dependency('xkbcommon'),
  hyprland_dep,
]

shared_module(meson.project_name(), src,
  dependencies: dependencies,
  install: true,
  include_directories: incdir
)

logic_tests = executable('logic-tests',
  [
    'tests/logic_tests.cpp',
    'src/logic/compat_model.cpp',
    'src/logic/controller_state.cpp',
    'src/logic/dispatch_args.cpp',
    'src/logic/gesture_model.cpp',
    'src/logic/geometry_model.cpp',
    'src/logic/interaction_model.cpp',
    'src/logic/layout_model.cpp',
    'src/logic/reload_model.cpp',
  ],
  include_directories: include_directories('.'),
)

test('logic-tests', logic_tests)
