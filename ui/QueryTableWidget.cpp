#include "QueryTableWidget.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>

QueryTableWidget::QueryTableWidget(const QString& title, const QString& description, QWidget* parent)
    : QWidget(parent), titleText(title), descText(description)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Header info
    titleLabel = new QLabel(titleText, this);
    titleLabel->setObjectName("viewTitle");
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #1e293b;");

    descLabel = new QLabel(descText, this);
    descLabel->setObjectName("viewDescription");
    descLabel->setStyleSheet("font-size: 13px; color: #64748b; margin-bottom: 4px;");

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(descLabel);

    // Toolbar / Controls bar
    controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(10);

    searchInput = new QLineEdit(this);
    searchInput->setPlaceholderText("🔍 Tìm kiếm nhanh trong bảng...");
    searchInput->setClearButtonEnabled(true);
    searchInput->setMinimumWidth(260);
    searchInput->setStyleSheet(
        "QLineEdit {"
        "  padding: 6px 12px;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  background-color: #ffffff;"
        "  font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #2563eb;"
        "}"
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
        "  background-color: #2563eb;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 6px 14px;"
        "  font-size: 13px;"
        "  color: #ffffff;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover { background-color: #1d4ed8; }"
        "QPushButton:pressed { background-color: #1e40af; }"
    );

    controlsLayout->addWidget(searchInput);
    controlsLayout->addWidget(countLabel);
    controlsLayout->addStretch();
    controlsLayout->addWidget(refreshBtn);
    controlsLayout->addWidget(exportBtn);

    mainLayout->addLayout(controlsLayout);

    // Data Table
    tableWidget = new QTableWidget(this);
    tableWidget->setAlternatingRowColors(true);
    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->verticalHeader()->setVisible(false);
    tableWidget->horizontalHeader()->setHighlightSections(false);
    tableWidget->horizontalHeader()->setStretchLastSection(true);
    tableWidget->setStyleSheet(
        "QTableWidget {"
        "  background-color: #ffffff;"
        "  alternate-background-color: #f8fafc;"
        "  border: 1px solid #e2e8f0;"
        "  border-radius: 6px;"
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
        "  color: #475569;"
        "  font-weight: 600;"
        "  font-size: 13px;"
        "  padding: 8px 10px;"
        "  border: none;"
        "  border-bottom: 2px solid #cbd5e1;"
        "}"
    );

    mainLayout->addWidget(tableWidget);

    // Connect signals
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

void QueryTableWidget::setColumns(const QStringList& displayHeaders, const std::vector<std::string>& columnKeys)
{
    headers = displayHeaders;
    keys = columnKeys;

    tableWidget->setColumnCount(headers.size());
    tableWidget->setHorizontalHeaderLabels(headers);
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

void QueryTableWidget::setData(const Vector<Record>& records)
{
    tableWidget->setSortingEnabled(false);
    tableWidget->setUpdatesEnabled(false);
    tableWidget->blockSignals(true);

    int rowCount = static_cast<int>(records.size());
    tableWidget->setRowCount(rowCount);

    for (size_t r = 0; r < records.size(); ++r) {
        const auto& rec = records[r];
        for (size_t c = 0; c < keys.size(); ++c) {
            QString text = "-";
            if (rec.has(keys[c])) {
                text = formatValue(rec.get(keys[c]));
            } else {
                // Fallback unqualified matching
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

    // Fast column width calculation: only sample first 50 rows if dataset is large
    if (rowCount <= 100) {
        tableWidget->resizeColumnsToContents();
    } else {
        // Fast default column widths
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
    tableWidget->setSortingEnabled(true);

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

void QueryTableWidget::addExtraControl(QWidget* widget)
{
    // Insert before searchInput or after countLabel
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

    // Write header
    for (int c = 0; c < tableWidget->columnCount(); ++c) {
        out << "\"" << headers[c] << "\"";
        if (c + 1 < tableWidget->columnCount()) out << ",";
    }
    out << "\n";

    // Write rows
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
