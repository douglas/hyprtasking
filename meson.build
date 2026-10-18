project('hyprtasking', 'cpp',
  version: '0.6.0',
  default_options: ['buildtype=debugoptimized', 'b_ndebug=if-release', 'warning_level=3'],
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

# Plugins must not use STB_GNU_UNIQUE symbols or they cannot be unloaded (GCC only).
# Designated initializers intentionally rely on default member initializers.
add_project_arguments(
  cpp_compiler.get_supported_arguments(
    '--no-gnu-unique',
    '-Wno-missing-field-initializers',
    '-Wold-style-cast',
    '-Wnon-virtual-dtor',
  ),
  language: 'cpp')

# System include type keeps warnings from Hyprland headers out of our build.
hyprland_dep = dependency('hyprland', version: '>=0.55.0', include_type: 'system')

build_contract = configuration_data()
build_contract.set('HT_BUILT_HYPRLAND_PKG_VERSION', hyprland_dep.version())
build_contract.set('HT_PROJECT_VERSION', meson.project_version())
build_contract.set10('HT_HYPRLAND_GE_0_56', hyprland_dep.version().version_compare('>=0.56.0'))

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

src = core_src + layout_src + plugin_src + pass_src

# Pure logic, shared by the plugin and the tests; no Hyprland dependency.
logic_lib = static_library('htlogic', logic_src, pic: true)

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
  link_with: logic_lib,
  install: true,
  include_directories: incdir
)

logic_tests = executable('logic-tests',
  'tests/logic_tests.cpp',
  link_with: logic_lib,
  include_directories: include_directories('.'),
)

test('logic-tests', logic_tests)
