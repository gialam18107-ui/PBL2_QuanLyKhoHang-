#ifndef TABLE_CLIPBOARD_HELPER_H
#define TABLE_CLIPBOARD_HELPER_H

#include <QTableWidget>
#include <QGuiApplication>
#include <QClipboard>
#include <QShortcut>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <algorithm>

namespace TableClipboardHelper {

inline void copySelection(QTableWidget* table, QLabel* statusLabel = nullptr)
{
    if (!table) return;
    auto selectedIndexes = table->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    // Sắp xếp các ô được chọn theo hàng trước, sau đó theo cột
    std::sort(selectedIndexes.begin(), selectedIndexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        if (a.row() != b.row()) return a.row() < b.row();
        return a.column() < b.column();
    });

    // Nếu chỉ chọn đúng 1 ô: sao chép giá trị nguyên bản, không kèm ký tự xuống dòng hay tab
    if (selectedIndexes.size() == 1) {
        auto* item = table->item(selectedIndexes[0].row(), selectedIndexes[0].column());
        QString txt = item ? item->text() : "";
        QGuiApplication::clipboard()->setText(txt);
        if (statusLabel) {
            QString preview = txt.length() > 40 ? txt.left(37) + "..." : txt;
            statusLabel->setText(QString("📋 Đã sao chép vào clipboard: \"%1\"").arg(preview));
        }
        return;
    }

    // Nhiều ô: ghép hàng ngang bằng tab (\t), hàng dọc bằng xuống dòng (\n)
    QString result;
    int currentRow = selectedIndexes[0].row();
    QStringList rowVals;

    for (const auto& idx : selectedIndexes) {
        if (idx.row() != currentRow) {
            result += rowVals.join("\t") + "\n";
            rowVals.clear();
            currentRow = idx.row();
        }
        auto* item = table->item(idx.row(), idx.column());
        rowVals << (item ? item->text() : "");
    }
    if (!rowVals.isEmpty()) {
        result += rowVals.join("\t");
    }

    QGuiApplication::clipboard()->setText(result);
    if (statusLabel) {
        statusLabel->setText(QString("📋 Đã sao chép %1 ô vào clipboard!").arg(selectedIndexes.size()));
    }
}

inline void copyRow(QTableWidget* table, int row, QLabel* statusLabel = nullptr)
{
    if (!table || row < 0 || row >= table->rowCount()) return;
    QStringList vals;
    for (int col = 0; col < table->columnCount(); ++col) {
        auto* item = table->item(row, col);
        vals << (item ? item->text() : "");
    }
    QString text = vals.join("\t");
    QGuiApplication::clipboard()->setText(text);
    if (statusLabel) {
        statusLabel->setText(QString("📋 Đã sao chép dòng %1 (%2 cột) vào clipboard!").arg(row + 1).arg(table->columnCount()));
    }
}

inline void copyTable(QTableWidget* table, bool includeHeaders, QLabel* statusLabel = nullptr)
{
    if (!table) return;
    QString result;
    if (includeHeaders) {
        QStringList headers;
        for (int c = 0; c < table->columnCount(); ++c) {
            auto* hItem = table->horizontalHeaderItem(c);
            headers << (hItem ? hItem->text() : QString("Cột %1").arg(c + 1));
        }
        result += headers.join("\t") + "\n";
    }

    for (int r = 0; r < table->rowCount(); ++r) {
        QStringList rowVals;
        for (int c = 0; c < table->columnCount(); ++c) {
            auto* item = table->item(r, c);
            rowVals << (item ? item->text() : "");
        }
        result += rowVals.join("\t") + "\n";
    }
    if (result.endsWith("\n")) result.chop(1);

    QGuiApplication::clipboard()->setText(result);
    if (statusLabel) {
        statusLabel->setText(QString("📋 Đã sao chép toàn bộ bảng (%1 dòng) vào clipboard!").arg(table->rowCount()));
    }
}

