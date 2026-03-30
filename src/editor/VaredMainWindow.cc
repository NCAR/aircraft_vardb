/*
 * VaredMainWindow.cc — Qt replacement for the Motif vared GUI.
 *
 * Logic faithfully ported from:
 *   ccb.cc  — callbacks (Accept, Clear, Delete, EditVariable, OpenNewFile_OK, …)
 *   initv.cc — Initialize()
 *   Xwin.cc  — CreateMainWindow() layout
 *   fbr.h    — resource strings (labels, sizes, colours)
 *
 * Changes from original Qt port:
 *   - Variable list replaced with a QTabWidget holding separate "Raw (N)" and
 *     "Derived (N)" QListWidgets.  A variable is derived when it carries a
 *     non-empty DERIVE attribute.
 *   - Dependency tree (QTreeWidget) shows the full recursive dependency graph,
 *     expanding automatically when depth ≤ 3.
 *   - Status bar shows current selection, raw/derived counts, unsaved flag.
 *   - List items carry tooltips showing title and units on hover.
 */

#include "VaredMainWindow.h"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSplitter>
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

/* Helper: true when text is empty or parses as a finite double          */
static bool isValidFloat(const QString& text)
{
    if (text.trimmed().isEmpty()) return true;
    bool ok = false;
    text.toDouble(&ok);
    return ok;
}

/* Helper: true when var has non-empty DEPENDENCIES — i.e. it is computed
 * from other variables.  A RAW variable has no dependencies (it comes from
 * a sensor) but may still carry a DERIVE list naming variables that use it
 * as an input; that forward-reference does not make it derived. */
