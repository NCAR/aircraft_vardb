/*
 * DependTableMerge.h Parse and apply a *_DependTable file to a VDBFile.
 *
 * A DependTable is a whitespace-separated text file where each non-comment
 * line encodes a dependency relationship:
 *
 *   DERIVED_VAR  DEP1 DEP2 ...
 *
 * First token  = variable being derived (the output).
 * Remaining    = its input variables (raw sensors or intermediate outputs).
 * Lines with fewer than two tokens and lines beginning with '#' are ignored.
 *
 * applyDependTable() applies the following changes to the supplied VDBFile:
 *   - Sets <dependencies>DEP1 DEP2 ...</dependencies> on DERIVED_VAR
 *     (replaces any existing value).
 *   - Appends DERIVED_VAR to <derive> on each DEPn if not already present.
 *   - Creates any variable that is referenced but not yet in the XML via
 *     VDBFile::add_var(), so the document stays self-consistent.
 *
 * The function is intentionally free of Qt so it can be called from both
 * the interactive editor (vared_qt) and the batch tool (apply_deptable).
 */

#pragma once

#include <string>
#include <vector>

#include "raf/vardb.hh"


/** Summary returned by applyDependTable(). */
struct MergeResult
{
    int updated {0};    ///< variables that already existed and received changes
    int created {0};    ///< variables created via VDBFile::add_var()
    std::vector<std::string> createdNames;  ///< names of every created variable
    std::vector<std::string> updatedNames;  ///< names of every updated variable
};

/**
 * Parse @p tablePath and merge the dependency relationships into @p vdb.
 *
 * Missing variables are created with VDBFile::add_var().  The caller is
 * responsible for saving the document afterwards.
 *
 * @param tablePath  Path to the *_DependTable file.
 * @param vdb        Open, valid VDBFile to modify in place.
 * @returns          MergeResult summary (zero counts if the file is empty or
 *                   cannot be opened).
 */
MergeResult applyDependTable(const std::string& tablePath, VDBFile& vdb);
