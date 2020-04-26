#!python

# This SConstruct does nothing more than load the SConscript in this dir
# The Environment() is created in the SConstruct script
# This dir can be built standalone by executing scons here, or together
# by executing scons in a parent directory

import eol_scons

AddOption('--prefix',
  dest='prefix',
  type='string',
  nargs=1,
  action='store',
  metavar='DIR',
  default='#',
  help='installation prefix')

def Vardb(env):
    env['DEFAULT_INSTALL_PREFIX'] = GetOption('prefix')
    env['DEFAULT_OPT_PREFIX'] = GetOption('prefix')
    env.Require(['prefixoptions'])

env = Environment(GLOBAL_TOOLS = [Vardb])

SConscript('tool_vardb.py')

variables = env.GlobalVariables()
variables.Update(env)
Help(variables.GenerateHelpText(env))
