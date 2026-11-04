#include "QueryTableWidget.h"
#include "schema/UserQuery.h"

#include <QHeaderView>
#include <QScrollBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include "ui/TableClipboardHelper.h"

QueryTableWidget::QueryTableWidget(const QString& title, const QString& description, QWidget* parent)
    : QWidget(parent), titleText(title), descText(description),
      addBtn(nullptr), editBtn(nullptr), deleteBtn(nullptr), extraActionBtn(nullptr)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    // 1. Header info
    titleLabel = new QLabel(titleText, this);
    titleLabel->setObjectName("viewTitle");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #0f172a;");

    descLabel = new QLabel(descText, this);
    descLabel->hide();

    mainLayout->addWidget(titleLabel);

    // 2. Setup dynamic filter bar (Rule 9.1)
    setupFilterUI(mainLayout);

    // 3. Setup secondary toolbar (Search, count, CRUD actions, refresh, export)
    setupToolbarUI(mainLayout);

    // 4. Setup table widget & sorting (Rule 11)
    setupTableUI(mainLayout);
}

void QueryTableWidget::setupFilterUI(QVBoxLayout* mainLayout)
{
    filterFrame = new QFrame(this);
    filterFrame->setStyleSheet(
        "QFrame {"
        "  background-color: #ffffff;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "}"
    );

    auto* filterLayout = new QHBoxLayout(filterFrame);
    filterLayout->setContentsMargins(12, 10, 12, 10);
    filterLayout->setSpacing(10);

    auto* filterIconLabel = new QLabel("🔍 Lọc dữ liệu:", filterFrame);
    filterIconLabel->setStyleSheet("font-size: 13px; font-weight: 600; color: #334155; border: none;");
    filterLayout->addWidget(filterIconLabel);

    filterColumnCombo = new QComboBox(filterFrame);
    filterColumnCombo->setMinimumWidth(210);
    filterColumnCombo->setStyleSheet(
        "QComboBox {"
        "  padding: 6px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #f8fafc;"
        "  font-size: 13px;"
        "  color: #0f172a;"
        "}"
        "QComboBox:focus { border: 1px solid #2563eb; background-color: #ffffff; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  padding: 4px;"
        "}"
    );
    filterColumnCombo->addItem("-- Không lọc (Toàn bộ) --");
    filterLayout->addWidget(filterColumnCombo);

    // Input 1: EXACT_TEXT (PK & Unique)
    filterExactInput = new QLineEdit(filterFrame);
    filterExactInput->setPlaceholderText("Nhập giá trị chính xác để tìm...");
    filterExactInput->setClearButtonEnabled(true);
    filterExactInput->setMinimumWidth(240);
    filterExactInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 6px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid #2563eb; }"
    );
    filterExactInput->hide();
    filterLayout->addWidget(filterExactInput);

    // Input 2: DATE_RANGE (Date from -> to)
    filterDateWidget = new QWidget(filterFrame);
    auto* dateLayout = new QHBoxLayout(filterDateWidget);
    dateLayout->setContentsMargins(0, 0, 0, 0);
    dateLayout->setSpacing(6);

    auto* fromLbl = new QLabel("Từ:", filterDateWidget);
    fromLbl->setStyleSheet("border: none; font-size: 12px; color: #475569; font-weight: 500;");
    fromDateEdit = new QDateEdit(filterDateWidget);
    fromDateEdit->setCalendarPopup(true);
    fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    fromDateEdit->setDate(QDate(2023, 1, 1));
    fromDateEdit->setStyleSheet(
        "QDateEdit {"
        "  padding: 5px 8px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 12px;"
        "}"
        "QCalendarWidget QWidget {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "}"
    );

    auto* toLbl = new QLabel("Đến:", filterDateWidget);
    toLbl->setStyleSheet("border: none; font-size: 12px; color: #475569; font-weight: 500;");
    toDateEdit = new QDateEdit(filterDateWidget);
    toDateEdit->setCalendarPopup(true);
    toDateEdit->setDisplayFormat("yyyy-MM-dd");
    toDateEdit->setDate(QDate(2026, 12, 31));
    toDateEdit->setStyleSheet(
        "QDateEdit {"
        "  padding: 5px 8px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 12px;"
        "}"
        "QCalendarWidget QWidget {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "}"
    );

    dateLayout->addWidget(fromLbl);
    dateLayout->addWidget(fromDateEdit);
    dateLayout->addWidget(toLbl);
    dateLayout->addWidget(toDateEdit);
    filterDateWidget->hide();
    filterLayout->addWidget(filterDateWidget);

    // Input 3: SELECT_OR_TYPE (is_sold, status, type, etc.)
    filterSelectCombo = new QComboBox(filterFrame);
    filterSelectCombo->setEditable(true);
    filterSelectCombo->setMinimumWidth(220);
    filterSelectCombo->setStyleSheet(
        "QComboBox {"
        "  padding: 6px 10px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "}"
        "QComboBox:focus { border: 1px solid #2563eb; }"
        "QComboBox QLineEdit {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: none;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "  border: 1px solid #cbd5e1;"
        "  padding: 4px;"
        "}"
    );
    filterSelectCombo->hide();
    filterLayout->addWidget(filterSelectCombo);

    // Filter Buttons
    applyFilterBtn = new QPushButton("🔍 Lọc dữ liệu", filterFrame);
    applyFilterBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563eb;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    applyFilterBtn->hide();
    filterLayout->addWidget(applyFilterBtn);

    resetFilterBtn = new QPushButton("✖ Bỏ lọc", filterFrame);
    resetFilterBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "  font-size: 13px;"
        "  color: #475569;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; color: #0f172a; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );
    filterLayout->addWidget(resetFilterBtn);
    filterLayout->addStretch();

    mainLayout->addWidget(filterFrame);

    // Connect filter bar signals
    connect(filterColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int idx) {
        onFilterColumnChanged(idx);
    });

    connect(applyFilterBtn, &QPushButton::clicked, [this]() {
        applyFilter();
    });

    connect(resetFilterBtn, &QPushButton::clicked, [this]() {
        resetFilter();
    });

    connect(filterExactInput, &QLineEdit::returnPressed, [this]() {
        applyFilter();
    });

    connect(filterSelectCombo->lineEdit(), &QLineEdit::returnPressed, [this]() {
        applyFilter();
    });

    connect(filterSelectCombo, QOverload<int>::of(&QComboBox::activated), [this](int) {
        applyFilter();
    });
}

