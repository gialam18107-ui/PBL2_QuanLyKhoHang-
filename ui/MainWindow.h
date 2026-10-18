#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

#include "database/Database.h"
#include "QueryTableWidget.h"

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(Database* database, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUI();
    void setupSidebar();
    void setupPages();
    void refreshQuery(int index);
    void reloadDatabase();
    void loadAuxSpecData(const QString& tableName);

    Database* db;

    QListWidget* sidebarList;
    QStackedWidget* stackedWidget;
    QLabel* statusLabel;
    QPushButton* reloadDbBtn;

    // 11 Core Query Table Widgets (PRODUCT_QUERY removed)
    QueryTableWidget* productQuantityWidget;
    QueryTableWidget* unitInventoryWidget;
    QueryTableWidget* statusHistoryWidget;
    QueryTableWidget* inboundTxnWidget;
    QueryTableWidget* inboundDetailWidget;
    QueryTableWidget* deliveryWidget;
    QueryTableWidget* deliveryDetailWidget;
    QueryTableWidget* outboundTxnWidget;
    QueryTableWidget* outboundDetailWidget;
    QueryTableWidget* shipmentWidget;
    QueryTableWidget* shipmentDetailWidget;

    // 8 Category Hardware Classification Widgets
    QueryTableWidget* cpuWidget;
    QueryTableWidget* gpuWidget;
    QueryTableWidget* mainboardWidget;
    QueryTableWidget* ramWidget;
    QueryTableWidget* ssdWidget;
    QueryTableWidget* psuWidget;
    QueryTableWidget* coolingWidget;
    QueryTableWidget* caseWidget;

    // Auxiliary Specs & Metadata Widget
    QueryTableWidget* auxSpecWidget;
    QComboBox* auxSpecTableCombo;
};

#endif // MAIN_WINDOW_H
