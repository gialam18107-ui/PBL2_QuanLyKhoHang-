#ifndef TRANSACTION_HUB_WIDGET_H
#define TRANSACTION_HUB_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QSplitter>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTabWidget>
#include <QStringList>
#include <functional>
#include <vector>
#include <string>

#include "datastructure/vector/Vector.h"
#include "database/Database.h"
#include "database/Record.h"
#include "ui/QueryTableWidget.h" // Reuses FilterType, FilterColumnDef

enum class HubType {
    INBOUND,
    OUTBOUND
};

class TransactionHubWidget : public QWidget
{
public:
    using FilterCallback = std::function<void(const std::string& columnKey, const QString& val1, const QString& val2, bool isRange)>;
    using DataChangedCallback = std::function<void(const QString& summary)>;

    TransactionHubWidget(HubType type, Database* database, const QString& title, const QString& description, QWidget* parent = nullptr);

    void setMasterColumns(const QStringList& displayHeaders, const std::vector<std::string>& columnKeys);
    void setFilterColumns(const std::vector<FilterColumnDef>& filterDefs);
    void setData(const Vector<Record>& records);
    void setRefreshCallback(std::function<void()> callback);
    void setFilterCallback(FilterCallback callback);
    void setDataChangedCallback(DataChangedCallback callback);

    using MarkDirtyCallback = std::function<void(const QString&)>;
    void setMarkDirtyCallback(MarkDirtyCallback callback) { onMarkDirty = callback; }

    void updateMasterRowInPlace(int row, const std::string& fieldKey, const Value& newValue);
    void updateLogisticsRowInPlace(int row, const std::string& fieldKey, const Value& newValue);

    void resetFilter();
    QTableWidget* getMasterTable() const { return masterTable; }

private:
    void setupUI();
    void setupFilterUI(QVBoxLayout* layout);
    void setupToolbarUI(QVBoxLayout* layout);
    void setupSplitterUI(QVBoxLayout* layout);

    void onFilterColumnChanged(int index);
    void applyFilter();
    void filterMasterRows(const QString& query);
    void renderMasterRows();

    void onTransactionSelected(int row);
    void onLogisticsSelected(int row);

    void loadOrderDetails(const std::string& orderCode);
    void loadLogisticsBatches(const std::string& orderCode);
    void loadLogisticsDetails(const std::string& logisticsCode);

    void renderOrderDetails();
    void renderLogisticsBatches();
    void renderLogisticsDetails();

    void onLogisticsProductSummaryClicked();

    const Record* getSelectedMasterRecord(bool warnIfNone = true);
    const Record* getSelectedLogisticsRecord(bool warnIfNone = true);

    void onCreateOrderClicked();
    void onUpdateOrderStatusClicked();
    void onInspectOrderClicked();
    void onAddLogisticsBatchClicked();
    void onUpdateLogisticsClicked();
    void onInspectLogisticsClicked();
    void onManageUnitsClicked();

    void exportMasterCsv();

    static QString formatValue(const Value& val);

    HubType hubType;
    Database* db;
    QString titleText;
    QString descText;

    // Data caches
    Vector<Record> cachedTransactions;
    Vector<Record> cachedOrderDetails;
    Vector<Record> cachedLogistics;
    Vector<Record> cachedLogisticsDetails;

    std::string currentOrderCode;
    std::string currentLogisticsCode;

    // Sorting state
    int currentSortColumn = -1;
    bool currentSortAscending = true;

    int currentDetailSortCol = -1;
    bool currentDetailSortAsc = true;

    int currentLogisticsSortCol = -1;
    bool currentLogisticsSortAsc = true;

    int currentLogisticsDetailSortCol = -1;
    bool currentLogisticsDetailSortAsc = true;

    // Header info
    QLabel* titleLabel;
    QLabel* descLabel;

    // Filter controls
    QFrame* filterFrame;
    QComboBox* filterColumnCombo;
    QLineEdit* filterExactInput;
    QWidget* filterDateWidget;
    QDateEdit* fromDateEdit;
    QDateEdit* toDateEdit;
    QComboBox* filterSelectCombo;
    QPushButton* applyFilterBtn;
    QPushButton* resetFilterBtn;

    // Toolbar controls
    QLineEdit* searchInput;
    QLabel* countLabel;
    QPushButton* createOrderBtn;
    QPushButton* addLogisticsBatchBtn;
    QPushButton* updateOrderStatusBtn;
    QPushButton* inspectOrderBtn;
    QPushButton* refreshBtn;
    QPushButton* exportBtn;

    // Logistics action controls
    QPushButton* manageUnitsBtn;
    QPushButton* updateLogisticsBtn;
    QPushButton* inspectLogisticsBtn;

    // UI Tables & Labels
    QTableWidget* masterTable;
    QLabel* detailBanner;

    QTableWidget* orderDetailTable;
    QLabel* orderDetailCountLabel;

    QTableWidget* logisticsTable;
    QLabel* logisticsCountLabel;

    QTableWidget* logisticsDetailTable;
    QLabel* logisticsDetailCountLabel;

    // Lower Right Batch Items components
    QPushButton* btnLogisticsProductSummary;

    // Maximize/Restore support
    void toggleMaximizePanel(int panelIndex);

    QSplitter* mainSplitter;
    QWidget* masterContainer;
    QWidget* bottomContainer;

    QSplitter* horizontalSplitter;
    QWidget* leftContainer;
    QWidget* rightContainer;

    QSplitter* rightVerticalSplitter;
    QWidget* rightUpperContainer;
    QWidget* rightLowerContainer;

    QPushButton* btnMaxMaster;
    QPushButton* btnMaxLeft;
    QPushButton* btnMaxRightUpper;
    QPushButton* btnMaxRightLower;
    int currentMaximizedPanel;

    QStringList masterHeaders;
    std::vector<std::string> masterKeys;
    std::vector<FilterColumnDef> filterDefs;

    std::function<void()> onRefresh;
    FilterCallback onFilter;
    DataChangedCallback onDataChanged;
    MarkDirtyCallback onMarkDirty;
};

#endif // TRANSACTION_HUB_WIDGET_H