void QueryTableWidget::setupToolbarUI(QVBoxLayout* mainLayout)
{
    controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(8);

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("🔍 Tìm nhanh từ khóa trên bảng...");
    searchInput->setClearButtonEnabled(true);
    searchInput->setMinimumWidth(260);
    searchInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 6px 12px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus { border: 1px solid #2563eb; }"
    );

    countLabel = new QLabel("Tổng cộng: 0 bản ghi", this);
    countLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #475569;");

    // CRUD Action Buttons (hidden until configured)
    addBtn = new QPushButton("➕ Thêm mới", this);
    addBtn->setCursor(Qt::PointingHandCursor);
    addBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #2563eb;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #ffffff;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );
    addBtn->hide();

    editBtn = new QPushButton("✏️ Sửa", this);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #eff6ff;"
        "  border: 1px solid #93c5fd;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #1d4ed8;"
        "}"
        "QPushButton:hover { background-color: #dbeafe; }"
        "QPushButton:pressed { background-color: #bfdbfe; }"
    );
    editBtn->hide();

    deleteBtn = new QPushButton("🗑️ Xóa", this);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #fef2f2;"
        "  border: 1px solid #fca5a5;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #dc2626;"
        "}"
        "QPushButton:hover { background-color: #fee2e2; }"
        "QPushButton:pressed { background-color: #fecaca; }"
    );
    deleteBtn->hide();

    extraActionBtn = new QPushButton(this);
    extraActionBtn->setCursor(Qt::PointingHandCursor);
    extraActionBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #fffbeb;"
        "  border: 1px solid #fcd34d;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  color: #b45309;"
        "}"
        "QPushButton:hover { background-color: #fef3c7; }"
        "QPushButton:pressed { background-color: #fde68a; }"
    );
    extraActionBtn->hide();

    refreshBtn = new QPushButton("🔄 Làm mới", this);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  color: #334155;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );

    exportBtn = new QPushButton("📥 Xuất CSV", this);
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #10b981;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 12px;"
        "  font-size: 13px;"
        "  color: #ffffff;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background-color: #059669; }"
        "QPushButton:pressed { background-color: #047857; }"
    );

    controlsLayout->addWidget(searchInput);
    controlsLayout->addWidget(countLabel);
    controlsLayout->addStretch();
    controlsLayout->addWidget(addBtn);
    controlsLayout->addWidget(editBtn);
    controlsLayout->addWidget(extraActionBtn);
    controlsLayout->addWidget(deleteBtn);
    controlsLayout->addWidget(refreshBtn);
    controlsLayout->addWidget(exportBtn);

    mainLayout->addLayout(controlsLayout);

    connect(searchInput, &QLineEdit::textChanged, [this](const QString& q) {
        filterRows(q);
    });

    connect(addBtn, &QPushButton::clicked, [this]() {
        if (onAdd) onAdd();
    });

    connect(editBtn, &QPushButton::clicked, [this]() {
        if (!onEdit) return;
        const Record* rec = getSelectedRecord(true);
        if (rec) onEdit(*rec);
    });

    connect(deleteBtn, &QPushButton::clicked, [this]() {
        if (!onDelete) return;
        const Record* rec = getSelectedRecord(true);
        if (rec) onDelete(*rec);
    });

    connect(extraActionBtn, &QPushButton::clicked, [this]() {
        if (!onExtraAction) return;
        const Record* rec = getSelectedRecord(true);
        if (rec) onExtraAction(*rec);
    });

    connect(refreshBtn, &QPushButton::clicked, [this]() {
        if (onRefresh) onRefresh();
    });

    connect(exportBtn, &QPushButton::clicked, [this]() {
        exportToCsv();
    });
}

