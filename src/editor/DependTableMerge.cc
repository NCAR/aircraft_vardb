/*
 * DependTableMerge.cc — Implementation of applyDependTable().
 *
 * See DependTableMerge.h for the full contract.
 */

#include "DependTableMerge.h"

#include <fstream>


/* -------------------------------------------------------------------- */
/* Internal types and helpers                                             */
/* -------------------------------------------------------------------- */

namespace {

struct DepEntry
{
    std::string derived;
    std::vector<std::string> deps;
};

/** Split @p s on ASCII whitespace into tokens. */
static std::vector<std::string> splitTokens(const std::string& s)
{
    std::vector<std::string> out;
    std::string::size_type i = 0;
    while (i < s.size())
    {
        i = s.find_first_not_of(" \t", i);
        if (i == std::string::npos) break;
        auto end = s.find_first_of(" \t", i);
        out.push_back(s.substr(i, end - i));
        i = (end == std::string::npos) ? s.size() : end;
    }
    return out;
}

/** Join tokens into a single space-separated string. */
static std::string joinTokens(const std::vector<std::string>& v)
{
    std::string out;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i) out += ' ';
        out += v[i];
    }
    return out;
}

/** Parse a DependTable file into a list of (derived, deps) entries.
 *  Strips inline '#' comments; skips blank and single-token lines. */
static std::vector<DepEntry> parseDependTable(const std::string& path)
{
    std::vector<DepEntry> entries;
    std::ifstream in(path);
    if (!in)
        return entries;

    std::string line;
    while (std::getline(in, line))
    {
        /* Strip inline comments */
        auto hash = line.find('#');
        if (hash != std::string::npos)
            line = line.substr(0, hash);

        std::vector<std::string> tokens = splitTokens(line);
        if (tokens.size() < 2)
            continue;   /* blank, comment-only, or no-dependency line */

        DepEntry e;
        e.derived = tokens[0];
        e.deps.assign(tokens.begin() + 1, tokens.end());
        entries.push_back(std::move(e));
    }
    return entries;
}

/** Append @p name to the <derive> attribute on @p var if not already present. */
static void addToDerive(VDBVar* var, const std::string& name)
{
    std::vector<std::string> current =
        splitTokens(var->get_attribute(VDBVar::DERIVE));

    for (const auto& t : current)
        if (t == name) return;  /* already listed */

    current.push_back(name);
    var->set_attribute(VDBVar::DERIVE, joinTokens(current));
}

} // anonymous namespace


/* -------------------------------------------------------------------- */
/* Public API                                                             */
/* -------------------------------------------------------------------- */

MergeResult applyDependTable(const std::string& tablePath, VDBFile& vdb)
{
    MergeResult result;

    std::vector<DepEntry> entries = parseDependTable(tablePath);
    if (entries.empty())
        return result;

    for (const auto& e : entries)
    {
        /* Resolve (or create) the derived variable */
        VDBVar* derived = vdb.get_var(e.derived);
        if (derived)
        {
            result.updated++;
            result.updatedNames.push_back(e.derived);
        }
        else
        {
            derived = vdb.add_var(e.derived);
            result.created++;
            result.createdNames.push_back(e.derived);
        }

        /* Set <dependencies> — replaces any existing value */
        derived->set_attribute(VDBVar::DEPENDENCIES, joinTokens(e.deps));

        /* Update <derive> on each input variable */
        for (const auto& dep : e.deps)
        {
            VDBVar* depVar = vdb.get_var(dep);
            if (!depVar)
            {
                /* Raw sensor stub — create so <derive> can be set on it */
                depVar = vdb.add_var(dep);
                result.created++;
                result.createdNames.push_back(dep);
            }
            addToDerive(depVar, e.derived);
        }
    }

    return result;
}
