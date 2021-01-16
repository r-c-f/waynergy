project('waynergy', 'c')

prot = [
  'idle.xml',
  'wlr-virtual-pointer-unstable-v1.xml',
  'virtual-keyboard-unstable-v1.xml',
  'xdg-output-unstable-v1.xml',
]
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

prot_src = []
prot_h = []
foreach p : prot
  prot_src += custom_target(
    p + '.c',
    input: p,
    output: '@BASENAME@.prot.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
  prot_h += custom_target(
    p + '.h',
    input: p,
    output: '@BASENAME@.prot.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])
endforeach
lib_prot = static_library('prot', prot_src + prot_h, dependencies: [wayland_client])

executable('waynergy', src_c, install: true, dependencies : [wayland_client, xkbcommon], link_with: lib_prot)
executable('waynergy-clip-update', 'clip-update.c', install: true)
