# -*- python -*-
# Sub-SConscript for vared Qt GUI tests.
#
# VaredMainWindow is imported as a pre-built static library (libvaredmw.a)
# exported by src/editor/SConscript.  This avoids recompiling
# VaredMainWindow.cc under a different Qt environment, which would
# cause an SCons "two environments for same target" conflict.

from SCons.Script import Split, Environment, Import

Import('vmw_lib')

test_env = Environment(tools=['default', 'vardb', 'netcdf', 'raf', 'qt5', 'testing'])
test_env.Append(CXXFLAGS=Split('-std=c++17 -Wall -g -O2'))
test_env.Append(CPPPATH=['#/src/editor'])

if vmw_lib and test_env.EnableQtModules(['QtCore', 'QtWidgets', 'QtTest']):
    # Link libvaredmw.a via LIBS (not as a source) so the qt5 automoc emitter
    # never sees it — the emitter only processes C++ source nodes.
    test_env.Append(LIBPATH=['#/src/editor'])
    test_env.Append(LIBS=['varedmw'])

    tg = test_env.Program(
        '#/tests/vared_gui_tests',
        ['#/tests/test_vared_gui.cc'])

    # Ensure libvaredmw.a is built before linking the test binary.
    test_env.Depends(tg, vmw_lib)

    test_env.Alias('guitest',
                   test_env.TestRun('guitest', tg,
                       "cd ${SOURCE.dir} && ./${SOURCE.file}"))
else:
    print("Qt5 not found or vmw_lib unavailable — skipping guitest build")
