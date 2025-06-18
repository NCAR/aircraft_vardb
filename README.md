# aircraft_vardb

A library and collection of programs for manipulating the variable database.  The variable database is a master list of all variables for netCDF output, their units, titles, and other miscellaneous metadata.
The variable database does not use an actual database, but is rather an XML file.

The OAP file format can be found at <http://www.eol.ucar.edu/raf/software/OAPfiles.html>

## Documentation

Directories in the aircraft_vardb library.

| Directory | Description |
| ----------- | ----------------------------------------------------------------------------------------- |
| lib | Library routines to open and manipulate a variable database. |
| editor | This is a Motif based editor for editing variables in a specific file. |
| vdbdump | Utility to produce an ASCII dump of a variable database. |
| vdb2xml | Utility to convert an style binary VarDB to new style vardb.xml. |
| vdb2ncml | Utility to convert an style binary VarDB to netCDF NCML output. |

## Environment

The aircraft_vardb libraries are written in C. The utilities build and run on any Unix/Linux operating system, including Mac OS X.

## Dependencies

To install these programs on your computer, ensure the following libraries are installed:

* python-devel
* log4cpp-devel
* xerces-c-devel
* boost-devel
* netcdf, netcdf-cxx

These programs depend on libraf, domx, logx, and site_scons, which are included as submodules to this repo. They are automatically downloaded by using --recursive on the command line.

### Installation

The aircraft_vardb libraries can either be installed in the local directory, or redirected to install in a common area (such as /opt/local in a linux system).

To install everything locally in the current working directory:

* git clone --recursive <https://github.com/ncar/aircraft_vardb>
* cd aircraft_vardb
* scons install   <- install under vardb/lib, vardb/include, and vardb/bin

To install in a common area, first download and install site_scons, libraf, domx, and logx in a common area. Then use to --prefix option to point to that area and install libvardb under that same area.

* scons install INSTALL_PREFIX=<path_to_install_dir>  <- Install under a dir of your choosing
These libraries may be used by placing a "-lVarDB" in your compile statement.