void QueryTableWidget::setupTableUI(QVBoxLayout* mainLayout)
{
    tableWidget = new QTableWidget(this);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->horizontalHeader()->setHighlightSections(false);
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->horizontalHeader()->setSortIndicatorShown(true);
    tableWidget->horizontalHeader()->setSectionsClickable(true);
    TableClipboardHelper::setupClipboardForTable(tableWidget);

    tableWidget->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 8px;"
        "  gridline-color: #f1f5f9;"
        "  font-size: 13px;"
        "  color: #1e293b;"
        "}"
        "QTableWidget::item {"
        "  padding: 6px 10px;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #dbeafe;"
        "  color: #1e40af;"
        "}"
        "QHeaderView::section {"
        "  background-color: #f1f5f9;"
        "  color: #334155;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 8px 10px;"
        "  border: none;"
        "  border-bottom: 2px solid #cbd5e1;"
        "}"
    );

    mainLayout->addWidget(tableWidget);

    // Rule 11: Sort directly on local Vector<Record> without re-running query
    connect(tableWidget->horizontalHeader(), &QHeaderView::sectionClicked, [this](int logicalIndex) {
        if (logicalIndex < 0 || logicalIndex >= static_cast<int>(keys.size())) return;
        if (cachedRecords.empty()) return;

        if (currentSortColumn == logicalIndex) {
            currentSortAscending = !currentSortAscending;
        } else {
            currentSortColumn = logicalIndex;
            currentSortAscending = true;
        }

        user_query::sort_query_results(cachedRecords, keys[logicalIndex], currentSortAscending);
        renderRows();
        tableWidget->horizontalHeader()->setSortIndicator(currentSortColumn, currentSortAscending ? Qt::AscendingOrder : Qt::DescendingOrder);
    });
}

void QueryTableWidget::setColumns(const QStringList& displayHeaders, const std::vector<std::string>& columnKeys)
{
    headers = displayHeaders;
    keys = columnKeys;

    tableWidget->setColumnCount(headers.size());
    tableWidget->setHorizontalHeaderLabels(headers);
}

void QueryTableWidget::setFilterColumns(const std::vector<FilterColumnDef>& defs)
{
    filterDefs = defs;

    filterColumnCombo->blockSignals(true);
    filterColumnCombo->clear();
    filterColumnCombo->addItem("-- Không lọc (Toàn bộ) --");

    for (const auto& def : filterDefs) {
        filterColumnCombo->addItem(def.displayName);
    }
    filterColumnCombo->blockSignals(false);
}