static bool isDerived(VDBVar* var)
{
    return var->has_attribute(VDBVar::DEPENDENCIES)
        && !var->get_attribute(VDBVar::DEPENDENCIES).empty();
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

    /* ---- Central widget: container holding splitter + find bar ---- */
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    splitter->setContentsMargins(8, 8, 8, 8);
    splitter->setHandleWidth(6);
    centralLayout->addWidget(splitter, 1);

    /* Find bar — hidden until Ctrl+F */
    m_findBar = new QFrame;
    m_findBar->setFrameShape(QFrame::StyledPanel);
    m_findBar->setStyleSheet(
        "QFrame { background: #1a1a1a; border-top: 1px solid #006400; }"
        "QLabel { color: #00cc00; padding: 0 4px; }"
        "QLineEdit { background: black; color: #00cc00;"
        "            border: 1px solid #006400; padding: 2px 4px; }"
        "QPushButton { background: #2a2a2a; color: #00cc00;"
        "              border: 1px solid #006400; padding: 2px 8px; }");
    QHBoxLayout* findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(8, 4, 8, 4);
    findLayout->setSpacing(6);

    m_findEdit  = new QLineEdit;
    m_findEdit->setPlaceholderText("search name, title, units, category...");
    m_findEdit->setMinimumWidth(280);
    m_findCount = new QLabel("0 matches");
    QPushButton* findClose = new QPushButton("Close");

    findLayout->addWidget(new QLabel("Find:"));
    findLayout->addWidget(m_findEdit, 1);
    findLayout->addWidget(m_findCount);
    findLayout->addWidget(findClose);

    centralLayout->addWidget(m_findBar);

    QWidget* leftPane  = new QWidget;
    QWidget* rightPane = new QWidget;
    splitter->addWidget(leftPane);
    splitter->addWidget(rightPane);

    QVBoxLayout* leftBox = new QVBoxLayout(leftPane);
    leftBox->setContentsMargins(0, 0, 4, 0);
    leftBox->setSpacing(0);

    /* ---- Left: form layout ---- */
    const QString fieldStyle =
        "background-color: #DEB887; color: black;"
        "padding-left: 4px; padding-right: 4px;";

    auto makeEdit = [&](int maxLen) -> QLineEdit* {
        QLineEdit* e = new QLineEdit;
        e->setMaxLength(maxLen);
        e->setStyleSheet(fieldStyle);
        return e;
    };

    m_nameEdit       = makeEdit(16);
    /* Force uppercase as the user types; preserve cursor position */
    connect(m_nameEdit, &QLineEdit::textEdited, this, [this](const QString& t) {
        int pos = m_nameEdit->cursorPosition();
        m_nameEdit->setText(t.toUpper());
        m_nameEdit->setCursorPosition(pos);
    });
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
    leftBox->addLayout(form);
    leftBox->addStretch();

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
    form->addRow("",                     m_acceptBtn);

    /* Enforce label minimum width (mirrors *editFieldRC*XmLabel.width: 200) */
    for (int i = 0; i < form->rowCount(); ++i) {
        QLayoutItem* li = form->itemAt(i, QFormLayout::LabelRole);
        if (li && li->widget())
            li->widget()->setMinimumWidth(200);
    }

    /* ---- Right: tabbed variable lists + dependency tree ---- */

    /* Shared style for both list widgets — mirrors original black/green theme */
    const QString listStyle =
        "QListWidget {"
        "  background-color: black;"
        "  color: green;"
        "}"
        "QScrollBar:vertical {"
        "  background-color: #1a1a1a;"
        "  width: 12px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #006400;"
        "  min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  background: none;"
        "}";

    m_rawList = new QListWidget;
    m_rawList->setStyleSheet(listStyle);

    m_derivedList = new QListWidget;
    m_derivedList->setStyleSheet(listStyle);

    /* visibleItemCount: 22 (matches original fbr.h list height) */
    QFontMetrics fm(m_rawList->font());
    const int listHeight = fm.height() * 22 + 4;
    m_rawList->setMinimumHeight(listHeight);
    m_derivedList->setMinimumHeight(listHeight);

    /* Filter boxes — styled to match the list: black bg, green text */
    const QString filterStyle =
        "QLineEdit {"
        "  background-color: black;"
        "  color: #00cc00;"
        "  border: none;"
        "  border-bottom: 1px solid #006400;"
        "  padding: 2px 4px;"
        "}";

    m_rawFilter = new QLineEdit;
    m_rawFilter->setPlaceholderText("filter...");
    m_rawFilter->setStyleSheet(filterStyle);

    m_derivedFilter = new QLineEdit;
    m_derivedFilter->setPlaceholderText("filter...");
    m_derivedFilter->setStyleSheet(filterStyle);

    /* Each tab holds a container: filter on top, list below */
    auto makeTabPage = [](QLineEdit* filter, QListWidget* list) -> QWidget* {
        QWidget* page = new QWidget;
        QVBoxLayout* vb = new QVBoxLayout(page);
        vb->setContentsMargins(0, 0, 0, 0);
        vb->setSpacing(0);
        vb->addWidget(filter);
        vb->addWidget(list);
        return page;
    };

    m_varTabs = new QTabWidget;
    m_varTabs->addTab(makeTabPage(m_rawFilter,     m_rawList),     "Raw");
    m_varTabs->addTab(makeTabPage(m_derivedFilter, m_derivedList), "Derived");
    m_varTabs->tabBar()->setExpanding(false);  // let each tab size to its text
    m_varTabs->setStyleSheet(
        "QTabWidget::pane { border: 2px solid #00cc00; background: black; }"
        "QTabBar::tab {"
        "  background: #2a2a2a;"
        "  color: #00cc00;"
        "  padding: 6px 20px;"
        "  border: 1px solid #006400;"
        "  border-bottom: none;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #00cc00;"
        "  color: black;"
        "  border: 1px solid #00ff00;"
        "  border-bottom: none;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  background: #003300;"
        "}");

    /* Dependency tree — always visible, expands below Accept button */
    m_depTree = new QTreeWidget;
    m_depTree->setHeaderLabel("Dependencies");
    m_depTree->setMinimumHeight(fm.height() * 8 + 4);
    m_depTree->setStyleSheet(
        "QTreeWidget {"
        "  background-color: black;"
        "  color: green;"
        "}"
        "QHeaderView::section {"
        "  background-color: #1a1a1a;"
        "  color: #00aa00;"
        "  padding: 2px;"
        "}"
        "QScrollBar:vertical {"
        "  background-color: #1a1a1a;"
        "  width: 12px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background-color: #006400;"
        "  min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  background: none;"
        "}");

    QVBoxLayout* rightCol = new QVBoxLayout(rightPane);
    rightCol->setContentsMargins(4, 0, 0, 0);
    rightCol->setSpacing(4);
    rightCol->addWidget(m_varTabs);
    rightCol->addWidget(m_depTree);

    /* ---- Connections ---- */
    connect(m_acceptBtn,  &QPushButton::clicked,
            this, &VaredMainWindow::onAccept);
    connect(m_rawList,    &QListWidget::itemClicked,
            this, &VaredMainWindow::onVariableSelected);
    connect(m_derivedList,&QListWidget::itemClicked,
            this, &VaredMainWindow::onVariableSelected);

    /* Context menus */
    m_depTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_depTree, &QTreeWidget::customContextMenuRequested,
            this, &VaredMainWindow::onTreeContextMenu);

    for (QListWidget* list : {m_rawList, m_derivedList}) {
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list, &QListWidget::customContextMenuRequested,
                this, &VaredMainWindow::onListContextMenu);
    }

    /* Arrow-key navigation: while focus is in a line edit, Up/Down move the
     * active list selection and load the new variable without leaving the field. */
    for (QListWidget* list : {m_rawList, m_derivedList}) {
        connect(list, &QListWidget::currentItemChanged,
                this, [this](QListWidgetItem* cur, QListWidgetItem*) {
                    if (cur) onVariableSelected(cur);
                });
    }

    /* Filter boxes: narrow the list as the user types */
    connect(m_rawFilter,     &QLineEdit::textChanged,
            this, [this](const QString& t){ applyFilter(m_rawList,     t); });
    connect(m_derivedFilter, &QLineEdit::textChanged,
            this, [this](const QString& t){ applyFilter(m_derivedList, t); });

    /* Escape inside a filter box: clear filter then focus the list */
    for (auto pair : {std::make_pair(m_rawFilter,     m_rawList),
                      std::make_pair(m_derivedFilter, m_derivedList)}) {
        QLineEdit*   filter = pair.first;
        QListWidget* list   = pair.second;
        auto* esc = new QShortcut(Qt::Key_Escape, filter,
                                  nullptr, nullptr, Qt::WidgetShortcut);
        connect(esc, &QShortcut::activated, this, [filter, list]() {
            filter->clear();
            list->setFocus();
        });
    }

    /* Enter anywhere in the form → Accept */
    auto* acceptShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    acceptShortcut->setContext(Qt::WindowShortcut);
    connect(acceptShortcut, &QShortcut::activated, this, &VaredMainWindow::onAccept);

    /* Escape or F6 from anywhere → focus active list */
    auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escShortcut->setContext(Qt::WindowShortcut);
    connect(escShortcut, &QShortcut::activated, this, &VaredMainWindow::onFocusList);

    auto* f6Shortcut = new QShortcut(QKeySequence(Qt::Key_F6), this);
    f6Shortcut->setContext(Qt::WindowShortcut);
    connect(f6Shortcut, &QShortcut::activated, this, &VaredMainWindow::onFocusList);

    /* Real-time numeric validation: highlight field on bad input */
    for (QLineEdit* field : {m_voltLoEdit, m_voltHiEdit,
                             m_sampleRateEdit,
                             m_minLimitEdit,  m_maxLimitEdit,
                             m_calLoEdit,     m_calHiEdit}) {
        connect(field, &QLineEdit::textChanged, this, [this, field](const QString& t) {
            setFieldValid(field, isValidFloat(t));
        });
    }

    auto* undoShortcut = new QShortcut(QKeySequence("Ctrl+Z"), this);
    undoShortcut->setContext(Qt::WindowShortcut);
    connect(undoShortcut, &QShortcut::activated, this, &VaredMainWindow::onUndo);

    /* Ctrl+F: show find bar */
    auto* findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    findShortcut->setContext(Qt::WindowShortcut);
    connect(findShortcut, &QShortcut::activated, this, &VaredMainWindow::onFindActivated);

    connect(m_findEdit, &QLineEdit::textChanged, this, &VaredMainWindow::onFindChanged);

    /* Close button and Escape both dismiss the find bar */
    connect(findClose, &QPushButton::clicked, this, &VaredMainWindow::onFindClosed);
    auto* findEsc = new QShortcut(Qt::Key_Escape, m_findEdit,
                                  nullptr, nullptr, Qt::WidgetShortcut);
    connect(findEsc, &QShortcut::activated, this, &VaredMainWindow::onFindClosed);

    setupAccessibility();
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
    m_addedVars.clear();
    m_modifiedVars.clear();
    m_deletedVars.clear();
    setWindowTitle(QString("Variable DataBase Editor — %1")
                   .arg(QString::fromStdString(spath)));
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::rebuildSortedVars()
{
    m_sortedVars.clear();
    m_varLookup.clear();

    for (int i = 0; i < m_vdbFile.num_vars(); ++i) {
        VDBVar* v = m_vdbFile.get_var(i);
        m_sortedVars.push_back(v);
        m_varLookup[v->name()] = v;
    }

    std::sort(m_sortedVars.begin(), m_sortedVars.end(),
        [](VDBVar* a, VDBVar* b){ return a->name() < b->name(); });
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::refreshList()
{
    m_rawList->clear();
    m_derivedList->clear();

    for (auto* v : m_sortedVars) {
        QString name  = QString::fromStdString(v->name());
        QString title = QString::fromStdString(v->get_attribute(VDBVar::LONG_NAME));
        QString units = QString::fromStdString(v->get_attribute(VDBVar::UNITS));

        /* Tooltip shows title and units without requiring a click */
        QString tip = title.isEmpty() ? name : title;
        if (!units.isEmpty())
            tip += QString("  [%1]").arg(units);

        auto* item = new QListWidgetItem(name);
        item->setToolTip(tip);

        if (::isDerived(v))
            m_derivedList->addItem(item);
        else
            m_rawList->addItem(item);
    }

    updateTabCounts();
    updateStatusBar();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::updateTabCounts()
{
    m_varTabs->setTabText(0, QString("Raw (%1)").arg(m_rawList->count()));
    m_varTabs->setTabText(1, QString("Derived (%1)").arg(m_derivedList->count()));
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::updateStatusBar()
{
    int total = m_rawList->count() + m_derivedList->count();

    QString msg = QString("%1 raw, %2 derived")
                  .arg(m_rawList->count())
                  .arg(m_derivedList->count());

    if (total > 0 && !m_selectedVarName.isEmpty())
        msg += QString("  |  %1").arg(m_selectedVarName);

    if (m_changesMade)
        msg += "  ●  unsaved changes";

    statusBar()->showMessage(msg);
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
    /* Reset any validation highlighting from a previous edit */
    for (QLineEdit* field : {m_voltLoEdit, m_voltHiEdit,
                             m_sampleRateEdit,
                             m_minLimitEdit,  m_maxLimitEdit,
                             m_calLoEdit,     m_calHiEdit})
        setFieldValid(field, true);

    /* Mirrors EditVariable() in ccb.cc */
    m_nameEdit->setText(QString::fromStdString(var->name()));
    m_titleEdit->setText(QString::fromStdString(
        var->get_attribute(VDBVar::LONG_NAME)));
    m_titleEdit->setCursorPosition(0);  // show start of text, not end
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
void VaredMainWindow::refreshDepTree(VDBVar* var)
{
    m_depTree->clear();
    const QString varName = QString::fromStdString(var->name());
    m_depTree->setHeaderLabel(QString("%1 relationships").arg(varName));

    /* Root node is always the selected variable itself */
    QTreeWidgetItem* root = new QTreeWidgetItem(m_depTree);
    root->setText(0, ::isDerived(var) ? varName : varName + "  [raw]");

    int maxDepth = 0;

    /* ---- "Depends on" section: what this variable requires as inputs ---- */
    QString depsStr = QString::fromStdString(var->get_attribute(VDBVar::DEPENDENCIES));
    const QStringList deps = depsStr.split(' ', Qt::SkipEmptyParts);
    if (!deps.isEmpty()) {
        QTreeWidgetItem* depsHeader = new QTreeWidgetItem(root);
        depsHeader->setText(0, "depends on");
        for (const QString& dep : deps) {
            QTreeWidgetItem* item = new QTreeWidgetItem(depsHeader);
            item->setText(0, dep);
            QSet<QString> visited;
            visited.insert(varName);
            int d = buildDepTree(item, dep, visited);
            maxDepth = std::max(maxDepth, d + 2);
        }
    }

    /* ---- "Used to derive" section: variables that use this one as an input ---- */
    QString deriveStr = QString::fromStdString(var->get_attribute(VDBVar::DERIVE));
    const QStringList derived = deriveStr.split(' ', Qt::SkipEmptyParts);
    if (!derived.isEmpty()) {
        QTreeWidgetItem* deriveHeader = new QTreeWidgetItem(root);
        deriveHeader->setText(0, "used to derive");
        for (const QString& d : derived) {
            QTreeWidgetItem* item = new QTreeWidgetItem(deriveHeader);
            item->setText(0, d);
        }
        maxDepth = std::max(maxDepth, 2);
    }

    if (maxDepth <= 4)
        m_depTree->expandAll();
}

/* -------------------------------------------------------------------- */
int VaredMainWindow::buildDepTree(QTreeWidgetItem* parent,
                                   const QString& varName,
                                   QSet<QString>& visited)
{
    /* Cycle on the current path — label the node and stop recursing */
    if (visited.contains(varName)) {
        parent->setText(0, varName + " [cycle]");
        return 0;
    }

    auto it = m_varLookup.find(varName.toStdString());
    if (it == m_varLookup.end())
        return 0;   // Unknown variable — treat as leaf

    VDBVar* var = it->second;
    if (!::isDerived(var))
        return 0;   // Raw variable — leaf, no children to add

    visited.insert(varName);

    QString deriveStr = QString::fromStdString(var->get_attribute(VDBVar::DEPENDENCIES));
    const QStringList deps = deriveStr.split(' ', Qt::SkipEmptyParts);

    int maxChildDepth = 0;
    for (const QString& dep : deps) {
        QTreeWidgetItem* child = new QTreeWidgetItem(parent);
        child->setText(0, dep);
        int d = buildDepTree(child, dep, visited);
        maxChildDepth = std::max(maxChildDepth, d);
    }

    /* Allow this variable in other independent branches of the tree */
    visited.remove(varName);

    return maxChildDepth + 1;
}

/* -------------------------------------------------------------------- */
/* Slots                                                                  */
/* -------------------------------------------------------------------- */

void VaredMainWindow::onVariableSelected(QListWidgetItem* item)
{
    if (!item) return;

    /* Clear selection in the other list so only one var is highlighted */
    if (item->listWidget() == m_rawList)
        m_derivedList->clearSelection();
    else
        m_rawList->clearSelection();

    auto it = m_varLookup.find(item->text().toStdString());
    if (it == m_varLookup.end()) return;

    m_selectedVarName = item->text();
    populateFields(it->second);
    refreshDepTree(it->second);
    updateStatusBar();

    /* Notify screen readers that the name field value changed.
     * VoiceOver and NVDA will announce the new variable name. */
    QAccessibleValueChangeEvent nameEvent(m_nameEdit, m_nameEdit->text());
    QAccessible::updateAccessibility(&nameEvent);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onAccept()
{
    /* Mirrors Accept() in ccb.cc */
    std::string varName = m_nameEdit->text().toUpper().toStdString();
    if (varName.empty()) return;
    m_nameEdit->setText(QString::fromStdString(varName));

    if (!validateFields()) return;

    /* Snapshot current state for single-level undo */
    VarSnapshot snap;
    snap.name = varName;
    VDBVar* existing = m_vdbFile.get_var(varName);
    snap.wasNew = (existing == nullptr);
    if (existing) {
        snap.longName     = existing->get_attribute(VDBVar::LONG_NAME);
        snap.units        = existing->get_attribute(VDBVar::UNITS);
        snap.altUnits     = existing->get_attribute(VDBVar::ALTERNATE_UNITS);
        snap.voltageRange = existing->get_attribute(VDBVar::VOLTAGE_RANGE);
        snap.sampleRate   = existing->get_attribute(VDBVar::DEFAULT_SAMPLE_RATE);
        snap.minLimit     = existing->get_attribute(VDBVar::MIN_LIMIT);
        snap.maxLimit     = existing->get_attribute(VDBVar::MAX_LIMIT);
        snap.calRange     = existing->get_attribute(VDBVar::CAL_RANGE);
        snap.category     = existing->get_attribute(VDBVar::CATEGORY);
        snap.standardName = existing->get_attribute(VDBVar::STANDARD_NAME);
        snap.isAnalog     = existing->get_attribute_value<bool>(VDBVar::IS_ANALOG);
        snap.isReference  = existing->get_attribute_value<bool>(VDBVar::REFERENCE);
    }
    m_undoSnapshot = snap;

    /* Record for quit summary */
    QString qname = QString::fromStdString(varName);
    if (snap.wasNew) {
        m_addedVars.insert(qname);
    } else {
        m_modifiedVars.insert(qname);
        m_deletedVars.remove(qname);  // un-delete if previously deleted
    }

    /* Preserve scroll positions in both lists */
    int rawScroll     = m_rawList->verticalScrollBar()->value();
    int derivedScroll = m_derivedList->verticalScrollBar()->value();

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

    /* Restore scroll positions */
    m_rawList->verticalScrollBar()->setValue(rawScroll);
    m_derivedList->verticalScrollBar()->setValue(derivedScroll);

    /* Re-select the variable in the correct tab */
    VDBVar* saved = m_vdbFile.get_var(varName);
    QListWidget* targetList = (saved && ::isDerived(saved))
                              ? m_derivedList : m_rawList;
    m_varTabs->setCurrentWidget(targetList);

    QList<QListWidgetItem*> found = targetList->findItems(
        QString::fromStdString(varName), Qt::MatchExactly);
    if (!found.isEmpty())
        targetList->setCurrentItem(found.first());

    m_changesMade = true;
    updateStatusBar();
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
    m_rawList->clearSelection();
    m_derivedList->clearSelection();
    m_depTree->clear();
    m_depTree->setHeaderLabel("Dependencies");
    m_selectedVarName.clear();
    updateStatusBar();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onDelete()
{
    /* Mirrors Delete() in ccb.cc */
    QListWidget* activeList = (m_varTabs->currentIndex() == 0)
                              ? m_rawList : m_derivedList;
    QListWidgetItem* item = activeList->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Warning", "No variable selected to delete.");
        return;
    }

    auto it = m_varLookup.find(item->text().toStdString());
    if (it == m_varLookup.end()) return;

    QString qname = item->text();
    m_vdbFile.remove_var(it->second->name());

    /* A variable added then deleted this session leaves no net change */
    if (!m_addedVars.remove(qname))
        m_deletedVars.insert(qname);
    m_modifiedVars.remove(qname);

    rebuildSortedVars();
    refreshList();
    m_depTree->clear();
    m_depTree->setHeaderLabel("Dependencies");
    m_selectedVarName.clear();
    m_changesMade = true;
    updateStatusBar();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onReset()
{
    /* "Reset Variable" = re-populate fields from the currently selected var */
    QListWidget* activeList = (m_varTabs->currentIndex() == 0)
                              ? m_rawList : m_derivedList;
    onVariableSelected(activeList->currentItem());
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::setupAccessibility()
{
    /* ---- Accessible names ---- */
    m_nameEdit->setAccessibleName("Variable name");
    m_nameEdit->setAccessibleDescription("Short identifier, up to 16 characters. Automatically uppercased.");

    m_titleEdit->setAccessibleName("Title");
    m_titleEdit->setAccessibleDescription("Long descriptive name, up to 64 characters.");

    m_unitsEdit->setAccessibleName("Units");
    m_altUnitsEdit->setAccessibleName("Alternate units");

    m_voltLoEdit->setAccessibleName("Voltage range low");
    m_voltLoEdit->setAccessibleDescription("Low end of voltage range. Floating-point number.");
    m_voltHiEdit->setAccessibleName("Voltage range high");
    m_voltHiEdit->setAccessibleDescription("High end of voltage range. Floating-point number.");

    m_sampleRateEdit->setAccessibleName("Default sample rate");
    m_sampleRateEdit->setAccessibleDescription("Sample rate in Hz. Numeric value.");

    m_minLimitEdit->setAccessibleName("Minimum value");
    m_minLimitEdit->setAccessibleDescription("Lower bound for valid data. Floating-point number.");
    m_maxLimitEdit->setAccessibleName("Maximum value");
    m_maxLimitEdit->setAccessibleDescription("Upper bound for valid data. Floating-point number.");

    m_calLoEdit->setAccessibleName("Calibration range low");
    m_calLoEdit->setAccessibleDescription("Low end of calibration range. Floating-point number.");
    m_calHiEdit->setAccessibleName("Calibration range high");
    m_calHiEdit->setAccessibleDescription("High end of calibration range. Floating-point number.");

    m_analogCheck->setAccessibleName("Is analog");
    m_referenceCheck->setAccessibleName("Is reference variable");
    m_categoryCombo->setAccessibleName("Category");
    m_stdNameCombo->setAccessibleName("Standard name");
    m_acceptBtn->setAccessibleName("Accept");
    m_acceptBtn->setAccessibleDescription("Save changes to the current variable. Shortcut: Enter.");

    m_varTabs->setAccessibleName("Variable list");
    m_rawList->setAccessibleName("Raw variables");
    m_rawList->setAccessibleDescription("Variables with no DERIVE attribute.");
    m_derivedList->setAccessibleName("Derived variables");
    m_derivedList->setAccessibleDescription("Variables that depend on other variables.");

    m_rawFilter->setAccessibleName("Filter raw variables");
    m_rawFilter->setAccessibleDescription("Type to filter the raw variable list by name.");
    m_derivedFilter->setAccessibleName("Filter derived variables");
    m_derivedFilter->setAccessibleDescription("Type to filter the derived variable list by name.");

    m_depTree->setAccessibleName("Dependency tree");
    m_depTree->setAccessibleDescription(
        "Recursive dependency graph for the selected variable. "
        "Right-click a node to select that variable.");

    m_findEdit->setAccessibleName("Find across all attributes");
    m_findEdit->setAccessibleDescription(
        "Search name, title, units, alternate units, category, and standard name "
        "across both lists. Shortcut: Ctrl+F.");
    m_findCount->setAccessibleName("Find results count");

    /* ---- Tab order: form fields → accept → lists → find bar ---- */
    setTabOrder(m_nameEdit,       m_titleEdit);
    setTabOrder(m_titleEdit,      m_unitsEdit);
    setTabOrder(m_unitsEdit,      m_altUnitsEdit);
    setTabOrder(m_altUnitsEdit,   m_analogCheck);
    setTabOrder(m_analogCheck,    m_voltLoEdit);
    setTabOrder(m_voltLoEdit,     m_voltHiEdit);
    setTabOrder(m_voltHiEdit,     m_sampleRateEdit);
    setTabOrder(m_sampleRateEdit, m_minLimitEdit);
    setTabOrder(m_minLimitEdit,   m_maxLimitEdit);
    setTabOrder(m_maxLimitEdit,   m_calLoEdit);
    setTabOrder(m_calLoEdit,      m_calHiEdit);
    setTabOrder(m_calHiEdit,      m_categoryCombo);
    setTabOrder(m_categoryCombo,  m_stdNameCombo);
    setTabOrder(m_stdNameCombo,   m_referenceCheck);
    setTabOrder(m_referenceCheck, m_acceptBtn);
    setTabOrder(m_acceptBtn,      m_rawFilter);
    setTabOrder(m_rawFilter,      m_rawList);
    setTabOrder(m_rawList,        m_derivedFilter);
    setTabOrder(m_derivedFilter,  m_derivedList);
    setTabOrder(m_derivedList,    m_findEdit);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::applyFilter(QListWidget* list, const QString& text)
{
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        item->setHidden(!text.isEmpty() &&
                        !item->text().contains(text, Qt::CaseInsensitive));
    }
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFocusList()
{
    /* Move keyboard focus to whichever list tab is currently visible */
    QListWidget* activeList = (m_varTabs->currentIndex() == 0)
                              ? m_rawList : m_derivedList;
    activeList->setFocus();

    /* Ensure something is selected so arrow keys work immediately */
    if (activeList->currentItem() == nullptr && activeList->count() > 0)
        activeList->setCurrentRow(0);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::setFieldValid(QLineEdit* field, bool valid)
{
    /* Valid: burlywood.  Invalid: amber + thick border (shape + hue cue,
     * safe for deuteranopia/protanopia since amber is not confused with tan). */
    field->setStyleSheet(valid
        ? "background-color: #DEB887; color: black;"
          "padding-left: 4px; padding-right: 4px;"
        : "background-color: #FFC107; color: black;"
          "border: 2px solid #E65100;"
          "padding-left: 3px; padding-right: 3px;");
}

/* -------------------------------------------------------------------- */
bool VaredMainWindow::validateFields()
{
    /* Per-field numeric check */
    bool allNumeric = true;
    for (QLineEdit* field : {m_voltLoEdit, m_voltHiEdit,
                             m_sampleRateEdit,
                             m_minLimitEdit,  m_maxLimitEdit,
                             m_calLoEdit,     m_calHiEdit}) {
        bool ok = isValidFloat(field->text());
        setFieldValid(field, ok);
        if (!ok) allNumeric = false;
    }

    if (!allNumeric) {
        QMessageBox::warning(this, "Invalid Input",
            "One or more fields contain non-numeric values (highlighted in red).\n"
            "Fix them or clear them before accepting.");
        return false;
    }

    /* Cross-field range checks — warn but let the user override */
    QStringList warnings;

    auto checkRange = [&](QLineEdit* lo, QLineEdit* hi, const QString& label) {
        QString ls = lo->text().trimmed(), hs = hi->text().trimmed();
        if (ls.isEmpty() || hs.isEmpty()) return;
        if (ls.toDouble() > hs.toDouble())
            warnings << QString("%1 lo (%2) > hi (%3)").arg(label, ls, hs);
    };

    checkRange(m_voltLoEdit,  m_voltHiEdit,  "Voltage range");
    checkRange(m_minLimitEdit, m_maxLimitEdit, "Min/Max limit");
    checkRange(m_calLoEdit,   m_calHiEdit,   "Calibration range");

    if (!warnings.isEmpty()) {
        auto btn = QMessageBox::warning(this, "Range Warning",
            warnings.join("\n") + "\n\nSave anyway?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        return (btn == QMessageBox::Yes);
    }

    return true;
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFindActivated()
{
    m_findBar->show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFindChanged(const QString& text)
{
    int rawHits = 0, derivedHits = 0;

    for (auto* var : m_sortedVars) {
        bool match = text.isEmpty();
        if (!match) {
            /* Search across all text attributes */
            for (const std::string& attr : {
                    var->name(),
                    var->get_attribute(VDBVar::LONG_NAME),
                    var->get_attribute(VDBVar::UNITS),
                    var->get_attribute(VDBVar::ALTERNATE_UNITS),
                    var->get_attribute(VDBVar::CATEGORY),
                    var->get_attribute(VDBVar::STANDARD_NAME)}) {
                if (QString::fromStdString(attr).contains(text, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }

        QString name = QString::fromStdString(var->name());
        QListWidget* list = ::isDerived(var) ? m_derivedList : m_rawList;
        QList<QListWidgetItem*> found = list->findItems(name, Qt::MatchExactly);
        for (auto* item : found) {
            item->setHidden(!match);
            if (match) {
                ::isDerived(var) ? ++derivedHits : ++rawHits;
            }
        }
    }

    if (text.isEmpty()) {
        m_findCount->setText("0 matches");
    } else {
        int total = rawHits + derivedHits;
        m_findCount->setText(QString("%1 match%2 (%3 raw, %4 derived)")
                             .arg(total).arg(total == 1 ? "" : "es")
                             .arg(rawHits).arg(derivedHits));
    }

    /* Notify screen readers of the updated match count */
    QAccessibleEvent countEvent(m_findCount, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&countEvent);
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onFindClosed()
{
    m_findEdit->clear();
    /* Restore full lists (clear any find-driven hiding) */
    onFindChanged(QString());
    onFocusList();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onUndo()
{
    if (!m_undoSnapshot) {
        statusBar()->showMessage("Nothing to undo.");
        return;
    }

    const VarSnapshot& snap = *m_undoSnapshot;

    if (snap.wasNew) {
        /* Accept created this variable — remove it */
        m_vdbFile.remove_var(snap.name);
        statusBar()->showMessage(QString("Undo: removed %1")
                                 .arg(QString::fromStdString(snap.name)));
    } else {
        /* Restore previous attribute values */
        VDBVar* var = m_vdbFile.get_var(snap.name);
        if (!var) { m_undoSnapshot.reset(); return; }

        var->set_attribute(VDBVar::LONG_NAME,           snap.longName);
        var->set_attribute(VDBVar::UNITS,               snap.units);
        var->set_attribute(VDBVar::ALTERNATE_UNITS,     snap.altUnits);
        var->set_attribute(VDBVar::VOLTAGE_RANGE,       snap.voltageRange);
        var->set_attribute(VDBVar::DEFAULT_SAMPLE_RATE, snap.sampleRate);
        var->set_attribute(VDBVar::MIN_LIMIT,           snap.minLimit);
        var->set_attribute(VDBVar::MAX_LIMIT,           snap.maxLimit);
        var->set_attribute(VDBVar::CAL_RANGE,           snap.calRange);
        var->set_attribute(VDBVar::CATEGORY,            snap.category);
        var->set_attribute(VDBVar::STANDARD_NAME,       snap.standardName);
        var->set_attribute(VDBVar::IS_ANALOG,           snap.isAnalog);
        var->set_attribute(VDBVar::REFERENCE,           snap.isReference);
        statusBar()->showMessage(QString("Undo: restored %1")
                                 .arg(QString::fromStdString(snap.name)));
    }

    QString qname = QString::fromStdString(snap.name);
    if (snap.wasNew)
        m_addedVars.remove(qname);
    else
        m_modifiedVars.remove(qname);

    m_undoSnapshot.reset();  // single-level: consume the snapshot
    rebuildSortedVars();
    refreshList();
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_depTree->itemAt(pos);
    if (!item) return;

    /* Strip decorators added by the tree builder (" [raw]", " [cycle]") */
    QString varName = item->text(0).section("  ", 0, 0).trimmed();
    if (varName.isEmpty() || !m_varLookup.count(varName.toStdString()))
        return;

    QMenu menu(this);
    QAction* selectVar = menu.addAction(QString("Select %1").arg(varName));
    if (menu.exec(m_depTree->viewport()->mapToGlobal(pos)) != selectVar)
        return;

    /* Switch to the correct tab and select the variable */
    VDBVar* var = m_varLookup.at(varName.toStdString());
    QListWidget* targetList = ::isDerived(var) ? m_derivedList : m_rawList;
    m_varTabs->setCurrentWidget(targetList->parentWidget());

    QList<QListWidgetItem*> found = targetList->findItems(varName, Qt::MatchExactly);
    if (!found.isEmpty()) {
        targetList->setCurrentItem(found.first());
        onVariableSelected(found.first());
        targetList->scrollToItem(found.first());
    }
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::onListContextMenu(const QPoint& pos)
{
    QListWidget* list = qobject_cast<QListWidget*>(sender());
    if (!list) return;

    QListWidgetItem* item = list->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction* dup = menu.addAction(QString("Duplicate %1").arg(item->text()));
    if (menu.exec(list->viewport()->mapToGlobal(pos)) != dup)
        return;

    /* Pre-fill the form with the source variable's attributes.
     * Leave Name blank so the user is forced to choose a new name. */
    auto it = m_varLookup.find(item->text().toStdString());
    if (it == m_varLookup.end()) return;

    populateFields(it->second);
    m_nameEdit->clear();
    m_nameEdit->setFocus();
    statusBar()->showMessage(
        QString("Duplicating %1 — enter new name and Accept").arg(item->text()));
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
        updateStatusBar();
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
        updateStatusBar();
    } catch (...) {
        QMessageBox::critical(this, "Error", "Error trying to save, aborted.");
    }
}

/* -------------------------------------------------------------------- */
void VaredMainWindow::closeEvent(QCloseEvent* e)
{
    if (!m_changesMade) { e->accept(); return; }

    /* Build a human-readable summary of what changed this session */
    auto formatSet = [](const QString& heading, const QSet<QString>& names) -> QString {
        if (names.isEmpty()) return QString();
        QStringList sorted = names.values();
        sorted.sort();
        const int maxShow = 8;
        QString out = heading + ":\n";
        for (int i = 0; i < std::min(sorted.size(), maxShow); ++i)
            out += "  " + sorted[i] + "\n";
        if (sorted.size() > maxShow)
            out += QString("  … and %1 more\n").arg(sorted.size() - maxShow);
        return out;
    };

    QString summary;
    summary += formatSet(QString("Added (%1)").arg(m_addedVars.size()),    m_addedVars);
    summary += formatSet(QString("Modified (%1)").arg(m_modifiedVars.size()), m_modifiedVars);
    summary += formatSet(QString("Deleted (%1)").arg(m_deletedVars.size()), m_deletedVars);
    if (summary.isEmpty())
        summary = "(no tracked changes)\n";

    QMessageBox dlg(this);
    dlg.setWindowTitle("Unsaved Changes");
    dlg.setText("You have unsaved changes.");
    dlg.setDetailedText(summary.trimmed());
    dlg.setIcon(QMessageBox::Warning);

    QPushButton* saveBtn    = dlg.addButton("Save && Quit",       QMessageBox::AcceptRole);
    QPushButton* discardBtn = dlg.addButton("Discard Changes",    QMessageBox::DestructiveRole);
    /*QPushButton* cancelBtn =*/ dlg.addButton("Cancel",          QMessageBox::RejectRole);
    dlg.setDefaultButton(saveBtn);

    dlg.exec();

    if (dlg.clickedButton() == saveBtn) {
        onFileSave();
        e->accept();
    } else if (dlg.clickedButton() == discardBtn) {
        e->accept();
    } else {
        e->ignore();
    }
}
