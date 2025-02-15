/*
-------------------------------------------------------------------------
OBJECT NAME:	vdbdump.cc

FULL NAME:	VarDB dump

DESCRIPTION:	This program does a basic text dump of a Variable DataBase
		file.

COPYRIGHT:	University Corporation for Atmospheric Research, 1993-2014
-------------------------------------------------------------------------
*/

#include <iostream>
#include <vector>
#include <unistd.h>	//for getopt
#include <iomanip>	// for setw

#include <raf/vardb.hh>
#include <raf/VarDBConverter.hh>

bool tracker = false;

using namespace std;


/* -------------------------------------------------------------------- */
void print_usage()
{
    cout << "Usage: vdbdump [options] [proj_num | VarDB_filename]\n"
              << "Options:\n"
              << "  -h            Display this message\n"
              << "  -a            Disabled legacy: stream without newlines, different formatting \n"
              << "  -c            Category\n"
              << "  -u            Units\n"
              << "  -l            Long_name\n";
}

/* -------------------------------------------------------------------- */
int
main(int argc, char *argv[])
{
  char opt;
  vector<string> attribute_names;
  vector<int> width;
  vector<int> substring_length;
  bool user_format_specified = false;

  if (argc < 2)
  {
    print_usage();
    return(1);
  }


  while ((opt = getopt(argc, argv, "aulch"))!= -1) {
    switch (opt) {
      case 'a':
        //tracker = true;
        //i = 1;
        // uncertain what this is for
        // breaking to find use cases
        cout << "Unknown use for -a: please revise" << endl;
        return(1);
        break;
      case 'c':
        width.push_back(12);
        substring_length.push_back(12);
        attribute_names.push_back("category");
        user_format_specified = true;
        break;
      case 'u':
        width.push_back(12);
        substring_length.push_back(12);
        attribute_names.push_back("units");
        user_format_specified = true;
        break;
      case 'l':
        width.push_back(50);
        substring_length.push_back(50);
        attribute_names.push_back("long_name");
        user_format_specified = true;
      case 'h':
        print_usage();
      default:
        cout<<"\n default case" << endl;
        //print_usage();
    }

    if (optind >= argc) {
      cerr << "Expected argument after options" << endl;
      return(1);
    }
  }

  if (!user_format_specified)
  {
    // If no output options default to name, units, long_name
    width.push_back(12);
    substring_length.push_back(12);
    attribute_names.push_back("units");
    width.push_back(50);
    substring_length.push_back(50);
    attribute_names.push_back("long_name");
  }

  VDBFile file;
  VarDBConverter vdbc;
  string substring;

  vdbc.open(&file, argv[optind]);
  if (file.is_valid() == false)
  {
    cerr << "vdbdump: Initialize failure.\n";
    return(1);
  }

  // print header
  cout << left << setw(12) << "name ";
  for (size_t k = 0; k < attribute_names.size(); ++k) {
    cout << left << setw(width[k]) << attribute_names[k] << " " ;
  }
  cout << endl;

  for (int i = 0; i < file.num_vars(); ++i)
  {
    VDBVar *var = file.get_var(i);

    cout << left << setw(12) << var->name();
    for (size_t j = 0; j < attribute_names.size(); ++j) {
      substring = var->get_attribute(attribute_names[j]).substr(0,substring_length[j]);
      cout << left << setw(width[j]) << substring << " ";
    }
    cout << endl;
  }
  return(0);
}