void QueryTableWidget::onFilterColumnChanged(int index)
{
    filterExactInput->hide();
    filterDateWidget->hide();
    filterSelectCombo->hide();
    applyFilterBtn->hide();

    if (index <= 0 || index - 1 >= static_cast<int>(filterDefs.size())) {
        // Reset back to full data
        if (onRefresh) onRefresh();
        return;
    }

    const auto& def = filterDefs[index - 1];
    applyFilterBtn->show();

    switch (def.type) {
    case FilterType::EXACT_TEXT:
        filterExactInput->clear();
        filterExactInput->setPlaceholderText(QString("Nhập chính xác %1...").arg(def.displayName));
        filterExactInput->show();
        filterExactInput->setFocus();
        break;

    case FilterType::DATE_RANGE:
        filterDateWidget->show();
        break;

    case FilterType::SELECT_OR_TYPE:
        filterSelectCombo->blockSignals(true);
        filterSelectCombo->clear();
        filterSelectCombo->addItems(def.options);
        filterSelectCombo->setEditText("");
        filterSelectCombo->blockSignals(false);
        filterSelectCombo->show();
        filterSelectCombo->setFocus();
        break;
    }
}

void QueryTableWidget::applyFilter()
{
    int index = filterColumnCombo->currentIndex();
    if (index <= 0 || index - 1 >= static_cast<int>(filterDefs.size())) {
        return;
    }

    const auto& def = filterDefs[index - 1];

    if (def.type == FilterType::EXACT_TEXT) {
        QString val = filterExactInput->text().trimmed();
        if (val.isEmpty()) {
            QMessageBox::warning(this, "Thông báo", "Vui lòng nhập giá trị cần lọc!");
            return;
        }
        if (onFilter) {
            onFilter(def.columnKey, val, "", false);
        }
    } else if (def.type == FilterType::DATE_RANGE) {
        QString d1 = fromDateEdit->date().toString("yyyy-MM-dd");
        QString d2 = toDateEdit->date().toString("yyyy-MM-dd");
        if (onFilter) {
            onFilter(def.columnKey, d1, d2, true);
        }
    } else if (def.type == FilterType::SELECT_OR_TYPE) {
        QString val = filterSelectCombo->currentText().trimmed();
        if (val.isEmpty()) {
            QMessageBox::warning(this, "Thông báo", "Vui lòng chọn hoặc nhập giá trị cần lọc!");
            return;
        }
        if (onFilter) {
            onFilter(def.columnKey, val, "", false);
        }
    }
}

void QueryTableWidget::resetFilter()
{
    filterColumnCombo->blockSignals(true);
    filterColumnCombo->setCurrentIndex(0);
    filterColumnCombo->blockSignals(false);

    filterExactInput->clear();
    filterExactInput->hide();
    filterDateWidget->hide();
    filterSelectCombo->clear();
    filterSelectCombo->hide();
    applyFilterBtn->hide();

    if (onRefresh) {
        onRefresh();
    }
}

void QueryTableWidget::setData(const Vector<Record>& records)
{
    int prevRow = tableWidget ? tableWidget->currentRow() : -1;
    QString savedKeyVal;
    if (tableWidget && prevRow >= 0 && prevRow < tableWidget->rowCount() && tableWidget->item(prevRow, 0)) {
        savedKeyVal = tableWidget->item(prevRow, 0)->text();
    }
    int savedVScroll = (tableWidget && tableWidget->verticalScrollBar()) ? tableWidget->verticalScrollBar()->value() : 0;

    cachedRecords = records;
    if (currentSortColumn >= 0 && currentSortColumn < static_cast<int>(keys.size())) {
        user_query::sort_query_results(cachedRecords, keys[currentSortColumn], currentSortAscending);
        tableWidget->horizontalHeader()->setSortIndicator(currentSortColumn, currentSortAscending ? Qt::AscendingOrder : Qt::DescendingOrder);
    } else {
        tableWidget->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    }

    renderRows();

    int targetRow = -1;
    if (!savedKeyVal.isEmpty()) {
        for (int r = 0; r < tableWidget->rowCount(); ++r) {
            QTableWidgetItem* it = tableWidget->item(r, 0);
            if (it && it->text() == savedKeyVal) {
                targetRow = r;
                break;
            }
        }
    }

    if (targetRow >= 0) {
        tableWidget->selectRow(targetRow);
        if (tableWidget->verticalScrollBar()) {
            tableWidget->verticalScrollBar()->setValue(savedVScroll);
        }
        tableWidget->scrollToItem(tableWidget->item(targetRow, 0));
    } else if (prevRow >= 0 && prevRow < tableWidget->rowCount()) {
        tableWidget->selectRow(prevRow);
        if (tableWidget->verticalScrollBar()) {
            tableWidget->verticalScrollBar()->setValue(savedVScroll);
        }
        tableWidget->scrollToItem(tableWidget->item(prevRow, 0));
    } else if (tableWidget->rowCount() > 0 && onDetailSelectionCallback) {
        tableWidget->selectRow(0);
    }

    if (onDetailSelectionCallback) {
        const Record* sel = getSelectedRecord(false);
        onDetailSelectionCallback(sel, detailBanner, detailTable);
    }
}

