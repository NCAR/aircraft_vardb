"The Variable class for representing a RAF variable and its metadata."

import os
import logging
import psycopg2 as pg

from vardb import VDBVar
from vardb import VDBFile

logger = logging.getLogger(__name__)

# The database variable_list table contains the currently active
# real-time variables and some of their metadata, so in real-time it
# should serve as the most consistent source for variables and
# metadata.  Normally it will be consistent with what is in the nidas
# XML file and the XML variable database (VDBFile), although it will
# not contain all the metadata from those files.  (For example, the
# variable_list table does not contain the min/max limits which might
# be needed for real-time qc checks.)  Therefore this class provides a
# consolidated view of the real-time variable metadata by augmenting
# the database variable_list with the metadata from the VDB file.  In
# the future, such consolidation should also include the nidas XML
# file.  Either all of the requisite metadata can be put in the
# variable_list table where other applications can get it, or there
# should be an API in both C++ and Python which provides a single,
# consistent, consolidated view of variables and metadata.

class VariableList:
    """
    Provide a consolidated, consistent list of real-time
    variables and metadata from the SQL database and VDB file.
    """

    def __init__(self):
        self.dbname = "real-time"
        self.user = "ads"
        self.hostname = "acserver"
        self.db = None
        self.vdbpath = None
        self.project = None
        self.aircraft = None
        self.platform = None
        self.projdir = None
        self.configpath = None
        self.variables = {}
        self.columns = []
        self.cmap = {}

    def getVariables(self):
        return self.variables

    def setVariables(self, variables):
        "Explicitly replace the current variable dictionary."
        self.variables = variables

    def setDatabaseSpecifier(self, hostspec):
        if hostspec == 'acserver':
            self.dbname = "real-time"
            self.hostname = "acserver"
        elif hostspec == 'c130':
            self.dbname = "real-time-C130"
            self.hostname = "eol-rt-data.eol.ucar.edu"
        elif hostspec == 'gv':
            self.dbname = "real-time-GV"
            self.hostname = "eol-rt-data.eol.ucar.edu"
        elif hostspec == 'env':
            self.dbname = None
            self.hostname = None
        else:
            self.hostname = hostspec

    def connect(self):
        if self.db:
            return
        # Apparently the psychopg2 package version on RHEL 6.x does not allow
        # keyword parameters to be passed as None, so if we want some 
        # connection parameters to come from the environment we have to leave
        # them out of the argument list.
        args = {}
        if self.dbname:
            args['database'] = self.dbname
        if self.user:
            args['user'] = self.user
        if self.hostname:
            args['host'] = self.hostname
        self.db = pg.connect(**args)

    def close(self):
        if self.db:
            self.db.close()
        self.db = None

    def configPath(self):
        if not self.configpath:
            select = (lambda a, b: [a, b][int(not a)])
            parms = {}
            parms['project'] = select(self.project, '${PROJECT}')
            parms['aircraft'] = select(self.aircraft, '${AIRCRAFT}')
            parms['projdir'] = select(self.projdir, '${PROJ_DIR}')
            path = '%(projdir)s/%(project)s/%(aircraft)s' % parms
            self.configpath = os.path.expandvars(path)
        return self.configpath

    def vdbPath(self):
        return os.path.join(self.configPath(), 'vardb.xml')

    def aircraftFromPlatform(self, platform):
        if platform == "N130AR":
            return "C130_N130AR"
        if platform == "N677F":
            return "GV_N677F"
        return platform

    def loadVariables(self):
        self.connect()
        cursor = self.db.cursor()
        # We want to initialize global metadata also in case it's needed
        # to locate the vdb file or other files.
        cursor.execute("""
SELECT key, value
FROM global_attributes;
""")
        rows = cursor.fetchall()
        gatts = {}
        for r in rows:
            (key, value) = r
            gatts[key] = value
        if not self.project:
            self.project = gatts['ProjectName']
        if not self.platform:
            self.platform = gatts['Platform']
        if not self.aircraft:
            self.aircraft = self.aircraftFromPlatform(self.platform)
        cursor.execute("""
SELECT name, units, uncalibrated_units, long_name, missing_value, ndims
FROM variable_list;
""")
        rows = cursor.fetchall()
        for r in rows:
            var = Variable(r[0])
            var.units = r[1]
            if r[2]:
                var.uncalibrated_units = r[2]
            var.long_name = r[3]
            var.missing_value = float(r[4])
            var.ndims = int(r[5])
            self.variables[var.name] = var
        logger.info("Loaded %d variables from database." % 
                    (len(self.variables)))
        return self.variables

    def loadVdbFile(self):
        """
        Load variable metadata from a vdb file and return it.  It can be merged
        separately with mergeVdbVariables().
        """
        # Any existing variable metadata from the database takes
        # precedence.
        if not self.vdbpath:
            self.vdbpath = self.vdbPath()
        vdb = VDBFile(self.vdbpath)
        if not vdb.is_valid():
            raise Exception("failed to open %s" % (self.vdbpath))
        variables = {}
        for i in range(vdb.num_vars()):
            vdbvar = vdb.get_var_at(i)
            var = Variable(vdbvar.name)
            var.fromVDBVar(vdbvar)
            variables[var.name] = var
        return variables

    def mergeVdbVariables(self, variables):
        for var in variables.itervalues():
            dbvar = self.variables.get(var.name, None)
            if dbvar:
                var.units = dbvar.units
                var.uncalibrated_units = dbvar.uncalibrated_units
                var.long_name = dbvar.long_name
                var.missing_value = dbvar.missing_value
                self.variables[var.name] = var
        return self.variables

    def selectVariable(self, vname):
        "Add the variable named @p vname to the select list, if it exists."
        if vname in self.variables:
            if not vname in self.cmap:
                logger.debug("selecting variable %s" % (vname))
                self.cmap[vname] = len(self.columns)
                self.columns.append(vname)
        else:
            logger.warning("cannot select %s: not in database" % (vname))

    def getLatestValues(self, datastore, lookback=1):
        """Fill @p datastore with recent values of selected variables.

        """
        self.connect()
        proj = ",".join(self.columns)
        if proj:
            proj += ","
        proj += "datetime"
        sql = """
SELECT %s FROM raf_lrt ORDER BY datetime DESC LIMIT %d;
"""
        sql = sql % (proj, lookback)
        logger.debug(sql)
        cursor = self.db.cursor()
        cursor.execute(sql)
        rows = cursor.fetchall()
        for r in rows:
            # The type of the datetime column is datetime.datetime()
            datastore.appendTime(r[-1])
            for i, vname in enumerate(self.columns):
                var = self.variables[vname]
                try:
                    datastore.appendValue(var, float(r[i]))
                except TypeError:
                    logger.error("variable %s, column %i: "
                                 "cannot convert %s to float." %
                                 (vname, i, r[i]))
                    datastore.appendValue(var, var.missing_value)

        return datastore


