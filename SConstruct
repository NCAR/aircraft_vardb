#!python

# This SConstruct does nothing more than load the SConscript in this dir
# The Environment() is created in the SConstruct script
# This dir can be built standalone by executing scons here, or together
# by executing scons in a parent directory

def Vardb(env):
    env.Require(['prefixoptions'])

env = Environment(GLOBAL_TOOLS = [Vardb])

SConscript('SConscript')
