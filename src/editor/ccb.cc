/*
-------------------------------------------------------------------------
OBJECT NAME:	ccb.c

FULL NAME:	Motif Command Callbacks

ENTRY POINTS:	Quit()
		OpenNewFile()
		SaveFile()
		SaveFileAs()
		EditVariable()
		Accept()
		Delete()
		SetCategory()

STATIC FNS:	none

DESCRIPTION:

COPYRIGHT:	University Corporation for Atmospheric Research, 1993-2011
-------------------------------------------------------------------------
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

// Fix deprecated 'register' keyword used in Motif.
#define register

#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "define.h"
#include <raf/vardb.hh>
#include <raf/VarDBConverter.hh>

static int	ChangesMade = FALSE, currentCategory = 0, currentStdName = 0;

static VDBFile vdbFile;
static VarDBConverter vdbConverter;
static std::vector<VDBVar*> sortedVars;
static std::vector<std::string> categoryNames;
static std::vector<std::string> stdNames;
static std::string xmlSavePath;
static std::string binarySavePath;  // non-empty when opened from a binary file

extern Widget	catXx, stdNameXx, catMenu, stdNameMenu, list, referenceButton,
		EFtext[], analogButton;
extern char	buffer[], FileName[], *ProjectDirectory;

extern "C" {
char *strupr(char *);
void ShowError(const char msg[]), FileCancel(Widget, XtPointer, XtPointer);
void QueryFile(const char *, const char *, XtCallbackProc), ExtractFileName(XmString, char **);
void WarnUser(const char msg[], XtCallbackProc, XtCallbackProc);
}


/* Helper: rebuild sorted variable list from VDBFile */
static void rebuildSortedVars()
{
  sortedVars.clear();
  for (int i = 0; i < vdbFile.num_vars(); ++i)
    sortedVars.push_back(vdbFile.get_var(i));

  std::sort(sortedVars.begin(), sortedVars.end(),
    [](VDBVar* a, VDBVar* b) { return a->name() < b->name(); });
}


/* Helper: refresh the Motif list widget from sortedVars */
static void refreshList()
{
  XmListDeleteAllItems(list);
  for (size_t i = 0; i < sortedVars.size(); ++i)
  {
    XmString name = XmStringCreateLocalized(
      const_cast<char*>(sortedVars[i]->name().c_str()));
    XmListAddItem(list, name, 0);
    XmStringFree(name);
  }
}


/* Helper: parse "lower upper" space-separated string into two values */
static void parsePair(const std::string& str, std::string& lo, std::string& hi)
{
  lo.clear();
  hi.clear();
  std::istringstream iss(str);
  iss >> lo >> hi;
}


/* Helper: find index of name in vector, return 0 if not found */
static int findIndex(const std::vector<std::string>& vec, const std::string& name)
{
  for (size_t i = 0; i < vec.size(); ++i)
  {
    if (vec[i] == name)
      return (int)i;
  }
  return 0;
}


/* -------------------------------------------------------------------- */
void Quit(Widget w, XtPointer client, XtPointer call)
{
  if (ChangesMade)
    {
    WarnUser((char *)"You have not saved this file.", (XtCallbackProc)exit, NULL);
    return;
    }

  exit(0);

}	/* END QUIT */

/* -------------------------------------------------------------------- */
void SetCategory(Widget w, XtPointer client, XtPointer call)
{
  Arg		args[2];
  XmString	name;

  currentCategory = (long)client;
  if (currentCategory < 0 || currentCategory >= (int)categoryNames.size())
    currentCategory = 0;

  if (!w)	/* If this is being called from EditVariable()	*/
    {
    if (!categoryNames.empty())
      {
      name = XmStringCreateLocalized(
        const_cast<char*>(categoryNames[currentCategory].c_str()));
      XtSetArg(args[0], XmNlabelString, name);
      XtSetValues(XmOptionButtonGadget(catXx), args, 1);
      XmStringFree(name);
      }
    }

}	/* END SETCATEGORY */

/* -------------------------------------------------------------------- */
void SetStandardName(Widget w, XtPointer client, XtPointer call)
{
  Arg		args[2];
  XmString	name;

  currentStdName = (long)client;
  if (currentStdName < 0 || currentStdName >= (int)stdNames.size())
    currentStdName = 0;

  if (!w)	/* If this is being called from EditVariable()	*/
    {
    if (!stdNames.empty())
      {
      name = XmStringCreateLocalized(
        const_cast<char*>(stdNames[currentStdName].c_str()));
      XtSetArg(args[0], XmNlabelString, name);
      XtSetValues(XmOptionButtonGadget(stdNameXx), args, 1);
      XmStringFree(name);
      }
    }

}	/* END SETSTANDARDNAME */