void QueryTableWidget::renderRows()
{
    tableWidget->setSortingEnabled(false);
    tableWidget->setUpdatesEnabled(false);
    tableWidget->blockSignals(true);

    int rowCount = static_cast<int>(cachedRecords.size());
    tableWidget->setRowCount(rowCount);

    for (size_t r = 0; r < cachedRecords.size(); ++r) {
        const auto& rec = cachedRecords[r];
        for (size_t c = 0; c < keys.size(); ++c) {
            QString text = "-";
            if (rec.has(keys[c])) {
                text = formatValue(rec.get(keys[c]));
            } else {
                for (const auto& item : rec.getValues()) {
                    size_t dot = item.first.find('.');
                    string colName = (dot == string::npos) ? item.first : item.first.substr(dot + 1);
                    size_t reqDot = keys[c].find('.');
                    string reqCol = (reqDot == string::npos) ? keys[c] : keys[c].substr(reqDot + 1);
                    if (colName == reqCol) {
                        text = formatValue(item.second);
                        break;
                    }
                }
            }
            auto* item = new QTableWidgetItem(text);
            tableWidget->setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }

    if (rowCount <= 100) {
        tableWidget->resizeColumnsToContents();
    } else {
        for (int c = 0; c < tableWidget->columnCount(); ++c) {
            int maxLen = static_cast<int>(headers.value(c).length());
            int sampleLimit = std::min(rowCount, 50);
            for (int r = 0; r < sampleLimit; ++r) {
                QTableWidgetItem* item = tableWidget->item(r, c);
                if (item) maxLen = std::max(maxLen, static_cast<int>(item->text().length()));
            }
            int estimatedWidth = std::max(120, maxLen * 10 + 24);
            tableWidget->setColumnWidth(c, std::min(estimatedWidth, 400));
        }
    }

    for (int c = 0; c < tableWidget->columnCount(); ++c) {
        if (tableWidget->columnWidth(c) < 120) {
            tableWidget->setColumnWidth(c, 120);
        }
    }

    tableWidget->blockSignals(false);
    tableWidget->setUpdatesEnabled(true);

    if (searchInput->text().trimmed().isEmpty()) {
        countLabel->setText(QString("Tổng cộng: %1 bản ghi").arg(rowCount));
    } else {
        filterRows(searchInput->text());
    }
}

void QueryTableWidget::setRefreshCallback(std::function<void()> callback)
{
    onRefresh = callback;
}

void QueryTableWidget::setFilterCallback(FilterCallback callback)
{
    onFilter = callback;
}

void QueryTableWidget::addExtraControl(QWidget* widget)
{
    controlsLayout->insertWidget(2, widget);
}

void QueryTableWidget::updateRecordInPlace(int row, const std::string& fieldKey, const Value& newValue)
{
    if (row < 0 || row >= static_cast<int>(cachedRecords.size())) return;

    cachedRecords[row].set(fieldKey, newValue);

    size_t dot = fieldKey.find('.');
    if (dot != std::string::npos) {
        cachedRecords[row].set(fieldKey.substr(dot + 1), newValue);
    }

    for (size_t c = 0; c < keys.size(); ++c) {
        bool match = (keys[c] == fieldKey);
        if (!match) {
            size_t reqDot = keys[c].find('.');
            std::string reqCol = (reqDot == std::string::npos) ? keys[c] : keys[c].substr(reqDot + 1);
            std::string curCol = (dot == std::string::npos) ? fieldKey : fieldKey.substr(dot + 1);
            match = (reqCol == curCol);
        }
        if (match) {
            QTableWidgetItem* it = tableWidget->item(row, static_cast<int>(c));
            if (it) {
                it->setText(formatValue(newValue));
            }
            break;
        }
    }
}

void QueryTableWidget::setCrudCallbacks(
    std::function<void()> onAddCallback,
    RecordActionCallback onEditCallback,
    RecordActionCallback onDeleteCallback,
    const QString& addText,
    const QString& editText,
    const QString& deleteText)
{
    onAdd = onAddCallback;
    onEdit = onEditCallback;
    onDelete = onDeleteCallback;

    if (!addText.isEmpty()) addBtn->setText(addText);
    if (!editText.isEmpty()) editBtn->setText(editText);
    if (!deleteText.isEmpty()) deleteBtn->setText(deleteText);

    addBtn->setVisible(static_cast<bool>(onAdd));
    editBtn->setVisible(static_cast<bool>(onEdit));
    deleteBtn->setVisible(static_cast<bool>(onDelete));
}

void QueryTableWidget::setCrudPermissions(bool canAdd, bool canEdit, bool canDelete)
{
    addBtn->setVisible(canAdd && static_cast<bool>(onAdd));
    editBtn->setVisible(canEdit && static_cast<bool>(onEdit));
    deleteBtn->setVisible(canDelete && static_cast<bool>(onDelete));
}

void QueryTableWidget::setExtraAction(const QString& label, RecordActionCallback callback)
{
    onExtraAction = callback;
    extraActionBtn->setText(label);
    extraActionBtn->setVisible(static_cast<bool>(onExtraAction));
}

void QueryTableWidget::clearExtraAction()
{
    onExtraAction = nullptr;
    extraActionBtn->hide();
}

const Record* QueryTableWidget::getSelectedRecord(bool warnIfNone)
{
    int row = tableWidget->currentRow();
    if ((row < 0 || tableWidget->isRowHidden(row)) && tableWidget->selectionModel()) {
        const auto selectedIdxs = tableWidget->selectionModel()->selectedIndexes();
        for (const auto& idx : selectedIdxs) {
            if (!tableWidget->isRowHidden(idx.row())) {
                row = idx.row();
                break;
            }
        }
    }

    if (row < 0 || row >= static_cast<int>(cachedRecords.size()) || tableWidget->isRowHidden(row)) {
        if (warnIfNone) {
            QMessageBox::warning(this, "Chưa chọn bản ghi", "Vui lòng chọn một dòng dữ liệu trên bảng trước khi thực hiện thao tác!");
        }
        return nullptr;
    }

    return &cachedRecords[row];
}

void QueryTableWidget::enableBottomDetailPanel(
    const QString& defaultBannerText,
    const QStringList& detailHeaders,
    const std::vector<std::string>& detailKeys,
    DetailSelectionCallback callback)
{
    Q_UNUSED(detailKeys);
    defaultBannerMsg = defaultBannerText;
    onDetailSelectionCallback = callback;

    if (!mainSplitter) {
        auto* mainLay = qobject_cast<QVBoxLayout*>(layout());
        if (mainLay) {
            mainLay->removeWidget(tableWidget);
            mainSplitter = new QSplitter(Qt::Vertical, this);
            mainSplitter->setStyleSheet(
                "QSplitter::handle {"
                "  background-color: #cbd5e1;"
                "  height: 4px;"
                "}"
                "QSplitter::handle:hover {"
                "  background-color: #2563eb;"
                "}"
            );
            mainSplitter->addWidget(tableWidget);

            bottomDetailContainer = new QWidget(mainSplitter);
            auto* btmLayout = new QVBoxLayout(bottomDetailContainer);
            btmLayout->setContentsMargins(0, 6, 0, 0);
            btmLayout->setSpacing(6);

            detailBanner = new QLabel(defaultBannerMsg, bottomDetailContainer);
            detailBanner->setStyleSheet(
                "QLabel {"
                "  background-color: #f8fafc;"
                "  border: 1px solid #cbd5e1;"
                "  border-radius: 6px;"
                "  padding: 8px 12px;"
                "  font-weight: 600;"
                "  font-size: 13px;"
                "  color: #1e293b;"
                "}"
            );
            btmLayout->addWidget(detailBanner);

            detailTable = new QTableWidget(bottomDetailContainer);
            detailTable->setAlternatingRowColors(true);
            detailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
            detailTable->verticalHeader()->setVisible(false);
            detailTable->horizontalHeader()->setHighlightSections(false);
            detailTable->horizontalHeader()->setStretchLastSection(true);
            TableClipboardHelper::setupClipboardForTable(detailTable);

            detailTable->setStyleSheet(
                "QTableWidget {"
                "  background-color: #ffffff;"
                "  alternate-background-color: #f8fafc;"
                "  border: 1px solid #e2e8f0;"
                "  border-radius: 8px;"
                "  gridline-color: #f1f5f9;"
                "  font-size: 13px;"
                "  color: #1e293b;"
                "}"
                "QTableWidget::item { padding: 4px 8px; }"
                "QHeaderView::section {"
                "  background-color: #f1f5f9;"
                "  color: #334155;"
                "  font-weight: 600;"
                "  font-size: 12px;"
                "  padding: 6px 8px;"
                "  border: none;"
                "  border-bottom: 2px solid #cbd5e1;"
                "}"
            );
            btmLayout->addWidget(detailTable);

            mainSplitter->addWidget(bottomDetailContainer);
            mainSplitter->setStretchFactor(0, 3);
            mainSplitter->setStretchFactor(1, 2);

            mainLay->addWidget(mainSplitter, 1);
        }
    }

    if (detailTable) {
        detailTable->setColumnCount(detailHeaders.size());
        detailTable->setHorizontalHeaderLabels(detailHeaders);
    }

    connect(tableWidget, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (!onDetailSelectionCallback) return;
        const Record* sel = getSelectedRecord(false);
        onDetailSelectionCallback(sel, detailBanner, detailTable);
    });

    if (tableWidget->rowCount() > 0) {
        tableWidget->selectRow(0);
    }
}

