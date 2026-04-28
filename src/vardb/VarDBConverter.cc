

#include "raf/VarDBConverter.hh"
#include <cstdlib>
#include <cstring>
#include <raf/portable.h>  // for ntohf()
#include <fstream>
#include <sstream>

#include "raf/vardb.h"

using std::string;
using std::ostringstream;
using std::cerr;
using std::endl;

static const char COMMENT = '#';
static const float fill_value = -32767.0;

extern long	VarDB_nRecords;



std::string
VarDBConverter::
defaultOutputPath()
{
  return projDirPath("vardb.xml");
}


void
VarDBConverter::
setupProjDir(const std::string& vardbarg)
{
  string dbdir(vardbarg);

  if (dbdir.find('/') != string::npos)
  {
    dbdir.erase(dbdir.rfind('/')+1, dbdir.length());
    projDir = dbdir;
  }
  // Initialize the global project configuration directory also.
  defaultProjDirPath("");
}


string
VarDBConverter::
defaultProjDirPath(const std::string& filename)
{
  if (defaultProjDir.empty())
  {
    const char* projdir = getenv("PROJ_DIR");
    if (projdir)
    {
      defaultProjDir = string(projdir) + "/Configuration/";
    }
  }
  string path = defaultProjDir + filename;
  if (filename.length())
  {
    cerr << "Looking for " << filename << " at path " << path << "\n";
  }
  return path;
}


string
VarDBConverter::
projDirPath(const std::string& filename)
{
  return projDir + filename;
}



void
VarDBConverter::
checkModVars(VDBFile& vdb)
{
  char buffer[BUFSIZ+1];
  char name[100];
  string path = defaultProjDirPath("ModVars");

  FILE *fp;
  if ((fp = fopen(path.c_str(), "r")) == NULL)
  {
    return;
  }

  float vals[2];
  while (fgets(buffer, BUFSIZ, fp))
  {
    if (buffer[0] == COMMENT)
      continue;

    sscanf(buffer, "%s %f %f", name, &vals[0], &vals[1]);

    VDBVar* var = vdb.get_var(name);
    if (var)
    {
      ostringstream oss;
      oss << vals[0] << " " << vals[1];
      var->set_attribute(VDBVar::MODULUS_RANGE, oss.str());
    }
  }
  fclose(fp);
}


void
VarDBConverter::
checkDerivedNames(VDBFile& vdb)
{
  string path = defaultProjDirPath("DerivedNames");
  char buffer[BUFSIZ];

  FILE *fp;
  if ((fp = fopen(path.c_str(), "r")) == NULL)
  {
    return;
  }

  while (fgets(buffer, BUFSIZ, fp))
  {
    if (buffer[0] == COMMENT)
      continue;

    char* p = strtok(buffer, " \t");

    VDBVar* var = vdb.get_var(p);
    if (var)
    {
      p = strtok(NULL, "\n");
      while (isspace(*p))
      {
	++p;
      }
      var->set_attribute(VDBVar::DERIVE, p);
    }
  }

  fclose(fp);
}


void
VarDBConverter::
checkDependencies(VDBFile& vdb)
{
  char buffer[BUFSIZ], *p;

  string path = projDirPath("DependTable");

  FILE *fp;
  if ((fp = fopen(path.c_str(), "r")) == NULL)
  {
    return;
  }

  while (fgets(buffer, BUFSIZ, fp))
  {
    if (buffer[0] == COMMENT)
      continue;

    p = strtok(buffer, " \t\n");

    if (!p)
    {
      continue;
    }
    VDBVar* var = vdb.get_var(p);
    if (var)
    {
      if ( (p = strtok(NULL, "\n")) )
      {
	if (var->get_attribute(VDBVar::DEPENDENCIES).size())
	{
	  // There is more than one entry in the DependTable
          cerr << "Multiple entries for " << var->name()
	       << " in DependTable found, repair.  Fatal.\n";
          exit(1);
        }
        while (isspace(*p))
	{
	  ++p;
	}
	var->set_attribute(VDBVar::DEPENDENCIES, p);
      }
    }
  }
  fclose(fp);
}



