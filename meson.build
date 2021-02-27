project('waynergy', 'c')

src_c = [
  'wl_idle.c',
  'wl_key.c',
  'wl_mouse.c',
  'main.c',
  'clip.c',
  'config.c',
  'net.c',
  'os.c',
  'sig.c',
  'ssp.c',
  'wayland.c',
  'uSynergy.c',
  'log.c'
]

wayland_client = dependency('wayland-client')
xkbcommon = dependency('xkbcommon')
wayland_scanner = find_program('wayland-scanner')
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
    wayland_client, 
    xkbcommon
  ], 
)
executable('waynergy-clip-update', 'clip-update.c', install: true)