inline void setupClipboardForTable(QTableWidget* table, QLabel* statusLabel = nullptr)
{
    if (!table) return;

    // Cho phép chọn từng ô độc lập hoặc quét khối ô nhiều dòng nhiều cột ("bôi đen" để copy)
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Phím tắt Ctrl+C
    auto* copyShortcut = new QShortcut(QKeySequence::Copy, table);
    QObject::connect(copyShortcut, &QShortcut::activated, [table, statusLabel]() {
        copySelection(table, statusLabel);
    });

    // Phím tắt phụ Ctrl+Insert
    auto* copyInsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Insert), table);
    QObject::connect(copyInsShortcut, &QShortcut::activated, [table, statusLabel]() {
        copySelection(table, statusLabel);
    });

    // Menu chuột phải (Context Menu)
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(table, &QTableWidget::customContextMenuRequested, [table, statusLabel](const QPoint& pos) {
        auto* clickedItem = table->itemAt(pos);
        if (!clickedItem && table->selectionModel()->selectedIndexes().isEmpty()) {
            return;
        }

        // Nếu chuột phải vào ô chưa được chọn và chưa có vùng chọn nào khác, chọn ô đó
        if (clickedItem && !clickedItem->isSelected()) {
            table->setCurrentItem(clickedItem);
        }

        QMenu menu(table);
        menu.setStyleSheet(
            "QMenu {"
            "  background-color: #ffffff;"
            "  border: 1px solid #cbd5e1;"
            "  border-radius: 6px;"
            "  padding: 4px;"
            "  font-size: 13px;"
            "  color: #0f172a;"
            "}"
            "QMenu::item {"
            "  padding: 6px 20px 6px 12px;"
            "  border-radius: 4px;"
            "}"
            "QMenu::item:selected {"
            "  background-color: #eff6ff;"
            "  color: #2563eb;"
            "}"
        );

        auto selectedIdxs = table->selectionModel()->selectedIndexes();
        QString copyLabel = "📋 Sao chép (Ctrl+C)";
        if (selectedIdxs.size() == 1) {
            auto* item = table->item(selectedIdxs[0].row(), selectedIdxs[0].column());
            if (item) {
                QString val = item->text();
                if (val.length() > 25) val = val.left(22) + "...";
                copyLabel = QString("📋 Sao chép ô: \"%1\" (Ctrl+C)").arg(val);
            }
        } else if (selectedIdxs.size() > 1) {
            copyLabel = QString("📋 Sao chép %1 ô đã chọn (Ctrl+C)").arg(selectedIdxs.size());
        }

        auto* actCopy = menu.addAction(copyLabel);
        QObject::connect(actCopy, &QAction::triggered, [table, statusLabel]() {
            copySelection(table, statusLabel);
        });

        int row = clickedItem ? clickedItem->row() : (selectedIdxs.isEmpty() ? -1 : selectedIdxs[0].row());
        if (row >= 0) {
            auto* actCopyRow = menu.addAction(QString("📋 Sao chép toàn bộ dòng %1").arg(row + 1));
            QObject::connect(actCopyRow, &QAction::triggered, [table, row, statusLabel]() {
                copyRow(table, row, statusLabel);
            });
        }

        menu.addSeparator();

        auto* actCopyAllWithHeader = menu.addAction("📑 Sao chép toàn bộ bảng (kèm tiêu đề)");
        QObject::connect(actCopyAllWithHeader, &QAction::triggered, [table, statusLabel]() {
            copyTable(table, true, statusLabel);
        });

        auto* actCopyAllData = menu.addAction("📄 Sao chép toàn bộ bảng (chỉ dữ liệu)");
        QObject::connect(actCopyAllData, &QAction::triggered, [table, statusLabel]() {
            copyTable(table, false, statusLabel);
        });

        menu.exec(table->viewport()->mapToGlobal(pos));
    });

    // Double click vào ô: copy ngay giá trị của ô đó vào clipboard
    QObject::connect(table, &QTableWidget::cellDoubleClicked, [table, statusLabel](int row, int col) {
        auto* item = table->item(row, col);
        if (item) {
            QGuiApplication::clipboard()->setText(item->text());
            if (statusLabel) {
                QString preview = item->text().length() > 40 ? item->text().left(37) + "..." : item->text();
                statusLabel->setText(QString("📋 Đã sao chép vào clipboard: \"%1\"").arg(preview));
            }
        }
    });
}

} // namespace TableClipboardHelper

#endif // TABLE_CLIPBOARD_HELPER_H
