# aircraft_vardb
A library and collection of programs for manipulating the variable database.  The variable database is a master list of all variables for netCDF output, their units, titles, and other miscellaneous metadata.
The variable database does not use an actual databae, but is rather an XML file.

The OAP file format can be found at http://www.eol.ucar.edu/raf/software/OAPfiles.html

Directories:

| Directory | Description |
| ----------- | ----------------------------------------------------------------------------------------- |
| lib | Library routines to open and manipulate a variable database. |
| editor | This is a Motif based editor for editing variables in a specific file. |
| vdbdump | Utility to produce an ASCII dump of a variable database. |
| vdb2xml | Utility to convert an style binary VarDB to new style vardb.xml. |
| vdb2ncml | Utility to convert an style binary VarDB to netCDF NCML output. |

### Documentation ###

### Environment ###

### Dependencies ###

 * python-devel
 * log4cpp-devel
 * xerces-c-devel
 * boost-devel

### Installation ###