VDBFile*
VarDBConverter::
open(VDBFile* vdb_in, const std::string& path)
{
  VDBFile& vdb = *vdb_in;

  if (path.rfind(".xml") == path.length() - 4)
  {
    cerr << "Opening " << path << " directly as XML..." << endl;
    vdb.open(path);
    return vdb_in;
  }

  // Initialize the project directory to be the directory containing the
  // vardb file in path.
  setupProjDir(path);

  if (defaultProjDir.empty())
  {
    cerr << "PROJ_DIR is not set, cannot convert to XML." << endl;
    return vdb_in;
  }

  vdb.create();

  // First categories and standard names.
  VDBFile::categories_type categories;
  std::string cpath = projDirPath("Categories");
  cerr << "Loading categories from " << cpath << "\n";
  categories = VDBFile::readCategories(cpath);
  vdb.set_categories(categories);

  std::string snamespath = projDirPath("StandardNames");
  VDBFile::standard_names_type standard_names;
  cerr << "Loading standard names from " << snamespath << "\n";
  standard_names = VDBFile::readStandardNames(snamespath);
  vdb.set_standard_names(standard_names);

  // Finally variables themselves, retrieved through the original 
  // VarDB interface.
  if (InitializeVarDB(path.c_str()) == ERR)
  {
    cerr << "VarDBConverter: Initialize failure." << endl;
    vdb.close();
    return vdb_in;
  }

  // Use the public getter API rather than touching var_v2 struct members
  // directly.  This isolates this code from struct layout changes: only
  // vardb.h and the getter/setter implementations need updating when the
  // binary format evolves — not every call site.
  //
  // Trade-off: each getter calls VarDB_lookup() which is a linear scan by
  // name, making this loop O(n^2) in the number of records.  For a one-time
  // file conversion of ~800 records this is acceptable (~2-5ms).  If a hot
  // path ever needs this, add index-based getter variants instead.
  int i;
  for (i = 0; i < VarDB_nRecords; ++i)
  {
    const char *name = VarDB_GetNameAt(i);
    if (!name || isdigit(name[0]))
      continue;

    VDBVar* var = vdb.add_var(name);
    var->set_attribute(VDBVar::UNITS,     string(VarDB_GetUnits(name)));
    var->set_attribute(VDBVar::LONG_NAME, string(VarDB_GetTitle(name)));

    const char *altUnits = VarDB_GetAlternateUnits(name);
    if (altUnits && altUnits[0])
      var->set_attribute(VDBVar::ALTERNATE_UNITS, string(altUnits));

    if (VarDB_isAnalog(name))
    {
      var->set_attribute(VDBVar::IS_ANALOG, true);
      ostringstream oss;
      oss << VarDB_GetVoltageRangeLower(name) << " "
          << VarDB_GetVoltageRangeUpper(name);
      var->set_attribute(VDBVar::VOLTAGE_RANGE, oss.str());
      var->set_attribute(VDBVar::DEFAULT_SAMPLE_RATE,
                         VarDB_GetDefaultSampleRate(name));
    }
    else
    {
      var->set_attribute(VDBVar::IS_ANALOG, false);
    }

    float minL = VarDB_GetMinLimit(name);
    float maxL = VarDB_GetMaxLimit(name);
    if (maxL - minL != 0)
    {
      var->set_attribute(VDBVar::MIN_LIMIT, minL);
      var->set_attribute(VDBVar::MAX_LIMIT, maxL);
    }

    float calLo = VarDB_GetCalRangeLower(name);
    float calHi = VarDB_GetCalRangeUpper(name);
    if (calHi - calLo != 0)
    {
      ostringstream oss;
      oss << calLo << " " << calHi;
      var->set_attribute(VDBVar::CAL_RANGE, oss.str());
    }

    string category = VarDB_GetCategoryName(name);
    if (category != "None")
      var->set_attribute(VDBVar::CATEGORY, category);
  }

  checkModVars(vdb);
  checkDerivedNames(vdb);
  checkDependencies(vdb);

  // Set reference and standard_name last, consistent with original ordering.
  for (i = 0; i < VarDB_nRecords; ++i)
  {
    const char *name = VarDB_GetNameAt(i);
    if (!name || isdigit(name[0]))
      continue;

    VDBVar* var = vdb.get_var(name);
    string standard_name = VarDB_GetStandardNameName(name);
    if (standard_name != "None")
      var->set_attribute(VDBVar::STANDARD_NAME, standard_name);

    var->set_attribute(VDBVar::REFERENCE, bool(VarDB_GetReference(name) != 0));
  }

  // What about Spikes & Lags?  Put in aircraft specific?
  // What about default global_attrs; coordinates, etc.

  // Done with the binary variable database.
  ReleaseVarDB();
  return vdb_in;
}