/* -------------------------------------------------------------------- */
void OpenNewFile_OK(Widget w, XtPointer client, XmFileSelectionBoxCallbackStruct *call)
{
  int		i, n;
  char		*file;
  Arg		args[8];
  XmString	name;

  static Widget	catButtons[128];
  static Widget	stdNameButtons[512];
  static int	nCatButtons = 0;
  static int	nStdNameButtons = 0;


  if (w)
    {
    ExtractFileName(call->value, &file);
    FileCancel((Widget)NULL, (XtPointer)NULL, (XtPointer)NULL);

    strcpy(FileName, file);
    }


  /* Open the VarDB file (XML or binary with auto-conversion).
   */
  vdbConverter.open(&vdbFile, FileName);

  if (!vdbFile.is_valid())
    {
    fprintf(stderr, "Can't initialize variable database %s.\n", FileName);
    exit(1);
    }

  /* Determine save paths.  XML is always written to a .xml path.  When
   * opened from a binary file, the original binary path is also tracked
   * so SaveFile() can keep both in sync.
   */
  std::string path(FileName);
  if (path.length() >= 4 && path.substr(path.length() - 4) == ".xml")
  {
    xmlSavePath   = path;
    binarySavePath.clear();
  }
  else
  {
    xmlSavePath    = vdbConverter.defaultOutputPath();
    binarySavePath = path;
  }

  /* Build sorted variable list and populate Motif list widget. */
  rebuildSortedVars();
  refreshList();


  /* Remove old category buttons and build new ones from XML.
   */
  for (i = 0; i < nCatButtons; ++i)
    {
    XtUnmanageChild(catButtons[i]);
    XtDestroyWidget(catButtons[i]);
    }

  categoryNames = vdbFile.get_categories();
  /* Ensure "None" is at the front as the default. */
  if (categoryNames.empty() ||
      categoryNames[0] != "None")
    {
    categoryNames.insert(categoryNames.begin(), "None");
    }

  for (i = 0; i < (int)categoryNames.size(); ++i)
    {
    name = XmStringCreateLocalized(
      const_cast<char*>(categoryNames[i].c_str()));

    n = 0;
    XtSetArg(args[n], XmNlabelString, name); ++n;
    catButtons[i] = XmCreatePushButton(catMenu, (char *)"opMenB", args, n);
    XtAddCallback(catButtons[i], XmNactivateCallback, SetCategory, (XtPointer)(long)i);

    XmStringFree(name);
    }

  nCatButtons = i;
  XtManageChildren(catButtons, nCatButtons);


  /* Remove old standard name buttons and build new ones from XML.
   */
  for (i = 0; i < nStdNameButtons; ++i)
    {
    XtUnmanageChild(stdNameButtons[i]);
    XtDestroyWidget(stdNameButtons[i]);
    }

  stdNames = vdbFile.get_standard_names();
  /* Ensure "None" is at the front as the default. */
  if (stdNames.empty() ||
      stdNames[0] != "None")
    {
    stdNames.insert(stdNames.begin(), "None");
    }

  for (i = 0; i < (int)stdNames.size(); ++i)
    {
    name = XmStringCreateLocalized(
      const_cast<char*>(stdNames[i].c_str()));

    n = 0;
    XtSetArg(args[n], XmNlabelString, name); ++n;
    stdNameButtons[i] = XmCreatePushButton(stdNameMenu, (char *)"opMenB", args, n);
    XtAddCallback(stdNameButtons[i], XmNactivateCallback, SetStandardName, (XtPointer)(long)i);

    XmStringFree(name);
    }

  nStdNameButtons = i;
  XtManageChildren(stdNameButtons, nStdNameButtons);

  ChangesMade = FALSE;
  currentCategory = 0;
  currentStdName = 0;

}	/* END OPENNEWFILE_OK */

/* -------------------------------------------------------------------- */
void OpenNewFile(Widget w, XtPointer client, XtPointer call)
{
  snprintf(buffer, 1024, "%s/*", ProjectDirectory);
  QueryFile((char *)"Enter file name to load:", buffer, (XtCallbackProc)OpenNewFile_OK);

}	/* END OPENNEWFILE */

/* -------------------------------------------------------------------- */
void SaveFileAs_OK(Widget w, XtPointer client, XmFileSelectionBoxCallbackStruct *call)
{
  char	*file;

  ExtractFileName(call->value, &file);
  FileCancel((Widget)NULL, (XtPointer)NULL, (XtPointer)NULL);

  try
    {
    vdbFile.save(std::string(file));
    xmlSavePath = file;
    ChangesMade = FALSE;
    }
  catch (...)
    {
    ShowError((char *)"Error trying to save, aborted.");
    }

}	/* END SAVEFILEAS_OK */

