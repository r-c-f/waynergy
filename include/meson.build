ver_h = vcs_tag(
  command: ['git', 'describe', '--tags'],
  input: 'ver.h.in',
  output: 'ver.h',
  replace_string: '@VCS_DESC@'
)
ver_dep = declare_dependency(
  sources: [ ver_h ],
)
