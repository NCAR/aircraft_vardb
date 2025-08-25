#!python

# This SConstruct does nothing more than load the SConscript in this dir
# The Environment() is created in the SConstruct script
# This dir can be built standalone by executing scons here, or together
# by executing scons in a parent directory
import os
import eol_scons

def Vardb(env):
    env['INSTALL_VARDB'] = install_vdb
    env.Require(['prefixoptions'])
    # Add NetCDF include path explicitly

install_vdb = False
if Dir('#') == Dir('.') :
  install_vdb = True

env = Environment(GLOBAL_TOOLS = [Vardb])

SConscript('tool_vardb.py')

variables = env.GlobalVariables()
variables.Update(env)
Help(variables.GenerateHelpText(env))
