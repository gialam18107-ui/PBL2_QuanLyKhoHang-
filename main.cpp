#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <QEvent>
#include <QComboBox>
#include <QAbstractSpinBox>
#include <iostream>

#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "ui/MainWindow.h"

class NoWheelEventFilter : public QObject {
public:
    explicit NoWheelEventFilter(QObject* parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Wheel) {
            if (qobject_cast<QComboBox*>(watched) || qobject_cast<QAbstractSpinBox*>(watched)) {
                return true; // Chặn cuộn chuột đổi lựa chọn; người dùng phải bấm chọn cụ thể
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    NoWheelEventFilter wheelFilter(&app);
    app.installEventFilter(&wheelFilter);
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(241, 245, 249));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(37, 99, 235));
    palette.setColor(QPalette::Highlight, QColor(37, 99, 235));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(148, 163, 184));
    app.setPalette(palette);

    QFont font("Segoe UI", 10);
    app.setFont(font);

    // Ensure readable dark text on light background for all QComboBox popups & context QMenus
    app.setStyleSheet(
        "QComboBox { combobox-popup: 0; color: #0f172a; background-color: #ffffff; }"
        "QComboBox:disabled { color: #64748b; background-color: #f1f5f9; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  selection-background-color: #2563eb;"
        "  selection-color: #ffffff;"
        "  outline: 0;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "  color: #0f172a;"
        "  background-color: #ffffff;"
        "  padding: 5px 10px;"
        "  min-height: 22px;"
        "}"
        "QComboBox QAbstractItemView::item:hover {"
        "  background-color: #eff6ff;"
        "  color: #1e40af;"
        "}"
        "QComboBox QAbstractItemView::item:selected {"
        "  background-color: #2563eb;"
        "  color: #ffffff;"
        "}"
        "QMenu {"
        "  background-color: #ffffff;"
        "  color: #0f172a;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 6px;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  color: #0f172a;"
        "  background-color: transparent;"
        "  padding: 6px 20px;"
        "  border-radius: 4px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #eff6ff;"
        "  color: #1d4ed8;"
        "  font-weight: 600;"
        "}"
        "QMenu::item:disabled { color: #94a3b8; }"
    );

    // Initialize Database and load all 32 tables from data/
    Database db("warehouse_db");
    std::cout << "[Init] Loading database from data/..." << std::endl;
    bool loaded = DataIO::DatabaseIO::readAll(db, "data");
    DataIO::DatabaseIO::readAllCategorySpecs(db, "data");
    std::cout << "[Init] Database loaded: " << (loaded ? "SUCCESS" : "PARTIAL") 
              << " (" << db.getTableCount() << " tables)" << std::endl;

    MainWindow mainWindow(&db);
    mainWindow.showMaximized();

    return app.exec();
}