void QueryTableWidget::filterRows(const QString& query)
{
    QString q = query.trimmed().toLower();
    int total = tableWidget->rowCount();
    int visibleCount = 0;

    tableWidget->setUpdatesEnabled(false);

    for (int r = 0; r < total; ++r) {
        bool match = q.isEmpty();
        if (!match) {
            for (int c = 0; c < tableWidget->columnCount(); ++c) {
                QTableWidgetItem* item = tableWidget->item(r, c);
                if (item && item->text().toLower().contains(q)) {
                    match = true;
                    break;
                }
            }
        }
        if (tableWidget->isRowHidden(r) == match) {
            tableWidget->setRowHidden(r, !match);
        }
        if (match) visibleCount++;
    }

    tableWidget->setUpdatesEnabled(true);

    if (q.isEmpty()) {
        countLabel->setText(QString("Tổng cộng: %1 bản ghi").arg(total));
    } else {
        countLabel->setText(QString("Tìm thấy: %1 / %2 bản ghi").arg(visibleCount).arg(total));
    }
}

void QueryTableWidget::exportToCsv()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Xuất dữ liệu CSV", QString(), "Tệp CSV (*.csv);;Tất cả tệp (*.*)");

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Lỗi", "Không thể mở tệp để ghi dữ liệu!");
        return;
    }

    QTextStream out(&file);

    for (int c = 0; c < tableWidget->columnCount(); ++c) {
        out << "\"" << headers[c] << "\"";
        if (c + 1 < tableWidget->columnCount()) out << ",";
    }
    out << "\n";

    int exportedRows = 0;
    for (int r = 0; r < tableWidget->rowCount(); ++r) {
        if (tableWidget->isRowHidden(r)) continue;
        for (int c = 0; c < tableWidget->columnCount(); ++c) {
            QTableWidgetItem* item = tableWidget->item(r, c);
            QString text = item ? item->text() : "";
            text.replace("\"", "\"\"");
            out << "\"" << text << "\"";
            if (c + 1 < tableWidget->columnCount()) out << ",";
        }
        out << "\n";
        exportedRows++;
    }

    file.close();
    QMessageBox::information(this, "Thành công", QString("Đã xuất %1 bản ghi ra tệp:\n%2").arg(exportedRows).arg(fileName));
}

QString QueryTableWidget::formatValue(const Value& val)
{
    if (holds_alternative<int>(val)) {
        return QString::number(get<int>(val));
    }
    if (holds_alternative<double>(val)) {
        return QString::number(get<double>(val), 'f', 2);
    }
    if (holds_alternative<string>(val)) {
        return QString::fromStdString(get<string>(val));
    }
    if (holds_alternative<bool>(val)) {
        return get<bool>(val) ? "True" : "False";
    }
    return "-";
}
