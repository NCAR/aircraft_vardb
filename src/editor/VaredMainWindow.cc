/*
 * VaredMainWindow.cc — Qt5 replacement for the Motif vared GUI.
 *
 * Logic faithfully ported from:
 *   ccb.cc  — callbacks (Accept, Clear, Delete, EditVariable, OpenNewFile_OK, …)
 *   initv.cc — Initialize()
 *   Xwin.cc  — CreateMainWindow() layout
 *   fbr.h    — resource strings (labels, sizes, colours)
 */

#include "VaredMainWindow.h"

#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QCloseEvent>

#include <algorithm>
#include <sstream>

extern "C" {
char* strupr(char*);
}

/* -------------------------------------------------------------------- */
/* Helper: parse "lo hi" space-separated string into two values         */
static void parsePair(const std::string& str,
                      std::string& lo, std::string& hi)
{
    lo.clear();
    hi.clear();
    std::istringstream iss(str);
    iss >> lo >> hi;
}

/* Helper: find index of name in vector, return 0 if not found          */
static int findIndex(const std::vector<std::string>& vec,
                     const std::string& name)
{
    for (int i = 0; i < (int)vec.size(); ++i)
        if (vec[i] == name)
            return i;
    return 0;
}

/* -------------------------------------------------------------------- */
VaredMainWindow::VaredMainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Variable DataBase Editor");
    setupUi();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::setupUi()
{
    /* ---- Menus (mirrors fbr.h accelerators exactly) ---- */
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Open New File",  this, &VaredMainWindow::onFileOpen,  QKeySequence("Ctrl+O"));
    fileMenu->addAction("Save",           this, &VaredMainWindow::onFileSave,  QKeySequence("Ctrl+S"));
    fileMenu->addAction("Save As",        this, &VaredMainWindow::onFileSaveAs,QKeySequence("Ctrl+A"));
    fileMenu->addSeparator();
    fileMenu->addAction("Quit",           qApp, &QApplication::quit,           QKeySequence("Ctrl+Q"));

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("Clear Variable",  this, &VaredMainWindow::onClear,  QKeySequence("Ctrl+C"));
    editMenu->addAction("Delete Variable", this, &VaredMainWindow::onDelete, QKeySequence("Ctrl+D"));
    editMenu->addAction("Reset Variable",  this, &VaredMainWindow::onReset,  QKeySequence("Ctrl+R"));

    /* ---- Central widget: HBox (form | list) ---- */
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout* hbox = new QHBoxLayout(central);
    hbox->setSpacing(8);
    hbox->setContentsMargins(8, 8, 8, 8);

    /* ---- Left: form layout ---- */
    const QString fieldStyle = "background-color: #DEB887; color: black;"; // Burlywood

    auto makeEdit = [&](int maxLen) -> QLineEdit* {
        QLineEdit* e = new QLineEdit;
        e->setMaxLength(maxLen);
        e->setStyleSheet(fieldStyle);
        return e;
    };

    m_nameEdit       = makeEdit(16);
    m_titleEdit      = makeEdit(64);
    m_unitsEdit      = makeEdit(16);
    m_altUnitsEdit   = makeEdit(16);
    m_voltLoEdit     = makeEdit(16);
    m_voltHiEdit     = makeEdit(16);
    m_sampleRateEdit = makeEdit(16);
    m_minLimitEdit   = makeEdit(16);
    m_maxLimitEdit   = makeEdit(16);
    m_calLoEdit      = makeEdit(16);
    m_calHiEdit      = makeEdit(16);

    m_analogCheck    = new QCheckBox;
    m_referenceCheck = new QCheckBox;
    m_categoryCombo  = new QComboBox;
    m_stdNameCombo   = new QComboBox;

    m_acceptBtn = new QPushButton("Accept");
    m_acceptBtn->setFixedSize(100, 32);

    /* Title field: minimum 40-char width */
    {
        QFontMetrics fmEdit(m_titleEdit->font());
        m_titleEdit->setMinimumWidth(fmEdit.averageCharWidth() * 40);
    }

    QFormLayout* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);

    /* Labels match fbr.h EFlabel strings exactly */
    form->addRow("Name:",                m_nameEdit);
    form->addRow("Title:",               m_titleEdit);
    form->addRow("Units:",               m_unitsEdit);
    form->addRow("Alt Units:",           m_altUnitsEdit);
    form->addRow("Is Analog:",           m_analogCheck);

    QHBoxLayout* voltRow = new QHBoxLayout;
    voltRow->addWidget(m_voltLoEdit);
    voltRow->addWidget(m_voltHiEdit);
    voltRow->setSpacing(4);
    form->addRow("Voltage Range:",       voltRow);

    form->addRow("Default Sample Rate:", m_sampleRateEdit);
    form->addRow("Minimum Value:",       m_minLimitEdit);
    form->addRow("Maximum Value:",       m_maxLimitEdit);

    QHBoxLayout* calRow = new QHBoxLayout;
    calRow->addWidget(m_calLoEdit);
    calRow->addWidget(m_calHiEdit);
    calRow->setSpacing(4);
    form->addRow("Calibration Range:",   calRow);

    form->addRow("Category:",            m_categoryCombo);
    form->addRow("Standard Name:",       m_stdNameCombo);
    form->addRow("Is reference?:",       m_referenceCheck);

    /* Enforce label minimum width (mirrors *editFieldRC*XmLabel.width: 200) */
    for (int i = 0; i < form->rowCount(); ++i) {
        QLayoutItem* li = form->itemAt(i, QFormLayout::LabelRole);
        if (li && li->widget())
            li->widget()->setMinimumWidth(200);
    }

    hbox->addLayout(form);

    /* ---- Right: variable list + Accept button below it ---- */
    m_varList = new QListWidget;
    m_varList->setFixedWidth(125);
    m_varList->setStyleSheet(
        "QListWidget {"
        "  background-color: black;"
        "  color: green;"
        "}"
        "QScrollBar:vertical {"
        "  background-color: #1a1a1a;"
        "  width: 12px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #006400;"  /* dark green handle */
        "  min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  background: none;"
        "}");

    /* visibleItemCount: 22 */
    QFontMetrics fm(m_varList->font());
    m_varList->setMinimumHeight(fm.height() * 22 + 4);

    QVBoxLayout* rightCol = new QVBoxLayout;
    rightCol->setSpacing(4);
    rightCol->addWidget(m_varList);
    rightCol->addWidget(m_acceptBtn, 0, Qt::AlignHCenter);

    hbox->addLayout(rightCol);

    /* ---- Connections ---- */
    connect(m_acceptBtn, &QPushButton::clicked,
            this, &VaredMainWindow::onAccept);
    connect(m_varList, &QListWidget::itemClicked,
            this, &VaredMainWindow::onVariableSelected);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::loadFile(const QString& path)
{
    /* Mirrors OpenNewFile_OK() in ccb.cc */
    m_vdbConverter.open(&m_vdbFile, path.toStdString());

    if (!m_vdbFile.is_valid()) {
        QMessageBox::critical(this, "Error",
            QString("Can't initialize variable database:\n%1").arg(path));
        return;
    }

    /* Determine XML save path */
    std::string spath = path.toStdString();
    if (spath.size() >= 4 && spath.substr(spath.size() - 4) == ".xml")
        m_xmlSavePath = spath;
    else
        m_xmlSavePath = m_vdbConverter.defaultOutputPath();

    rebuildSortedVars();
    refreshList();
    populateCombos();

    m_changesMade = false;
    setWindowTitle(QString("Variable DataBase Editor — %1")
                   .arg(QString::fromStdString(spath)));
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::rebuildSortedVars()
{
    m_sortedVars.clear();
    for (int i = 0; i < m_vdbFile.num_vars(); ++i)
        m_sortedVars.push_back(m_vdbFile.get_var(i));

    std::sort(m_sortedVars.begin(), m_sortedVars.end(),
        [](VDBVar* a, VDBVar* b){ return a->name() < b->name(); });
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::refreshList()
{
    m_varList->clear();
    for (auto* v : m_sortedVars)
        m_varList->addItem(QString::fromStdString(v->name()));
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::populateCombos()
{
    /* Mirrors the category/stdName button-rebuild logic in OpenNewFile_OK() */
    m_categoryCombo->clear();
    m_categoryNames = m_vdbFile.get_categories();
    if (m_categoryNames.empty() || m_categoryNames[0] != "None")
        m_categoryNames.insert(m_categoryNames.begin(), "None");
    for (const auto& s : m_categoryNames)
        m_categoryCombo->addItem(QString::fromStdString(s));

    m_stdNameCombo->clear();
    m_stdNames = m_vdbFile.get_standard_names();
    if (m_stdNames.empty() || m_stdNames[0] != "None")
        m_stdNames.insert(m_stdNames.begin(), "None");
    for (const auto& s : m_stdNames)
        m_stdNameCombo->addItem(QString::fromStdString(s));
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::populateFields(VDBVar* var)
{
    /* Mirrors EditVariable() in ccb.cc */
    m_nameEdit->setText(QString::fromStdString(var->name()));
    m_titleEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::LONG_NAME)));
    m_unitsEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::UNITS)));
    m_altUnitsEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::ALTERNATE_UNITS)));

    std::string vrLo, vrHi;
    parsePair(var->get_attribute(VDBVar::VOLTAGE_RANGE), vrLo, vrHi);
    m_voltLoEdit->setText(QString::fromStdString(vrLo));
    m_voltHiEdit->setText(QString::fromStdString(vrHi));

    m_sampleRateEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::DEFAULT_SAMPLE_RATE)));
    m_minLimitEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::MIN_LIMIT)));
    m_maxLimitEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::MAX_LIMIT)));

    std::string crLo, crHi;
    parsePair(var->get_attribute(VDBVar::CAL_RANGE), crLo, crHi);
    m_calLoEdit->setText(QString::fromStdString(crLo));
    m_calHiEdit->setText(QString::fromStdString(crHi));

    m_analogCheck->setChecked(
        var->get_attribute_value<bool>(VDBVar::IS_ANALOG));

    m_categoryCombo->setCurrentIndex(
        findIndex(m_categoryNames, var->get_attribute(VDBVar::CATEGORY)));
    m_stdNameCombo->setCurrentIndex(
        findIndex(m_stdNames, var->get_attribute(VDBVar::STANDARD_NAME)));

    m_referenceCheck->setChecked(
        var->get_attribute_value<bool>(VDBVar::REFERENCE));
}

