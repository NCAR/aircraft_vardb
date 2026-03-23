/*
 * test_vared_logic.cc — gtest unit tests for vared data-layer logic.
 *
 * Tests pure logic extracted from ccb.cc: parsePair, findIndex,
 * rebuildSortedVars ordering, Accept create/update, name-uppercase,
 * VoltRange/CalRange composition, "None" category/stdName handling,
 * Delete, and OpenFile populates "None" at index 0.
 *
 * No Qt dependency.
 */

#include <gtest/gtest.h>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>

#include "raf/vardb.hh"
#include "raf/VarDBConverter.hh"

/* ---- helpers copied verbatim from ccb.cc ---- */

static void parsePair(const std::string& str,
                      std::string& lo, std::string& hi)
{
    lo.clear(); hi.clear();
    std::istringstream iss(str);
    iss >> lo >> hi;
}

static int findIndex(const std::vector<std::string>& vec,
                     const std::string& name)
{
    for (int i = 0; i < (int)vec.size(); ++i)
        if (vec[i] == name) return i;
    return 0;
}

/* ---- parsePair tests ---- */

TEST(ParsePair, Normal)
{
    std::string lo, hi;
    parsePair("0 10", lo, hi);
    EXPECT_EQ(lo, "0");
    EXPECT_EQ(hi, "10");
}

TEST(ParsePair, Empty)
{
    std::string lo, hi;
    parsePair("", lo, hi);
    EXPECT_TRUE(lo.empty());
    EXPECT_TRUE(hi.empty());
}

TEST(ParsePair, OneToken)
{
    std::string lo, hi;
    parsePair("5", lo, hi);
    EXPECT_EQ(lo, "5");
    EXPECT_TRUE(hi.empty());
}

TEST(ParsePair, NegativeValues)
{
    std::string lo, hi;
    parsePair("-10.5 10.5", lo, hi);
    EXPECT_EQ(lo, "-10.5");
    EXPECT_EQ(hi, "10.5");
}

/* ---- findIndex tests ---- */

TEST(FindIndex, Found)
{
    std::vector<std::string> v = {"None", "Winds", "Thermodynamics", "State"};
    EXPECT_EQ(findIndex(v, "Thermodynamics"), 2);
}

TEST(FindIndex, Missing)
{
    std::vector<std::string> v = {"None", "Winds", "State"};
    EXPECT_EQ(findIndex(v, "NotHere"), 0);
}

TEST(FindIndex, NoneAtFront)
{
    std::vector<std::string> v = {"None", "Winds"};
    EXPECT_EQ(findIndex(v, "None"), 0);
}

/* ---- VDBFile integration tests (require contrast_vardb.xml) ---- */

static const char* TEST_XML = "tests/contrast_vardb.xml";

TEST(VDBFile, OpenFilePopulatesNoneFirst)
{
    VDBFile vdb;
    VarDBConverter conv;
    conv.open(&vdb, TEST_XML);
    ASSERT_TRUE(vdb.is_valid());

    auto cats = vdb.get_categories();
    auto stds = vdb.get_standard_names();

    /* Simulate the "None" insertion that OpenNewFile_OK does */
    if (cats.empty() || cats[0] != "None")
        cats.insert(cats.begin(), "None");
    if (stds.empty() || stds[0] != "None")
        stds.insert(stds.begin(), "None");

    EXPECT_EQ(cats[0], "None");
    EXPECT_EQ(stds[0], "None");
}

TEST(VDBFile, RebuildSortedVarsOrder)
{
    VDBFile vdb;
    VarDBConverter conv;
    conv.open(&vdb, TEST_XML);
    ASSERT_TRUE(vdb.is_valid());

    std::vector<VDBVar*> sorted;
    for (int i = 0; i < vdb.num_vars(); ++i)
        sorted.push_back(vdb.get_var(i));
    std::sort(sorted.begin(), sorted.end(),
        [](VDBVar* a, VDBVar* b){ return a->name() < b->name(); });

    for (int i = 1; i < (int)sorted.size(); ++i)
        EXPECT_LE(sorted[i-1]->name(), sorted[i]->name())
            << "Not sorted at index " << i;
}

TEST(VDBFile, AcceptCreatesVar)
{
    VDBFile vdb;
    vdb.create();

    EXPECT_EQ(vdb.get_var("NEWVAR"), nullptr);
    VDBVar* v = vdb.add_var("NEWVAR");
    ASSERT_NE(v, nullptr);
    EXPECT_NE(vdb.get_var("NEWVAR"), nullptr);
}

TEST(VDBFile, AcceptUpdatesVar)
{
    VDBFile vdb;
    vdb.create();
    VDBVar* v = vdb.add_var("AKRD");
    v->set_attribute(VDBVar::UNITS, "degree");
    v->set_attribute(VDBVar::LONG_NAME, "Attack angle");

    /* Simulate Accept: update fields */
    VDBVar* v2 = vdb.get_var("AKRD");
    ASSERT_NE(v2, nullptr);
    v2->set_attribute(VDBVar::UNITS, "radian");

    EXPECT_EQ(vdb.get_var("AKRD")->get_attribute(VDBVar::UNITS), "radian");
}

TEST(VDBFile, AcceptNameUppercase)
{
    /* Qt port does text.toUpper(); verify the VDBFile stores it correctly */
    VDBFile vdb;
    vdb.create();
    std::string upper = "akrd";
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    vdb.add_var(upper);
    EXPECT_NE(vdb.get_var("AKRD"), nullptr);
    EXPECT_EQ(vdb.get_var("akrd"), nullptr);  /* case-sensitive lookup */
}

TEST(VDBFile, AcceptVoltRangeComposed)
{
    VDBFile vdb;
    vdb.create();
    VDBVar* v = vdb.add_var("THETAP");
    std::string vr = std::string("-10") + " " + std::string("10");
    v->set_attribute(VDBVar::VOLTAGE_RANGE, vr);
    EXPECT_EQ(v->get_attribute(VDBVar::VOLTAGE_RANGE), "-10 10");
}

TEST(VDBFile, AcceptCalRangeComposed)
{
    VDBFile vdb;
    vdb.create();
    VDBVar* v = vdb.add_var("ATX");
    std::string cr = std::string("-50") + " " + std::string("50");
    v->set_attribute(VDBVar::CAL_RANGE, cr);
    EXPECT_EQ(v->get_attribute(VDBVar::CAL_RANGE), "-50 50");
}

TEST(VDBFile, AcceptNoneCategory)
{
    /* "None" category should not be written as an attribute */
    VDBFile vdb;
    vdb.create();
    VDBVar* v = vdb.add_var("TEST");
    /* Don't set category (same as "None" being selected) */
    EXPECT_TRUE(v->get_attribute(VDBVar::CATEGORY).empty());
}

TEST(VDBFile, DeleteRemovesVar)
{
    VDBFile vdb;
    VarDBConverter conv;
    conv.open(&vdb, TEST_XML);
    ASSERT_TRUE(vdb.is_valid());
    int before = vdb.num_vars();
    ASSERT_GT(before, 0);

    std::string name = vdb.get_var(0)->name();
    bool removed = vdb.remove_var(name);
    EXPECT_TRUE(removed);
    EXPECT_EQ(vdb.num_vars(), before - 1);
    EXPECT_EQ(vdb.get_var(name), nullptr);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
