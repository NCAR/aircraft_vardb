#pragma once
/*
 * VaredMainWindow.h: Qt replacement for the Motif vared GUI.
 *
 * Faithfully mirrors the layout and behaviour of Xwin.cc / ccb.cc / initv.cc.
 *
 * Changes from original Qt port:
 *   - Variable list split into "Raw (N)" / "Derived (N)" tabs (QTabWidget).
 *   - Dependency tree (QTreeWidget) shows the full recursive dependency graph.
 *   - Status bar shows selection, counts, and unsaved-changes indicator.
 *   - List items carry tooltips showing title and units on hover.
 */

#include <QMainWindow>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QStringList>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

#include "raf/vardb.hh"
#include "raf/VarDBConverter.hh"

class VaredMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit VaredMainWindow(QWidget* parent = nullptr);

    /** Load a vardb file (XML or binary with auto-conversion). */
    void loadFile(const QString& path);

    /** Parse @p tablePath and merge dependency relationships into the
     *  currently-loaded vardb.  Creates missing variables, updates
     *  <dependencies> and <derive> attributes, refreshes the UI, and
     *  marks the document dirty.  Safe to call before show(). */
    void importDependTable(const QString& tablePath);

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onFileOpen();
    void onFileSave();
    void onFileSaveAs();
    void onImportDependTable();  // File menu "Merge DependTable..."
    void onVariableSelected(QListWidgetItem* item);
    void onAccept()
    void onClear();
    void onDelete();
    void onReset();      // "Reset Variable" = re-populate fields from current selection
    void onFocusList();  // return keyboard focus to the active list
    void onTreeContextMenu(const QPoint& pos);   // right-click dep tree Select variable
    void onListContextMenu(const QPoint& pos);   // right-click list Duplicate variable
    void onUndo();                               // Ctrl+Z: restore pre-Accept state
    void onFindActivated();                      // Ctrl+F: show/focus find bar
    void onFindChanged(const QString& text);     // live search across all attributes
    void onFindClosed();                         // Escape/close: hide find bar, restore lists

    /** Hide list items that do not contain text (case-insensitive); show all when empty. */
    void applyFilter(QListWidget* list, const QString& text);

    /** Set field background to indicate valid/invalid state. */
    void setFieldValid(QLineEdit* field, bool valid);

    /** Assign accessible names/descriptions and establish tab order.
     *  Called once at the end of setupUi. */
    void setupAccessibility();

    /** Validate all numeric fields; returns true when all pass cross-field range checks.
     *  Highlights individual fields in real time; range violations reported as warnings. */
    bool validateFields();

private:
    void setupUi();
    void rebuildSortedVars();
    void refreshList();
    void populateCombos();
    void populateFields(VDBVar* var);

    /** Update tab labels to reflect current raw/derived counts. */
    void updateTabCounts();

    /** Refresh the status bar: selection name, counts, unsaved flag. */
    void updateStatusBar();

    /** Repopulate the dependency tree for the selected variable.
     *  Expands all nodes automatically when tree depth is 3 or fewer. */
    void refreshDepTree(VDBVar* var);

    /** Recursively add dependency nodes for varName under parent.
     *  visited tracks the current path to detect cycles.
     *  Returns the depth of the subtree created (0 = leaf). */
    int buildDepTree(QTreeWidgetItem* parent,
                     const QString& varName,
                     QSet<QString>& visited);

    /* Snapshot of one variable's editable attributes, used for single-level undo */
    struct VarSnapshot {
        std::string name;
        std::string longName;
        std::string units;
        std::string altUnits;
        std::string voltageRange;
        std::string sampleRate;
        std::string minLimit;
        std::string maxLimit;
        std::string calRange;
        std::string category;
        std::string standardName;
        bool        isAnalog   {false};
        bool        isReference{false};
        bool        wasNew     {false}; // true = Accept created this var (undo = delete)
    };
    std::optional<VarSnapshot> m_undoSnapshot;

    VDBFile           m_vdbFile;
    VarDBConverter    m_vdbConverter;
    std::vector<VDBVar*>               m_sortedVars;
    std::unordered_map<std::string, VDBVar*> m_varLookup; // fast name var
    std::vector<std::string>           m_categoryNames;
    std::vector<std::string>           m_stdNames;
    std::string                        m_xmlSavePath;
    bool                               m_changesMade{false};
    QString                            m_selectedVarName; // for status bar

    // Change tracking for the quit summary dialog
    QSet<QString> m_addedVars;
    QSet<QString> m_modifiedVars;
    QSet<QString> m_deletedVars;

    // ---- widgets ----
    // ---- find bar (hidden until Ctrl+F) ----
    QFrame*      m_findBar;
    QLineEdit*   m_findEdit;
    QLabel*      m_findCount;

    QTabWidget*  m_varTabs;        // tabs: "Raw" | "Derived"
    QListWidget* m_rawList;        // variables with no DERIVE attribute
    QListWidget* m_derivedList;    // variables that have a DERIVE attribute
    QLineEdit*   m_rawFilter;      // filter box above raw list
    QLineEdit*   m_derivedFilter;  // filter box above derived list
    QTreeWidget* m_depTree;      // recursive dependency graph for selection

    // EFtext[0..10] equivalents (indices match Motif)
    QLineEdit*   m_nameEdit;         // [0] Name       max 16
    QLineEdit*   m_titleEdit;        // [1] Title       max 64
    QLineEdit*   m_unitsEdit;        // [2] Units       max 16
    QLineEdit*   m_altUnitsEdit;     // [3] Alt Units   max 16
    QLineEdit*   m_voltLoEdit;       // [4] Volt Lo     max 16
    QLineEdit*   m_voltHiEdit;       // [5] Volt Hi     max 16
    QLineEdit*   m_sampleRateEdit;   // [6] Sample Rate max 16
    QLineEdit*   m_minLimitEdit;     // [7] Min Limit   max 16
    QLineEdit*   m_maxLimitEdit;     // [8] Max Limit   max 16
    QLineEdit*   m_calLoEdit;        // [9] Cal Lo      max 16
    QLineEdit*   m_calHiEdit;        // [10] Cal Hi     max 16

    QCheckBox*   m_analogCheck;      // "Is Analog"
    QCheckBox*   m_referenceCheck;   // "Is this variable *the* reference?"
    QComboBox*   m_categoryCombo;
    QComboBox*   m_stdNameCombo;
    QPushButton* m_acceptBtn;
};