/* -------------------------------------------------------------------- */
/* Slots                                                                  */
/* -------------------------------------------------------------------- */

void VaredMainWindow::onVariableSelected(QListWidgetItem* item)
{
    if (!item) return;
    int pos = m_varList->row(item);
    if (pos < 0 || pos >= (int)m_sortedVars.size()) return;
    populateFields(m_sortedVars[pos]);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onAccept()
{
    /* Mirrors Accept() in ccb.cc */
    std::string varName = m_nameEdit->text().toUpper().toStdString();
    if (varName.empty()) return;
    m_nameEdit->setText(QString::fromStdString(varName));

    /* Preserve scroll position (mirrors XmNtopItemPosition logic) */
    int scrollVal = m_varList->verticalScrollBar()->value();

    VDBVar* var = m_vdbFile.get_var(varName);
    if (!var)
        var = m_vdbFile.add_var(varName);

    var->set_attribute(VDBVar::LONG_NAME,   m_titleEdit->text().toStdString());
    var->set_attribute(VDBVar::UNITS,       m_unitsEdit->text().toStdString());

    std::string altU = m_altUnitsEdit->text().toStdString();
    if (!altU.empty())
        var->set_attribute(VDBVar::ALTERNATE_UNITS, altU);

    bool isAnalog = m_analogCheck->isChecked();
    var->set_attribute(VDBVar::IS_ANALOG, isAnalog);

    std::string vrLo = m_voltLoEdit->text().toStdString();
    std::string vrHi = m_voltHiEdit->text().toStdString();
    if (isAnalog && (!vrLo.empty() || !vrHi.empty()))
        var->set_attribute(VDBVar::VOLTAGE_RANGE, vrLo + " " + vrHi);

    std::string sr = m_sampleRateEdit->text().toStdString();
    if (isAnalog && !sr.empty())
        var->set_attribute(VDBVar::DEFAULT_SAMPLE_RATE, sr);

    std::string minL = m_minLimitEdit->text().toStdString();
    if (!minL.empty())
        var->set_attribute(VDBVar::MIN_LIMIT, minL);
    std::string maxL = m_maxLimitEdit->text().toStdString();
    if (!maxL.empty())
        var->set_attribute(VDBVar::MAX_LIMIT, maxL);

    std::string crLo = m_calLoEdit->text().toStdString();
    std::string crHi = m_calHiEdit->text().toStdString();
    if (!crLo.empty() || !crHi.empty())
        var->set_attribute(VDBVar::CAL_RANGE, crLo + " " + crHi);

    int catIdx = m_categoryCombo->currentIndex();
    if (catIdx > 0 && catIdx < (int)m_categoryNames.size())
        var->set_attribute(VDBVar::CATEGORY, m_categoryNames[catIdx]);

    int snIdx = m_stdNameCombo->currentIndex();
    if (snIdx > 0 && snIdx < (int)m_stdNames.size())
        var->set_attribute(VDBVar::STANDARD_NAME, m_stdNames[snIdx]);

    var->set_attribute(VDBVar::REFERENCE, m_referenceCheck->isChecked());

    rebuildSortedVars();
    refreshList();

    /* Re-select the variable and restore scroll position */
    int selectRow = -1;
    for (int i = 0; i < (int)m_sortedVars.size(); ++i) {
        if (m_sortedVars[i]->name() == varName) { selectRow = i; break; }
    }
    if (selectRow >= 0)
        m_varList->setCurrentRow(selectRow);
    m_varList->verticalScrollBar()->setValue(scrollVal);

    m_changesMade = true;
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onClear()
{
    /* Mirrors Clear() in ccb.cc */
    m_nameEdit->clear();
    m_titleEdit->clear();
    m_unitsEdit->clear();
    m_altUnitsEdit->clear();
    m_voltLoEdit->clear();
    m_voltHiEdit->clear();
    m_sampleRateEdit->clear();
    m_minLimitEdit->clear();
    m_maxLimitEdit->clear();
    m_calLoEdit->clear();
    m_calHiEdit->clear();
    m_analogCheck->setChecked(false);
    m_referenceCheck->setChecked(false);
    m_categoryCombo->setCurrentIndex(0);
    m_stdNameCombo->setCurrentIndex(0);
    m_varList->clearSelection();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onDelete()
{
    /* Mirrors Delete() in ccb.cc */
    QListWidgetItem* item = m_varList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Warning", "No variable selected to delete.");
        return;
    }
    int pos = m_varList->row(item);
    if (pos < 0 || pos >= (int)m_sortedVars.size()) return;

    m_vdbFile.remove_var(m_sortedVars[pos]->name());
    rebuildSortedVars();
    refreshList();
    m_changesMade = true;
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onReset()
{
    /* "Reset Variable" = re-populate fields from the currently selected var */
    onVariableSelected(m_varList->currentItem());
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFileOpen()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Open Variable Database", QString(),
        "VarDB files (*.xml VarDB);;All files (*)");
    if (!path.isEmpty())
        loadFile(path);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFileSave()
{
    if (m_xmlSavePath.empty()) { onFileSaveAs(); return; }
    try {
        m_vdbFile.save(m_xmlSavePath);
        m_changesMade = false;
    } catch (...) {
        QMessageBox::critical(this, "Error", "Error trying to save, aborted.");
    }
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFileSaveAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save As", QString::fromStdString(m_xmlSavePath),
        "XML files (*.xml);;All files (*)");
    if (path.isEmpty()) return;
    try {
        m_vdbFile.save(path.toStdString());
        m_xmlSavePath = path.toStdString();
        m_changesMade = false;
    } catch (...) {
        QMessageBox::critical(this, "Error", "Error trying to save, aborted.");
    }
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::closeEvent(QCloseEvent* e)
{
    /* Mirrors Quit() in ccb.cc — warn if unsaved */
    if (m_changesMade) {
        auto btn = QMessageBox::question(
            this, "Unsaved Changes",
            "You have not saved this file.\nQuit anyway?",
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (btn != QMessageBox::Ok) { e->ignore(); return; }
    }
    e->accept();
}
