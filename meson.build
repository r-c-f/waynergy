project('waynergy',
 'c',
 version: '0.0.1'
)

src_c = files(
  'src/wl_idle.c',
  'src/wl_key.c',
  'src/wl_mouse.c',
  'src/main.c',
  'src/clip.c',
  'src/config.c',
  'src/net.c',
  'src/os.c',
  'src/sig.c',
  'src/ssp.c',
  'src/wayland.c',
  'src/uSynergy.c',
  'src/log.c'
)

wayland_client = dependency('wayland-client')
xkbcommon = dependency('xkbcommon')
libtls = dependency('libtls')

if host_machine.system() == 'linux'
  add_project_arguments('-D_GNU_SOURCE ', language: 'c')
endif
if host_machine.endian() == 'big'
  add_project_arguments('-DUSYNERGY_BIG_ENDIAN', language: 'c')
else
  add_project_arguments('-DUSYNERGY_LITTLE_ENDIAN', language: 'c')
endif

subdir('protocol')

executable(
  'waynergy', 
  src_c, 
  install: true, 
  dependencies : [
    client_protos,
    libtls,
    wayland_client, 
    xkbcommon
  ],
  include_directories: [include_directories('include')],
)
executable(
  'waynergy-clip-update', 
  'src/clip-update.c', 
  install: true,
  include_directories: [include_directories('include')],
)
