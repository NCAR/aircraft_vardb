/*
 * -------------------------------------------------------------------------
 * OBJECT NAME:  apply_deptable.cc
 *
 * FULL NAME:    VarDB DependTable importer
 *
 * DESCRIPTION:  Reads a *_DependTable file and merges dependency/derive
 *               relationships into a vardb XML file.
 *
 * DependTable format:
 *   # comment
 *   DERIVED_VAR  DEP1 DEP2 ...
 *
 *   - First token is the derived variable.
 *   - Remaining tokens are the variables it depends on.
 *   - Lines with only one token (e.g. ONE, ZERO) are skipped.
 *
 * XML changes applied per line:
 *   - <dependencies>DEP1 DEP2 ...</dependencies> set on DERIVED_VAR
 *     (replaces any existing value).
 *   - DERIVED_VAR appended to <derive> on each DEPn that exists in the
 *     XML, if not already present.
 *
 * Usage:
 *   apply_deptable <DependTable> <vardb.xml> [output.xml]
 *
 *   If output.xml is omitted the input XML is overwritten in place.
 *
 * COPYRIGHT:    University Corporation for Atmospheric Research, 2025
 * -------------------------------------------------------------------------
 */

#include <raf/vardb.hh>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;


/* -------------------------------------------------------------------- */
/* DependTable parsing                                                    */
/* -------------------------------------------------------------------- */

struct DepEntry
{
    string derived;
    vector<string> deps;
};

/** Parse a DependTable file into a list of dependency entries.
 *  Skips comment lines (#) and lines with no dependencies. */
static vector<DepEntry>
parseDependTable(const string& path)
{
    vector<DepEntry> entries;
    ifstream in(path);
    if (!in)
    {
        cerr << "apply_deptable: cannot open DependTable: " << path << "\n";
        return entries;
    }

    string line;
    while (getline(in, line))
    {
        /* Strip inline comments: everything from '#' onward */
        auto hash = line.find('#');
        if (hash != string::npos)
            line = line.substr(0, hash);

        istringstream ss(line);
        vector<string> tokens;
        string tok;
        while (ss >> tok)
            tokens.push_back(tok);

        if (tokens.size() < 2)
            continue;   /* comment-only, blank, or no-dependency line */

        DepEntry e;
        e.derived = tokens[0];
        e.deps.assign(tokens.begin() + 1, tokens.end());
        entries.push_back(e);
    }
    return entries;
}


/* -------------------------------------------------------------------- */
/* derive-list helpers                                                    */
/* -------------------------------------------------------------------- */

/** Split a space-separated attribute string into tokens. */
static vector<string>
splitTokens(const string& s)
{
    vector<string> out;
    istringstream ss(s);
    string tok;
    while (ss >> tok)
        out.push_back(tok);
    return out;
}

/** Join tokens into a single space-separated string. */
static string
joinTokens(const vector<string>& v)
{
    string out;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i) out += ' ';
        out += v[i];
    }
    return out;
}

/** Append @p name to the <derive> attribute on @p var if not already present. */
static void
addToDerive(VDBVar* var, const string& name)
{
    vector<string> current = splitTokens(var->get_attribute(VDBVar::DERIVE));
    for (const auto& t : current)
        if (t == name) return;   /* already present */
    current.push_back(name);
    var->set_attribute(VDBVar::DERIVE, joinTokens(current));
}


/* -------------------------------------------------------------------- */
int
main(int argc, char* argv[])
{
    if (argc < 3)
    {
        cerr << "Usage: apply_deptable <DependTable> <vardb.xml> [output.xml]\n";
        return 1;
    }

    const string tableFile = argv[1];
    const string xmlIn     = argv[2];
    const string xmlOut    = (argc >= 4) ? argv[3] : xmlIn;

    /* Parse the table first so we fail fast before touching the XML */
    vector<DepEntry> entries = parseDependTable(tableFile);
    if (entries.empty())
    {
        cerr << "apply_deptable: no entries found in " << tableFile << "\n";
        return 1;
    }

    VDBFile vdb;
    vdb.open(xmlIn);
    if (!vdb.is_valid())
    {
        cerr << "apply_deptable: failed to open vardb XML: " << xmlIn << "\n";
        return 1;
    }

    int warnings = 0;

    for (const auto& e : entries)
    {
        VDBVar* derived = vdb.get_var(e.derived);
        if (!derived)
        {
            cerr << "WARNING: derived variable '" << e.derived
                 << "' not found in XML — skipping\n";
            ++warnings;
            continue;
        }

        /* Set <dependencies> on the derived variable (replace any existing) */
        derived->set_attribute(VDBVar::DEPENDENCIES, joinTokens(e.deps));

        /* Update <derive> on each input variable */
        for (const auto& dep : e.deps)
        {
            VDBVar* depVar = vdb.get_var(dep);
            if (!depVar)
            {
                cerr << "WARNING: dependency '" << dep
                     << "' (needed by '" << e.derived << "') not found in XML\n";
                ++warnings;
                continue;
            }
            addToDerive(depVar, e.derived);
        }
    }

    vdb.save(xmlOut);

    if (warnings)
        cerr << warnings << " warning(s) — check output.\n";

    return (warnings > 0) ? 2 : 0;
}