class DataStore:
    "Simple container for a subset of data from the database."

    def __init__(self):
        "Start out empty: no variables, no values."
        self.variables = {}
        self.values = {}
        self.times = []

    def appendTime(self, dtime):
        "Append datetime @p dtime."
        self.times.append(dtime)

    def getTimes(self):
        return self.times

    def appendValue(self, variable, value):
        vname = variable.name
        if not vname in self.variables:
            self.variables[vname] = variable
            self.values[vname] = [value]
        else:
            self.values[vname].append(value)

    def setValues(self, variable, values):
        vname = variable.name
        if not vname in self.variables:
            self.variables[vname] = variable
        self.values[vname] = values

    def getVariable(self, vname):
        return self.variables.get(vname)

    def getValues(self, vname, lookback=None):
        """Returns the list of values for this variable, or None.

        In theory this should return a copy, perhaps trimmed to contain
        only the number of values the caller wants.  However, I assume
        returning the exact list saves on memory thrashing, and the callers
        can be trusted not to modify the list.  If lookback is not
        specified, then all the values are returned, otherwise a list of
        only the @p lookback most recent values are returned.  The values
        are always returned most recent first.

        """
        values = self.values.get(vname)
        if values and lookback is not None:
            return values[:lookback]
        return values


class Variable:

    _attributes = {
        VDBVar.UNITS : str,
        VDBVar.LONG_NAME : str,
        VDBVar.IS_ANALOG : (lambda txt: txt == "true"),
        VDBVar.VOLTAGE_RANGE : str,
        VDBVar.DEFAULT_SAMPLE_RATE : int,
        VDBVar.MIN_LIMIT : float,
        VDBVar.MAX_LIMIT : float,
        VDBVar.CATEGORY : str,
        VDBVar.MODULUS_RANGE : str,
        VDBVar.DERIVE : str,
        VDBVar.DEPENDENCIES : str,
        VDBVar.STANDARD_NAME : str,
        VDBVar.REFERENCE : (lambda txt: txt == "true")
    }

    def __init__(self, name):
        self.name = name
        self.units = None
        self.uncalibrated_units = None
        self.long_name = None
        self.is_analog = None
        self.voltage_range = None
        self.default_sample_rate = None
        self.min_limit = None
        self.max_limit = None
        self.category = None
        self.modulus_range = None
        self.derive = None
        self.dependencies = None
        self.standard_name = None
        self.reference = None
        self.missing_value = None
        self.ndims = None

    def _get_value(self, vdbvar, att, dfault, cvt):
        attvalue = vdbvar.get_attribute(att)
        if attvalue:
            return cvt(attvalue)
        return dfault

    def fromVDBVar(self, vdbvar):
        "Fill this Variable from the given variable database entry."
        self.name = vdbvar.name()
        for attname, attcvt in Variable._attributes.items():
            value = self._get_value(vdbvar, attname, 
                                    self.__getattribute__(attname), attcvt)
            self.__setattr__(attname, value)

