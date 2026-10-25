#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

#include "database/Database.h"
#include "QueryTableWidget.h"

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(Database* database, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUI();
    void setupMenuBar();
    void setupSidebar();
    void setupPages();
    void refreshQuery(int index);
    void reloadDatabase();
    void loadAuxSpecData(const QString& tableName);
    QStringList loadSpecOptions(const QString& specTableName);
    QStringList loadColumnOptions(const QString& tableName, const QString& colName);
    void updateCategoryFilterOptions();

    void switchToDomain(int domainIndex); // 0: Warehouse, 1: Category, 2: Aux Specs, 3: Customers
    void selectWarehouseQuery(int row);
    void selectCategoryQuery(int row);
    void selectAuxSpecQuery(const QString& tableName);
    void selectCustomerQuery(int row);

    Database* db;

    // Top Menus (Rule 9.2, 9.3 & 9.4)
    QMenu* warehouseMenu;
    QMenu* categoryMenu;   // "Danh mục sản phẩm"
    QMenu* customerMenu;   // "Khách hàng" (Rule 9.4)
    QMenu* auxSpecMenu;    // "Danh mục phụ"

    // Sidebar navigation tabs & separated lists
    QPushButton* tabWarehouseBtn;
    QPushButton* tabCategoryBtn;
    QPushButton* tabCustomerBtn;
    QPushButton* tabAuxSpecBtn;
    QStackedWidget* sidebarStack;
    QListWidget* warehouseList;
    QListWidget* categoryList;
    QListWidget* customerList;
    QListWidget* auxSpecList;

    QStackedWidget* stackedWidget;
    QLabel* statusLabel;
    QPushButton* reloadDbBtn;

    // 11 Warehouse Query Table Widgets
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

    // 8 Category Hardware Classification Widgets (Rule 9.2)
    QueryTableWidget* cpuWidget;
    QueryTableWidget* gpuWidget;
    QueryTableWidget* mainboardWidget;
    QueryTableWidget* ramWidget;
    QueryTableWidget* ssdWidget;
    QueryTableWidget* psuWidget;
    QueryTableWidget* coolingWidget;
    QueryTableWidget* caseWidget;

    // Auxiliary Specs & Metadata Widget (Rule 9.3)
    QueryTableWidget* auxSpecWidget;
    QComboBox* auxSpecTableCombo;

    // 2 Customer Query Table Widgets (Rule 9.4)
    QueryTableWidget* individualCustomerWidget;
    QueryTableWidget* businessCustomerWidget;
};

#endif // MAIN_WINDOW_H