/* -------------------------------------------------------------------- */
void SaveFileAs(Widget w, XtPointer client, XtPointer call)
{
  strncpy(buffer, xmlSavePath.c_str(), 1024);
  buffer[1023] = '\0';
  QueryFile((char *)"Save as:", buffer, (XtCallbackProc)SaveFileAs_OK);

}	/* END SAVEFILEAS */

/* -------------------------------------------------------------------- */
void SaveFile(Widget w, XtPointer client, XtPointer call)
{
  try
    {
    vdbFile.save(xmlSavePath);
    }
  catch (...)
    {
    ShowError((char *)"Error saving XML, aborted.");
    return;
    }

  /* When the session originated from a binary file, keep it in sync. */
  if (!binarySavePath.empty())
    {
    if (vdbConverter.saveAsBinary(&vdbFile, binarySavePath) == ERR)
      {
      ShowError((char *)"Error saving binary VarDB, aborted.");
      return;
      }
    }

  ChangesMade = FALSE;

}	/* END SAVEFILE */

/* -------------------------------------------------------------------- */
void EditVariable(Widget w, XtPointer client, XmListCallbackStruct *call)
{
  int		pos, *pos_list, pcnt;

  if (XmListGetSelectedPos(list, &pos_list, &pcnt) == FALSE)
    return;

  pos = pos_list[0] - 1;

  if (pos < 0 || pos >= (int)sortedVars.size())
    return;

  VDBVar* var = sortedVars[pos];

  XmTextFieldSetString(EFtext[0],
    const_cast<char*>(var->name().c_str()));
  XmTextFieldSetString(EFtext[1],
    const_cast<char*>(var->get_attribute(VDBVar::LONG_NAME).c_str()));
  XmTextFieldSetString(EFtext[2],
    const_cast<char*>(var->get_attribute(VDBVar::UNITS).c_str()));
  XmTextFieldSetString(EFtext[3],
    const_cast<char*>(var->get_attribute(VDBVar::ALTERNATE_UNITS).c_str()));

  /* Voltage range: stored as "lower upper" string in XML */
  std::string vrLo, vrHi;
  parsePair(var->get_attribute(VDBVar::VOLTAGE_RANGE), vrLo, vrHi);
  XmTextFieldSetString(EFtext[4], const_cast<char*>(vrLo.c_str()));
  XmTextFieldSetString(EFtext[5], const_cast<char*>(vrHi.c_str()));

  XmTextFieldSetString(EFtext[6],
    const_cast<char*>(var->get_attribute(VDBVar::DEFAULT_SAMPLE_RATE).c_str()));

  XmTextFieldSetString(EFtext[7],
    const_cast<char*>(var->get_attribute(VDBVar::MIN_LIMIT).c_str()));
  XmTextFieldSetString(EFtext[8],
    const_cast<char*>(var->get_attribute(VDBVar::MAX_LIMIT).c_str()));

  /* Cal range: stored as "lower upper" string in XML */
  std::string crLo, crHi;
  parsePair(var->get_attribute(VDBVar::CAL_RANGE), crLo, crHi);
  XmTextFieldSetString(EFtext[9], const_cast<char*>(crLo.c_str()));
  XmTextFieldSetString(EFtext[10], const_cast<char*>(crHi.c_str()));

  /* Is Analog toggle */
  bool isAnalog = var->get_attribute_value<bool>(VDBVar::IS_ANALOG);
  XmToggleButtonSetState(analogButton, isAnalog, false);

  /* Category dropdown */
  std::string catName = var->get_attribute(VDBVar::CATEGORY);
  SetCategory(NULL, (XtPointer)(long)findIndex(categoryNames, catName), NULL);

  /* Standard name dropdown */
  std::string stdName = var->get_attribute(VDBVar::STANDARD_NAME);
  SetStandardName(NULL, (XtPointer)(long)findIndex(stdNames, stdName), NULL);

  /* Reference toggle */
  bool isRef = var->get_attribute_value<bool>(VDBVar::REFERENCE);
  XmToggleButtonSetState(referenceButton, isRef, False);

}	/* END EDITVARIABLE */