int
VarDBConverter::
saveAsBinary(VDBFile* vdb_in, const std::string& path)
{
  // Round-trip an in-memory VDBFile back to the binary var_v2 format.
  //
  // Strategy: use InitializeEmptyVarDB() + VarDB_AddVar() + setters to
  // populate the global binary state via the public C API, then call
  // SaveVarDB().  This keeps var_v2 struct manipulation contained in the
  // C library; VarDBConverter never touches the struct directly.
  //
  // The Categories and StandardNames files must be reachable (projDir must
  // be set) so that VarDB_GetCategoryNumber() and
  // VarDB_GetStandardNameNumber() can resolve string → index.  Both are
  // loaded lazily on first call via their internal firstTime flag.
  //
  // XML-only attributes (DERIVE, DEPENDENCIES, MODULUS_RANGE) have no
  // binary field and are silently dropped — consistent with the original
  // binary format's capabilities.

  if (!vdb_in || !vdb_in->is_valid())
  {
    cerr << "saveAsBinary: invalid VDBFile\n";
    return(ERR);
  }

  // Point the C category/standard-name readers at the right directory.
  string catPath = projDirPath("Categories");
  SetCategoryFileName(catPath.c_str());
  string stdPath = projDirPath("StandardNames");
  SetStandardNameFileName(stdPath.c_str());

  if (InitializeEmptyVarDB() == ERR)
  {
    cerr << "saveAsBinary: InitializeEmptyVarDB failed\n";
    return(ERR);
  }

  VDBFile& vdb = *vdb_in;
  int nv = vdb.num_vars();

  for (int i = 0; i < nv; ++i)
  {
    VDBVar* var = vdb.get_var(i);
    if (!var)
      continue;

    const string vname = var->name();
    const char  *vn    = vname.c_str();

    int idx = VarDB_AddVar(vn);
    if (idx == ERR)
    {
      cerr << "saveAsBinary: failed to add var " << vname << "\n";
      ReleaseVarDB();
      return(ERR);
    }

    VarDB_SetTitle(vn,         var->get_attribute(VDBVar::LONG_NAME).c_str());
    VarDB_SetUnits(vn,         var->get_attribute(VDBVar::UNITS).c_str());
    VarDB_SetAlternateUnits(vn,var->get_attribute(VDBVar::ALTERNATE_UNITS).c_str());

    bool isAnalog = var->get_attribute_value<bool>(VDBVar::IS_ANALOG);
    VarDB_SetIsAnalog(vn, isAnalog);

    if (isAnalog)
    {
      string vrLo, vrHi;
      std::istringstream vr(var->get_attribute(VDBVar::VOLTAGE_RANGE));
      vr >> vrLo >> vrHi;
      VarDB_SetVoltageRangeLower(vn, vrLo.empty() ? 0 : atoi(vrLo.c_str()));
      VarDB_SetVoltageRangeUpper(vn, vrHi.empty() ? 0 : atoi(vrHi.c_str()));
      VarDB_SetDefaultSampleRate(vn,
        atoi(var->get_attribute(VDBVar::DEFAULT_SAMPLE_RATE).c_str()));
    }

    string minS = var->get_attribute(VDBVar::MIN_LIMIT);
    string maxS = var->get_attribute(VDBVar::MAX_LIMIT);
    if (!minS.empty()) VarDB_SetMinLimit(vn, (float)atof(minS.c_str()));
    if (!maxS.empty()) VarDB_SetMaxLimit(vn, (float)atof(maxS.c_str()));

    string calLo, calHi;
    std::istringstream cr(var->get_attribute(VDBVar::CAL_RANGE));
    cr >> calLo >> calHi;
    if (!calLo.empty()) VarDB_SetCalRangeLower(vn, (float)atof(calLo.c_str()));
    if (!calHi.empty()) VarDB_SetCalRangeUpper(vn, (float)atof(calHi.c_str()));

    string catName = var->get_attribute(VDBVar::CATEGORY);
    VarDB_SetCategory(vn,
      catName.empty() ? 0 : (uint32_t)VarDB_GetCategoryNumber(catName.c_str()));

    string stdName = var->get_attribute(VDBVar::STANDARD_NAME);
    VarDB_SetStandardName(vn,
      stdName.empty() ? 0 : (uint32_t)VarDB_GetStandardNameNumber(stdName.c_str()));

    VarDB_SetReference(vn,
      var->get_attribute_value<bool>(VDBVar::REFERENCE) ? 1 : 0);
  }

  SortVarDB();

  int rc = SaveVarDB(path.c_str());
  ReleaseVarDB();
  return(rc);

}	/* END SAVEASBINARY */


#ifdef notdef
// Copied from vdb2xml when it's functionality was moved into the library.
// Not sure it will ever be needed.
void
update_dictionary()
{
  // We could at this point search for the Dictionary file and insert those
  // lines into the XML file.  However, since the Dictionary is not used
  // anymore (not since the schema was fixed rather than generated), it's
  // not clear how to use it and where it should be maintained.
  fprintf(vdb,"  <Dictionary>\n");
  std::string dictionaryLocation =
    "/home/local/vardb/utils/vdb2xml/Dictionary";
  std::ifstream dictionaryNames;
  dictionaryNames.open(dictionaryLocation.c_str());

  std::string raw_line,named,definition;
  while(std::getline(dictionaryNames,raw_line))
  {
    if (raw_line[0]!='#' and !raw_line.empty())
    {
      named="";
      definition="";
      std::stringstream iss(raw_line);
      std::getline(iss,named,',');
      std::getline(iss,definition,',');
      //std::cout<<named<<":: "<<definition<<"\n";
      fprintf(vdb,"    <definition name=\"%s\">%s</definition>\n",
	      named.c_str(),definition.c_str());
    }
  }
  fprintf(vdb,"  </Dictionary>\n");
}
#endif

