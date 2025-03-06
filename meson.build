project('ampart', 'c',
    version : run_command('bash', 'scripts/build-only-version.sh', check: true).stdout().strip())
incdir = include_directories('include')
zlibdep = dependency('zlib')

executable('ampart', 
    'src/cli.c', 'src/dtb.c', 'src/dts.c', 'src/ept.c', 'src/gzip.c', 'src/io.c', 'src/main.c', 'src/parg.c', 'src/size.c', 'src/stringblock.c', 'src/util.c', 'src/version.c',
    dependencies : zlibdep,
    include_directories: incdir,
    c_args: '-DVERSION="@0@"'.format(meson.project_version()),
    install: true)
