/*
-------------------------------------------------------------------------
OBJECT NAME:	init.c

FULL NAME:	Initialize Global Variables

ENTRY POINTS:	Initialize()

STATIC FNS:	none

DESCRIPTION:

COPYRIGHT:	University Corporation for Atmospheric Research, 1993-2006
-------------------------------------------------------------------------
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

// Fix deprecated 'register' keyword used in Motif.
#define register

#include <Xm/List.h>

#include "define.h"

char	buffer[1024], FileName[1024], *ProjectDirectory;

extern "C" {
void ShowError(char msg[]);
void WarnUser(char msg[], void (*okCB)(Widget, XtPointer, XtPointer), void (*cancelCB)(Widget, XtPointer, XtPointer));

const char * getAircraftName(int num);
}

static bool fileExists(const char *path)
{
  struct stat st;
  return (stat(path, &st) == 0);
}

/* -------------------------------------------------------------------- */
void Initialize(int argc, char *argv[])
{
  if (argc < 2)
    {
    fprintf(stderr, "Usage: vared var_db_file\n");
    exit(1);
    }

  int	pnum = atoi(argv[1]);

  ProjectDirectory = (char *)getenv("PROJ_DIR");


  if (pnum > 99)
    {
    /* Try vardb.xml first, fall back to binary VarDB. */
    snprintf(FileName, 1024, "%s/%d/%s/vardb.xml",
      ProjectDirectory, pnum, getAircraftName(pnum));

    if (!fileExists(FileName))
      snprintf(FileName, 1024, "%s/%d/%s/VarDB",
        ProjectDirectory, pnum, getAircraftName(pnum));
    }
  else
    strcpy(FileName, argv[1]);

  OpenNewFile_OK(NULL, NULL, NULL);

}	/* END INITIALIZE */

/* END INIT.C */
