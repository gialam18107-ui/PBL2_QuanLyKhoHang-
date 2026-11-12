#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QCloseEvent>

#include "database/Database.h"
#include "data_io/DatabaseIO.h"
#include "QueryTableWidget.h"
#include "ui/InventoryCheckDialog.h"
#include "ui/AdjustmentMovementDialog.h" 

class TransactionHubWidget;

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(Database* database, QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupSidebar();
    void setupPages();
    void setupCrudWiring();
    void refreshQuery(int index);
    void reloadDatabase();
    bool saveDatabase(bool showSuccessPopup = true);
    void notifyDataChanged(const QString& summary = "");
    void markDirty(const QString& summary = "");
    void loadAuxSpecData(const QString& tableName);
    QStringList loadSpecOptions(const QString& specTableName);
    QStringList loadColumnOptions(const QString& tableName, const QString& colName);
    void updateCategoryFilterOptions();

    int resolveProductId(const Record& rec, const std::string& catTable = "") const;
    int resolveCustomerId(const Record& rec) const;
    static bool isEditableSpecSubTable(const QString& tableName);

    void onAddAuxSpecOrBrand();
    void onEditAuxSpecOrBrand(const Record& rec);
    void onDeleteAuxSpecOrBrand(const Record& rec);

    void switchToDomain(int domainIndex); // 0: Warehouse, 1: Category, 2: Customers, 3: Aux Specs, 4: Report
    void selectWarehouseQuery(int row);
    void selectCategoryQuery(int row);
    void selectAuxSpecQuery(const QString& tableName);
    void selectCustomerQuery(int row);
    void selectReportQuery();
    void setupReportPage();
    void renderReportTable();
    void setupTopNavBar();
    void setActiveNavButton(QPushButton* btn);

    Database* db;
    DataIO::DatabaseIO dbIO;
    bool hasUnsavedChanges;

    // Top Menus (Rule 9.2, 9.3 & 9.4, Section 9)
    QMenu* warehouseMenu;
    QMenu* categoryMenu;   // "Danh mục sản phẩm"
    QMenu* customerMenu;   // "Khách hàng" (Rule 9.4)
    QMenu* auxSpecMenu;    // "Danh mục phụ"
    QMenu* reportMenu;     // "Báo cáo" (Section 9)

    // Top Horizontal Navigation Bar
    QWidget* topNavBar;
    QPushButton* navWarehouseBtn;
    QPushButton* navProductBtn;
    QPushButton* navInboundBtn;
    QPushButton* navOutboundBtn;
    QPushButton* navCheckBtn;
    QPushButton* navAdjustmentBtn;
    QPushButton* navReportBtn;
    QPushButton* navCustomerBtn;
    QPushButton* navMoreBtn;

    // Legacy sidebar references kept for backwards compatibility
    QPushButton* tabWarehouseBtn;
    QPushButton* tabCategoryBtn;
    QPushButton* tabCustomerBtn;
    QPushButton* tabAuxSpecBtn;
    QPushButton* tabReportBtn;
    QStackedWidget* sidebarStack;
    QListWidget* warehouseList;
    QListWidget* categoryList;
    QListWidget* customerList;
    QListWidget* auxSpecList;
    QListWidget* reportList;

    QStackedWidget* stackedWidget;
    QLabel* statusLabel;
    QPushButton* saveDbBtn;
    QPushButton* reloadDbBtn;

    // 11 Warehouse Query Table Widgets
    QueryTableWidget* productQuantityWidget;
    QueryTableWidget* unitInventoryWidget;
    QueryTableWidget* statusHistoryWidget;
    TransactionHubWidget* inboundTxnWidget;
    QueryTableWidget* inboundDetailWidget;
    QueryTableWidget* deliveryWidget;
    QueryTableWidget* deliveryDetailWidget;
    TransactionHubWidget* outboundTxnWidget;
    QueryTableWidget* outboundDetailWidget;
    QueryTableWidget* shipmentWidget;
    QueryTableWidget* shipmentDetailWidget;

    // 6 Inventory Check & Adjustment Movement Query Table Widgets
    QueryTableWidget* inventoryCheckWidget;
    QueryTableWidget* inventoryCheckQuantityWidget;
    QueryTableWidget* inventoryCheckUnitWidget;
    QueryTableWidget* adjustmentMovementWidget;
    QueryTableWidget* adjustMovementDetailWidget;
    QueryTableWidget* isSoldChangingWidget;

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

    // Periodic Inventory Report Widget & Caching (Section 9)
    QWidget* reportPageWidget;
    QComboBox* reportMonthCombo;
    QSpinBox* reportYearSpin;
    QPushButton* btnGenerateReport;
    QPushButton* btnExportCsv;
    QTableWidget* reportTable;
    QLabel* reportSummaryLabel;

    bool reportDataCached;
    int cachedMonth;
    int cachedYear;
    Vector<Record> cachedReportRecords;
    Record cachedGrandTotal;
    int currentReportSortColumn;
    bool currentReportSortAscending;
};

#endif // MAIN_WINDOW_H
