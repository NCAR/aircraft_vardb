/*
 * main.cc — Qt5 entry point for vared (Variable Database Editor).
 *
 * Mirrors the Initialize() logic from initv.cc:
 *   argc == 1          → print usage and exit
 *   argc > 1, numeric  → resolve path from $PROJ_DIR + aircraft name
 *   argc > 1, string   → treat argv[1] as a direct file path
 */

#include <QApplication>
#include <QMessageBox>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "VaredMainWindow.h"

extern "C" {
const char* getAircraftName(int num);
}

static bool fileExists(const char* path)
{
    struct stat st;
    return (stat(path, &st) == 0);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: vared_qt <var_db_file> [DependTable]\n");
        return 1;
    }

    /* Resolve the file path (mirrors initv.cc Initialize()) */
    char fileName[1024];
    int pnum = atoi(argv[1]);
    const char* projDir = getenv("PROJ_DIR");

    if (pnum > 99) {
        if (!projDir) {
            fprintf(stderr, "PROJ_DIR not set.\n");
            return 1;
        }
        /* Try XML first, fall back to binary VarDB */
        snprintf(fileName, sizeof(fileName), "%s/%d/%s/vardb.xml",
                 projDir, pnum, getAircraftName(pnum));
        if (!fileExists(fileName))
            snprintf(fileName, sizeof(fileName), "%s/%d/%s/VarDB",
                     projDir, pnum, getAircraftName(pnum));
    } else {
        strncpy(fileName, argv[1], sizeof(fileName) - 1);
        fileName[sizeof(fileName) - 1] = '\0';
    }

    VaredMainWindow win;
    win.show();
    win.loadFile(QString::fromLocal8Bit(fileName));

    if (argc >= 3)
        win.importDependTable(QString::fromLocal8Bit(argv[2]));

    return app.exec();
}
