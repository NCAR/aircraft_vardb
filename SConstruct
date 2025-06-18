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
    env.Append(CPPPATH=['/opt/homebrew/Cellar/netcdf/4.9.3/include'])
    env.Append(CPPPATH=['/opt/homebrew/Cellar/netcdf-cxx/4.3.1_3/include'])
    # Add Xerces-C++ include path
    env.Append(CPPPATH=['/opt/homebrew/Cellar/xerces-c/3.3.0/include'])
    env.Append(CPPPATH=['/opt/homebrew/Cellar/boost/1.88.0/include'])
    
    # Add X11 include paths - using paths we found
    env.Append(CPPPATH=['/opt/homebrew/include'])
    env.Append(CPPPATH=['/opt/homebrew/Cellar/libxt/1.3.0/include'])
    env.Append(CPPPATH=['/opt/X11/include'])
    # Add library paths
    env.Append(LIBPATH=['/opt/homebrew/Cellar/netcdf/4.9.3/lib'])
    env.Append(LIBPATH=['/opt/homebrew/Cellar/netcdf-cxx/4.3.1_3/lib'])
    env.Append(LIBPATH=['/opt/homebrew/Cellar/xerces-c/3.3.0/lib'])
    env.Append(LIBPATH=['/opt/homebrew/lib'])
    env.Append(LIBPATH=['/opt/homebrew/Cellar/libxt/1.3.0/lib'])
    env.Append(LIBPATH=['/opt/X11/lib'])
    # Add the libraries needed
    env.Append(LIBS=['netcdf', 'netcdf-cxx4','X11', 'Xt'])  # Add NetCDF C++ library
install_vdb = False
if Dir('#') == Dir('.') :
  install_vdb = True

env = Environment(GLOBAL_TOOLS = [Vardb])

SConscript('tool_vardb.py')

variables = env.GlobalVariables()
variables.Update(env)
Help(variables.GenerateHelpText(env))
