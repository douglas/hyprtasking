project('hyprtasking', 'cpp',
  version: '0.5',
  default_options: ['buildtype=debug'],
  meson_version: '>= 1.1'
)

if host_machine.cpu_family() != 'x86_64'
  error('Hyprtasking uses Hyprland function hooks, which are supported only on x86_64')
endif

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
build_contract.set('HT_SUPPORTED_HYPRLAND_TARGETS', '0.54.3 and 0.55.0')
build_contract.set('HT_PROJECT_VERSION', meson.project_version())
build_contract.set10('HT_HYPRLAND_GE_0_55', hyprland_dep.version().version_compare('>=0.55.0'))
build_contract.set10('HT_FUNCTION_HOOKS_SUPPORTED_ARCH', host_machine.cpu_family() == 'x86_64')

configure_file(
  input: 'src/build_contract.hpp.in',
  output: 'build_contract.hpp',
  configuration: build_contract,
)

core_src = [
  'src/config.cpp',
  'src/compat/renderer_compat.cpp',
  'src/compat/runtime_compat.cpp',
  'src/input.cpp',
  'src/main.cpp',
  'src/manager.cpp',
  'src/overview.cpp',
  'src/render.cpp',
  'src/render_snapshot.cpp',
  'src/runtime_validation.cpp',
  'src/trace.cpp',
]

layout_src = [
  'src/layout/grid.cpp',
  'src/layout/layout_base.cpp',
]

logic_src = [
  'src/logic/compat_model.cpp',
  'src/logic/controller_state.cpp',
  'src/logic/gesture_model.cpp',
  'src/logic/geometry_model.cpp',
  'src/logic/interaction_model.cpp',
  'src/logic/navigation_model.cpp',
  'src/logic/view_sync_model.cpp',
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
    'src/logic/gesture_model.cpp',
    'src/logic/geometry_model.cpp',
    'src/logic/interaction_model.cpp',
    'src/logic/navigation_model.cpp',
    'src/logic/view_sync_model.cpp',
  ],
  include_directories: include_directories('.'),
)

test('logic-tests', logic_tests)
