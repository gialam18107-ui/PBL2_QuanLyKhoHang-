#include "QueryTableWidget.h"
#include "schema/UserQuery.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include "ui/TableClipboardHelper.h"

QueryTableWidget::QueryTableWidget(const QString& title, const QString& description, QWidget* parent)
    : QWidget(parent), titleText(title), descText(description)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 18, 18, 18);
    mainLayout->setSpacing(12);

    // 1. Header info
    titleLabel = new QLabel(titleText, this);
    titleLabel->setObjectName("viewTitle");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #0f172a;");

    descLabel = new QLabel(descText, this);
    descLabel->setObjectName("viewDescription");
    descLabel->setStyleSheet("font-size: 13px; color: #64748b; margin-bottom: 2px;");

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(descLabel);

    // 2. Setup dynamic filter bar (Rule 9.1)
    setupFilterUI(mainLayout);

    // 3. Setup secondary toolbar (Search, count, refresh, export)
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

    auto* filterIconLabel = new QLabel("🎯 Tìm kiếm theo cấu trúc dữ liệu:", filterFrame);
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
    applyFilterBtn = new QPushButton("🔍 Tìm kiếm theo cấu trúc dữ liệu", filterFrame);
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
    controlsLayout->setSpacing(10);

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("⚡ Tìm nhanh từ khóa trên bảng (Bộ nhớ đệm Vector)...");
    searchInput->setClearButtonEnabled(true);
    searchInput->setMinimumWidth(280);
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

    refreshBtn = new QPushButton("🔄 Làm mới", this);
    refreshBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #f1f5f9;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "  font-size: 13px;"
        "  color: #334155;"
        "}"
        "QPushButton:hover { background-color: #e2e8f0; }"
        "QPushButton:pressed { background-color: #cbd5e1; }"
    );

    exportBtn = new QPushButton("📥 Xuất CSV", this);
    exportBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #10b981;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
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
    controlsLayout->addWidget(refreshBtn);
    controlsLayout->addWidget(exportBtn);

    mainLayout->addLayout(controlsLayout);

    connect(searchInput, &QLineEdit::textChanged, [this](const QString& q) {
        filterRows(q);
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
        QString typeTag;
        switch (def.type) {
        case FilterType::EXACT_TEXT:
            typeTag = " [Bảng băm / Hash]";
            break;
        case FilterType::DATE_RANGE:
            typeTag = " [Cây AVL / Range]";
            break;
        case FilterType::SELECT_OR_TYPE:
            typeTag = " [Cây băm / Hash Tree]";
            break;
        }
        filterColumnCombo->addItem(def.displayName + typeTag);
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
    cachedRecords = records;
    currentSortColumn = -1;
    currentSortAscending = true;
    tableWidget->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);

    renderRows();
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
        countLabel->setText(QString("Tổng cộng: %1 bản ghi | Đã nạp vào Vector<Record>").arg(rowCount));
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
        countLabel->setText(QString("Tổng cộng: %1 bản ghi | Đã nạp vào Vector<Record>").arg(total));
    } else {
        countLabel->setText(QString("Tìm thấy: %1 / %2 bản ghi (Bộ nhớ đệm Vector)").arg(visibleCount).arg(total));
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
