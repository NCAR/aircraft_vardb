#pragma once
/*
 * VaredMainWindow.h — Qt5 replacement for the Motif vared GUI.
 *
 * Faithfully mirrors the layout and behaviour of Xwin.cc / ccb.cc / initv.cc.
 */

#include <QMainWindow>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QScrollBar>
#include <string>
#include <vector>

#include "raf/vardb.hh"
#include "raf/VarDBConverter.hh"

class VaredMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit VaredMainWindow(QWidget* parent = nullptr);

    /** Load a vardb file (XML or binary with auto-conversion). */
    void loadFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* e) override;

private slots:
    void onFileOpen();
    void onFileSave();
    void onFileSaveAs();
    void onVariableSelected(QListWidgetItem* item);
    void onAccept();
    void onClear();
    void onDelete();
    void onReset();   // "Reset Variable" = re-populate fields from current selection

private:
    void setupUi();
    void rebuildSortedVars();
    void refreshList();
    void populateCombos();
    void populateFields(VDBVar* var);

    VDBFile           m_vdbFile;
    VarDBConverter    m_vdbConverter;
    std::vector<VDBVar*>      m_sortedVars;
    std::vector<std::string>  m_categoryNames;
    std::vector<std::string>  m_stdNames;
    std::string               m_xmlSavePath;
    bool                      m_changesMade{false};

    // ---- widgets (mirror order from fbr.h / Xwin.cc) ----
    QListWidget* m_varList;

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
