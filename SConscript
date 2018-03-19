# -*- python -*-

Import('PREFIX')

import os
import sys
import eol_scons

def vardb_global(env):
    "Copy prefix settings into the prefixoptions."
    env['DEFAULT_INSTALL_PREFIX'] = "$PREFIX"
    # The python wrapper must be built as a shared library, and so all the
    # libraries it links against must be relocatable, eg liblogx, libdomx,
    # and libVarDB.
    env.AppendUnique(CXXFLAGS=['-fPIC'])
    env.Require('prefixoptions')
    env['VARDB_README_FILE'] = env.File("$PREFIX/README")
    env.Append(CPPPATH = PREFIX+'/raf')
    env.Append(CPPPATH = PREFIX+'/include')
    env.Append(CPPPATH = PREFIX+'vardb')
    env.Append(LIBPATH = PREFIX+'/lib')

env = Environment(tools=['default'], GLOBAL_TOOLS=[vardb_global])

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
