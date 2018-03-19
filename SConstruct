# -*- python -*-

# This SConstruct does nothing more than load the SConscript in this dir
# The Environment() is created in the SConstruct script
# This dir can be built standalone by executing scons here, or together
# by executing scons in a parent directory

AddOption('--prefix',
  dest='prefix',
  type='string',
  nargs=1,
  action='store',
  metavar='DIR',
  default='#',
  help='installation prefix')

env = Environment(PREFIX = GetOption('prefix'))
PREFIX=env['PREFIX']

Export('PREFIX')

SConscript('SConscript', exports = ['PREFIX'])