/* -------------------------------------------------------------------- */
void Accept(Widget w, XtPointer client, XtPointer call)
{
  char		*p;
  int		firstVisPos;
  Arg		args[5];


  p = XmTextFieldGetString(EFtext[0]);
  strupr(p);
  XmTextFieldSetString(EFtext[0], p);
  std::string varName(p);
  XtFree(p);

  int i = 0;
  XtSetArg(args[i], XmNtopItemPosition, &firstVisPos); ++i;
  XtGetValues(list, args, i);

  /* Get or create the variable in the VDBFile. */
  VDBVar* var = vdbFile.get_var(varName);
  if (!var)
    var = vdbFile.add_var(varName);

  /* Set all attributes from the edit fields. */
  p = XmTextFieldGetString(EFtext[1]);
  var->set_attribute(VDBVar::LONG_NAME, std::string(p));
  XtFree(p);

  p = XmTextFieldGetString(EFtext[2]);
  var->set_attribute(VDBVar::UNITS, std::string(p));
  XtFree(p);

  p = XmTextFieldGetString(EFtext[3]);
  if (strlen(p) > 0)
    var->set_attribute(VDBVar::ALTERNATE_UNITS, std::string(p));
  XtFree(p);

  /* Is Analog */
  bool isAnalog = XmToggleButtonGetState(analogButton);
  var->set_attribute(VDBVar::IS_ANALOG, isAnalog);

  /* Voltage Range: compose "lower upper" string */
  char *vrLo = XmTextFieldGetString(EFtext[4]);
  char *vrHi = XmTextFieldGetString(EFtext[5]);
  if (isAnalog && (strlen(vrLo) > 0 || strlen(vrHi) > 0))
    {
    std::string vr = std::string(vrLo) + " " + std::string(vrHi);
    var->set_attribute(VDBVar::VOLTAGE_RANGE, vr);
    }
  XtFree(vrLo);
  XtFree(vrHi);

  /* Default Sample Rate */
  p = XmTextFieldGetString(EFtext[6]);
  if (isAnalog && strlen(p) > 0)
    var->set_attribute(VDBVar::DEFAULT_SAMPLE_RATE, std::string(p));
  XtFree(p);

  /* Min/Max Limits */
  p = XmTextFieldGetString(EFtext[7]);
  if (strlen(p) > 0)
    var->set_attribute(VDBVar::MIN_LIMIT, std::string(p));
  XtFree(p);

  p = XmTextFieldGetString(EFtext[8]);
  if (strlen(p) > 0)
    var->set_attribute(VDBVar::MAX_LIMIT, std::string(p));
  XtFree(p);

  /* Cal Range: compose "lower upper" string */
  char *crLo = XmTextFieldGetString(EFtext[9]);
  char *crHi = XmTextFieldGetString(EFtext[10]);
  if (strlen(crLo) > 0 || strlen(crHi) > 0)
    {
    std::string cr = std::string(crLo) + " " + std::string(crHi);
    var->set_attribute(VDBVar::CAL_RANGE, cr);
    }
  XtFree(crLo);
  XtFree(crHi);

  /* Category: stored by name */
  if (currentCategory >= 0 && currentCategory < (int)categoryNames.size()
      && categoryNames[currentCategory] != "None")
    var->set_attribute(VDBVar::CATEGORY, categoryNames[currentCategory]);

  /* Standard Name: stored by name */
  if (currentStdName >= 0 && currentStdName < (int)stdNames.size()
      && stdNames[currentStdName] != "None")
    var->set_attribute(VDBVar::STANDARD_NAME, stdNames[currentStdName]);

  /* Reference */
  var->set_attribute(VDBVar::REFERENCE,
    bool(XmToggleButtonGetState(referenceButton)));

  /* Rebuild sorted list and refresh display. */
  rebuildSortedVars();
  refreshList();

  /* Find the variable's position in the sorted list and select it. */
  int selectPos = 0;
  for (size_t j = 0; j < sortedVars.size(); ++j)
    {
    if (sortedVars[j]->name() == varName)
      {
      selectPos = (int)j + 1;
      break;
      }
    }

  XmListSetPos(list, firstVisPos);
  if (selectPos > 0)
    XmListSelectPos(list, selectPos, FALSE);

  ChangesMade = TRUE;

}	/* END ACCEPT */

/* -------------------------------------------------------------------- */
void Clear(Widget w, XtPointer client, XtPointer call)
{
  int	i;

  for (i = 0; i < 11; ++i)
    XmTextFieldSetString(EFtext[i], (char *)"");

  XmToggleButtonSetState(analogButton, False, False);
  SetCategory(NULL, 0, NULL);
  SetStandardName(NULL, 0, NULL);
  XmToggleButtonSetState(referenceButton, False, False);

}	/* END CLEAR */

/* -------------------------------------------------------------------- */
void Delete(Widget w, XtPointer client, XtPointer call)
{
  int	*pos_list, pcnt;

  if (XmListGetSelectedPos(list, &pos_list, &pcnt) == FALSE)
    {
    ShowError((char *)"No variable selected to delete.");
    return;
    }

  int pos = pos_list[0] - 1;

  if (pos < 0 || pos >= (int)sortedVars.size())
    return;

  vdbFile.remove_var(sortedVars[pos]->name());

  rebuildSortedVars();
  refreshList();

  ChangesMade = TRUE;

}	/* END DELETE */

/* END CCB.C */
