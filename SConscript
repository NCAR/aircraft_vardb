# -*- python -*-


import os
import sys
import eol_scons

def vardb_global(env):
    "Copy prefix settings into the prefixoptions."
    env['DEFAULT_INSTALL_PREFIX'] = "$INSTALL_PREFIX"
    # The python wrapper must be built as a shared library, and so all the
    # libraries it links against must be relocatable, eg liblogx, libdomx,
    # and libVarDB.
    env.AppendUnique(CXXFLAGS=['-fPIC'])
    env.Require('prefixoptions')
    env['VARDB_README_FILE'] = env.File("$INSTALL_PREFIX/README")
    env.Append(CPPPATH = ['#/include'])
    env.Append(LIBPATH = ['#/lib'])
    env.Append(LIBPATH = ['#/raf'])
    if env['PLATFORM'] == 'darwin':
      env.Append(CPPPATH=['/opt/X11/include'])
      env.Append(LIBPATH=['/opt/X11/lib'])

env = Environment(tools=['default','prefixoptions'], GLOBAL_TOOLS=[vardb_global])

SConscript('raf/SConscript')
SConscript('lib/SConscript')
SConscript('vdbdump/SConscript')
SConscript('vdb2xml/SConscript')
SConscript('vdb2ncml/SConscript')
SConscript('editpy/SConscript')
SConscript('editor/SConscript')
SConscript('python/SConscript')
SConscript('tests/SConscript')

env.Alias('apidocs', env.Dir("apidocs"))

variables = env.GlobalVariables()
variables.Update(env)
Help(variables.GenerateHelpText(env))
