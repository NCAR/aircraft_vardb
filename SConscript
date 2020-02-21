# -*- python -*-

AddOption('--python',
    dest='python',
    action='store_true',
    default='False',
    help='compile python dir')

def vardb_global(env):
    "Copy prefix settings into the prefixoptions."
    # The python wrapper must be built as a shared library, and so all the
    # libraries it links against must be relocatable, eg liblogx, libdomx,
    # and libVarDB.
    env.AppendUnique(CXXFLAGS=['-fPIC'])
    env['VARDB_README_FILE'] = env.File("$INSTALL_PREFIX/README")
    if env['PLATFORM'] == 'darwin':
      env.Append(CPPPATH=['/opt/X11/include'])
      env.Append(LIBPATH=['/opt/X11/lib'])

env = Environment(tools=['default','prefixoptions'], GLOBAL_TOOLS=[vardb_global])

if env['DEFAULT_OPT_PREFIX'] == "#": 	# build locally
    SConscript('raf/SConscript')
# If not built locally, the raf tool under eol_scons will add libraf to the path.

SConscript('src/vardb/SConscript')
SConscript('src/vdbdump/SConscript')
SConscript('src/vdb2xml/SConscript')
SConscript('src/vdb2ncml/SConscript')
SConscript('editpy/SConscript')
SConscript('src/editor/SConscript')
if GetOption('python') == "True":
    SConscript('python/SConscript')
SConscript('tests/SConscript')


env.Alias('apidocs', env.Dir("apidocs"))
