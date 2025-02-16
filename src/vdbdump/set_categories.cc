/*
-------------------------------------------------------------------------
OBJECT NAME:	set_categories.c

FULL NAME:	VarDB dump w/ Categories

DESCRIPTION:	Update categories.

COPYRIGHT:	University Corporation for Atmospheric Research, 2025

COMPILE:	g++ -I../vardb/raf -I../../raf set_categories.cc -L../vardb -L../../raf -lVarDB -lraf -lnetcdf -o set_categories
-------------------------------------------------------------------------
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>

#include <vardb.h>
#include <portable.h>

extern long	VarDB_nRecords;

/* -------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
  int i;

  if (argc != 2)
  {
    fprintf(stderr, "Usage: catdump [proj_num | VarDB_filename]\n");
    exit(1);
  }

  if (InitializeVarDB(argv[1]) == ERR)
  {
    fprintf(stderr, "catdump: Initialize failure.\n");
    exit(1);
  }


  printf("Version %d, with %d records.\n", ntohl(master_VarDB_Hdr.Version),
						VarDB_nRecords);

  for (i = 0; i < VarDB_nRecords; ++i)
  {
    // Move old Aerosol to new Aerosol
    if (ntohl(((struct var_v2 *)VarDB)[i].Category) == 13)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 5);

    // NavAttitide
    if (strstr(((struct var_v2 *)VarDB)[i].Name, "PITCH") ||
        strstr(((struct var_v2 *)VarDB)[i].Name, "ROLL") ||
        strstr(((struct var_v2 *)VarDB)[i].Name, "DIFR") ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "DRFTA") == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "BL", 2) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "AK", 2) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "SS", 2) == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "ATTACK") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "BNORMA") == 0 ||
        strstr(((struct var_v2 *)VarDB)[i].Name, "YAW"))
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 13);



    // Cloud
    if (strncmp(((struct var_v2 *)VarDB)[i].Name, "XG", 2) == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "NLWCC") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "NTWCC") == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "PLWCC", 5) == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "RICE") == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 8);

    // NavPosition
    if (strncmp(((struct var_v2 *)VarDB)[i].Name, "ALT", 3) == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 1);

    // NavVelocity
    if (strcmp(((struct var_v2 *)VarDB)[i].Name, "VZI") == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "TAS", 3) == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "XMACH2") == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 3);

    // Chemistry
    if (strncmp(((struct var_v2 *)VarDB)[i].Name, "XWL", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "AO2", 3) == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 10);

    // Atmos State
    if (strcmp(((struct var_v2 *)VarDB)[i].Name, "PSC") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "OAT") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "ATLH") == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 4);

    // Raw uncorr
    if (strcmp(((struct var_v2 *)VarDB)[i].Name, "XPSFD") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "PSFRD") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "CORAW") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "NO") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "FO3") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "NOY") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "SO2") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "FCN") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "TEO3") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "CORAW") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "PSLH") == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 6);

    // Housekeeping
    if (strstr(((struct var_v2 *)VarDB)[i].Name, "CAV") ||
        strstr(((struct var_v2 *)VarDB)[i].Name, "TIME") ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "XUV", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "2D", 2) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "HV", 2) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "H2D", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "THIM", 4) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "TCAB", 4) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "FCN", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "MED", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "PER", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "SDW", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "SHDOR", 5) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "SPB", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "SPT", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "XAE", 3) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "XCELL", 5) == 0 ||
        strncmp(((struct var_v2 *)VarDB)[i].Name, "INLET", 5) == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "AO2STAT") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "NSYS") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "COZRO") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "TASFLG") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "DPB") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "DPT") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "DPL") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "TEP") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "TET") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "DPR") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "XTUBETEMP") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "HOUR") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "MINUTE") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "SECOND") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "YEAR") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "MONTH") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "DAY") == 0 ||
        strcmp(((struct var_v2 *)VarDB)[i].Name, "XSIGV") == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 9);


    // None
    if (strncmp(((struct var_v2 *)VarDB)[i].Name, "DIFF", 4) == 0)
      VarDB_SetCategory(((struct var_v2 *)VarDB)[i].Name, 0);

    printf("%-12.12s %-40.40s %-15.15s %d %s\n",
		((struct var_v2 *)VarDB)[i].Name,
		((struct var_v2 *)VarDB)[i].Title,
		VarDB_GetCategoryName(((struct var_v2 *)VarDB)[i].Name),
		VarDB_GetStandardName(((struct var_v2 *)VarDB)[i].Name),
		VarDB_GetStandardNameName(((struct var_v2 *)VarDB)[i].Name));
  }

  SaveVarDB(argv[1]);
  ReleaseVarDB();

  return(0);

}
