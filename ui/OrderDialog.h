#ifndef ORDER_DIALOG_H
#define ORDER_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDateEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QStackedWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QTimer>
#include <QProgressBar>
#include <QTextEdit>
#include <QCloseEvent>
#include <QDateTime>
#include <vector>
#include <string>

#include "database/Database.h"
#include "schema/UserCreate.h"
#include "ui/TransactionHubWidget.h"

/**
 * @brief Coupled Order & Primary Logistics Creation Dialog (Rule 4.3, 4.4, 6.3.1, 7.2).
 * Supports both Inbound Orders (inbound_transaction + inbound_detail + delivery + delivery_detail)
 * and Outbound Orders (outbound_transaction + outbound_detail + shipment + shipment_detail).
 * Supports single-batch and multi-batch logistics allocations (Rule 4.3).
 */
class OrderDialog : public QDialog
{
public:
    OrderDialog(HubType type, Database* database, QWidget* parent = nullptr);
    ~OrderDialog() override = default;

    bool didCreateAuxCustomer() const { return auxCustomerCreated; }
    QString getCreatedOrderCode() const { return createdOrderCode; }

private:
    struct OrderLineItem {
        int productId;
        QString productCode;
        QString productName;
        int availQty;
        int warrantyQty;
        int scrappedQty;
        int orderedQty;
        std::vector<std::string> assignedSerials;
    };

    struct MultiBatchDef {
        QString expectedDate;
        std::vector<int> allocatedQuantities; // Corresponding to lineItems
    };

    void setupUI();
    void populateOrderTypes();
    void populateCustomers(const QString& selectCode = "");
    void populateActiveProducts();
    void onOrderTypeChanged(int index);
    void onQuickAddCustomer();
    void onAddLineItem();
    void onPickUnitsForProduct();
    void removeLineItem(int row);
    void renderLineItems();
    void onSaveOrder();

    // Multi-batch schedule helpers
    void setupMultiBatchUI(QVBoxLayout* parentLayout);
    void onToggleMultiBatch(bool checked);
    void onAddScheduleBatch();
    void onRemoveScheduleBatch();
    void renderMultiBatchTable();

    // Wizard navigation helpers
    void goToStep(int stepIndex);
    bool validateCurrentStep();
    void updateStepBar();
    QWidget* createStep1Widget();
    QWidget* createStep2Widget();
    QWidget* createStep3Widget();

    HubType hubType;
    Database* db;
    bool auxCustomerCreated;

    // Wizard navigation controls
    QStackedWidget* stepStack;
    QPushButton* step1Btn;
    QPushButton* step2Btn;
    QPushButton* step3Btn;
    QPushButton* prevBtn;
    QPushButton* nextBtn;
    QPushButton* saveBtn;
    QPushButton* cancelBtn;

    QComboBox* typeCombo;
    QLabel* customerLabel;
    QWidget* customerContainer;
    QComboBox* customerCombo;
    QPushButton* addCustomerBtn;
    QLabel* sourceCodeLabel;
    QComboBox* sourceCodeCombo;
    QPushButton* viewCheckBtn;
    QDateEdit* orderDateEdit;
    QDateEdit* expectedDateEdit;
    QLineEdit* noteEdit;

    QComboBox* productCombo;
    QSpinBox* quantitySpin;
    QPushButton* addItemBtn;
    QPushButton* pickUnitsBtn;
    QLabel* productStockHintLabel;
    QTableWidget* itemsTable;
    QLabel* itemsSummaryLabel;

    void updateProductStockHint(int idx);


    // Logistics allocation controls (Rule 4.3)
    QRadioButton* singleBatchRadio;
    QRadioButton* multiBatchRadio;
    QWidget* singleBatchContainer;
    QLabel* singleBatchSummaryLabel;
    QWidget* multiBatchContainer;
    QTableWidget* multiBatchTable;
    QPushButton* addBatchBtn;
    QPushButton* removeBatchBtn;
    QLabel* multiBatchStatusLabel;

    std::vector<OrderLineItem> lineItems;
    std::vector<MultiBatchDef> scheduleBatches;
    QString createdOrderCode;
};

/**
 * @brief Modal dialog to update transaction status (inbound_transaction / outbound_transaction) (Rule 5.3).
 */
class TransactionStatusDialog : public QDialog
{
public:
    TransactionStatusDialog(
        HubType type,
        Database* database,
        int transactionId,
        const QString& orderCode,
        const QString& customerCode,
        const QString& orderDate,
        const QString& currentStatus,
        QWidget* parent = nullptr
    );
    ~TransactionStatusDialog() override = default;

    std::string getSelectedStatus() const;

private:
    void onSave();

    HubType hubType;
    Database* db;
    int txnId;
    QString orderCodeStr;
    QString initialStatus;
    QComboBox* statusCombo;
};

/**
 * @brief Modal confirmation dialog required before transitioning an order or logistics batch to COMPLETED (Rule 4.5).
 * Features explicit warning, a mandatory confirmation tick-box, and Confirm/Cancel buttons.
 */
class CompletionConfirmationDialog : public QDialog
{
public:
    CompletionConfirmationDialog(
        const QString& itemCode,
        const QString& itemDescription = "đợt giao vận",
        QWidget* parent = nullptr
    );
    ~CompletionConfirmationDialog() override = default;

private:
    QCheckBox* confirmCheck;
    QPushButton* confirmBtn;
    QPushButton* cancelBtn;
};

/**
 * @brief Modal dialog to update logistics batch status and actual date (delivery / shipment) (Rule 5.3).
 */
class LogisticsUpdateDialog : public QDialog
{
public:
    LogisticsUpdateDialog(
        HubType type,
        Database* database,
        int logisticsId,
        const QString& batchCode,
        const QString& parentOrderCode,
        const QString& expectedDate,
        const QString& currentActualDate,
        const QString& currentStatus,
        QWidget* parent = nullptr
    );
    ~LogisticsUpdateDialog() override = default;

    std::string getSelectedStatus() const;
    std::string getSelectedActualDate() const;

private:
    void onSave();

    HubType hubType;
    Database* db;
    int logId;
    QString batchCodeStr;
    QString orderDateStr;
    QString initialStatusStr;
    QComboBox* statusCombo;
    QCheckBox* hasActualDateCheck;
    QDateEdit* actualDateEdit;
    QPushButton* saveBtn;
};

/**
 * @brief Modal dialog displaying status breakdown per product for a delivery/shipment batch.
 */
class LogisticsProductStatusSummaryDialog : public QDialog
{
public:
    LogisticsProductStatusSummaryDialog(
        HubType type,
        Database* database,
        const QString& batchCode,
        QWidget* parent = nullptr
    );
    ~LogisticsProductStatusSummaryDialog() override = default;

private:
    void setupUI();
    void loadData();

    HubType hubType;
    Database* db;
    QString batchCodeStr;
    QTableWidget* summaryTable;
    QLabel* totalCountLabel;
};

/**
 * @brief Dedicated inspection sub-window displaying line items for an Order or Logistics batch (Rule 7.2).
 */
class DetailInspectionDialog : public QDialog
{
public:
    enum class InspectionMode {
        INBOUND_ORDER,
        OUTBOUND_ORDER,
        DELIVERY_BATCH,
        SHIPMENT_BATCH
    };

    DetailInspectionDialog(
        InspectionMode mode,
        Database* database,
        const QString& targetCode,
        const QString& extraSummary = "",
        QWidget* parent = nullptr
    );
    ~DetailInspectionDialog() override = default;
};

/**
 * @brief Modal dialog to create an additional delivery or shipment batch (Rule 4.3).
 */
class LogisticsBatchDialog : public QDialog
{
public:
    LogisticsBatchDialog(
        HubType type,
        Database* database,
        int transactionId,
        const QString& orderCode,
        const QString& orderDate,
        QWidget* parent = nullptr
    );
    ~LogisticsBatchDialog() override = default;

    QString getCreatedBatchCode() const { return createdBatchCode; }

private:
    struct BatchLineItem {
        int productId;
        QString productCode;
        QString productName;
        int orderedQty;
        int alreadyAllocatedQty;
        int remainingToAllocate;
        int currentStockAvail;
        int currentStockWrt;
        int currentStockScrap;
        int thisBatchQty;
    };

    void setupUI();
    void loadOrderProducts();
    void renderItems();
    void onSave();

    HubType hubType;
    Database* db;
    int txnId;
    QString orderCodeStr;
    QString orderDateStr;
    std::string transactionType;

    QDateEdit* expectedDateEdit;
    QTableWidget* itemsTable;
    std::vector<BatchLineItem> lineItems;
    QString createdBatchCode;
};

struct BatchItemOverview {
    int productId = 0;
    QString productCode;
    QString productName;
    int batchQty = 0;
    int actualQty = 0;
    // Batch unit breakdown:
    int batchAvail = 0;
    int batchWrt = 0;
    int batchScrap = 0;
    int batchOut = 0;
    // Total warehouse stock:
    int stockAvail = 0;
    int stockWrt = 0;
    int stockScrap = 0;
};

/**
 * @brief Modal dialog to pick candidate units from warehouse inventory for a specific product during Outbound operations.
 */
class OutboundProductUnitPickerDialog : public QDialog
{
public:
    OutboundProductUnitPickerDialog(
        Database* database,
        int productId,
        const QString& productCode,
        const QString& productName,
        int targetQuantity = 0,
        const std::string& transactionType = "",
        const std::vector<std::string>& excludedSerials = {},
        QWidget* parent = nullptr
    );
    ~OutboundProductUnitPickerDialog() override = default;

    std::vector<std::string> getSelectedSerials() const { return selectedSerials; }

private:
    struct CandidateRow {
        int unitId;
        std::string serial;
        std::string status;
    };

    void setupUI();
    void loadCandidates();
    void filterTable();
    void updateSelectionCount();

    Database* db;
    int prodId;
    QString prodCode;
    QString prodName;
    int targetQty;
    std::string txnType;
    std::vector<std::string> excludeList;

    QLineEdit* searchInput;
    QPushButton* quickSelectBtn;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QTableWidget* unitsTable;
    QLabel* selectionCountLabel;
    QPushButton* confirmBtn;
    QPushButton* cancelBtn;

    std::vector<CandidateRow> candidateList;
    std::vector<std::string> selectedSerials;
};

class UnitEntryDialog;

/**
 * @brief Standalone companion window for Testing & Hardware Simulation (Rule 7.2).
 * Runs parallel/side-by-side with UnitEntryDialog to provide tester utilities:
 * - Tab 1: Pick candidate units from warehouse inventory (filter, multi-select, select all).
 * - Tab 2: Single random serial generator & batch auto-creation for remaining order quotas.
 */
class SimulationToolDialog : public QDialog
{
public:
    SimulationToolDialog(
        HubType type,
        Database* database,
        int logisticsId,
        const QString& batchCode,
        const std::string& txnType,
        const std::vector<std::pair<int, QString>>& orderProducts,
        UnitEntryDialog* entryDialog,
        QWidget* parent = nullptr
    );
    ~SimulationToolDialog() override = default;

    void refreshCandidateList();
    void updateMissingSummary();

private:
    struct CandidateUnit {
        int unitId;
        std::string serial;
        int productId;
        std::string productName;
        std::string productCode;
        std::string status;
        bool isEligible;
        QString reason;
    };

    void setupUI();
    void loadCandidateUnits();
    void filterTable();
    void onSelectionChanged();
    void onSelectAllEligible();
    void onDeselectAll();
    void onConfirmCandidates();
    void onGenerateSingleRandomSerial();
    void onFillSerialToEntry();
    void onDirectRecordGeneratedSerial();
    void onBatchAutoGenerate();

    bool isUnitEligible(const std::string& status) const;
    QString getEligibilityReason(const std::string& status) const;

    HubType hubType;
    Database* db;
    int logId;
    QString batchCodeStr;
    std::string transactionType;
    std::vector<std::pair<int, QString>> products;
    UnitEntryDialog* entryDlg;

    // Tab 1 controls (Warehouse candidates)
    QComboBox* productFilterCombo;
    QCheckBox* onlyEligibleCheck;
    QLineEdit* searchSerialInput;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QTableWidget* unitsTable;
    QLabel* selectionSummaryLabel;
    QPushButton* confirmBtn;

    // Tab 2 controls (Generators)
    QLineEdit* generatedSerialDisplay;
    QPushButton* genSingleSerialBtn;
    QPushButton* fillToEntryBtn;
    QPushButton* recordNowBtn;
    QLabel* batchMissingSummaryLabel;
    QPushButton* batchAutoReceiveBtn;

    std::vector<CandidateUnit> candidates;
};

/**
 * @brief Modal dialog for Unit Entry (receiving units into Delivery or dispatching units for Shipment) (Rule 4.4, 7.2).
 */
class UnitEntryDialog : public QDialog
{
public:
    UnitEntryDialog(
        HubType type,
        Database* database,
        int logisticsId,
        const QString& batchCode,
        const QString& parentOrderCode,
        QWidget* parent = nullptr
    );
    ~UnitEntryDialog() override;

    bool didModifyUnits() const { return hasModifiedUnits; }
    void showToast(const QString& message, bool isSuccess = true);

    // Methods for SimulationToolDialog communication
    void fillSerialInput(const QString& serial);
    bool recordCustomSerial(const QString& serial, QString& outErrMsg);
    void onBatchAutoGenerateUnits();
    void onCandidateSerialsSelected(const Vector<std::string>& serials);
    std::vector<std::pair<int, QString>> getOrderProductsList() const;
    const std::vector<BatchItemOverview>& getBatchItems() const { return batchItems; }

    bool recordPendingInboundSerial(int productId, const QString& serial, const QString& status, QString& outErrMsg);
    bool isSerialInPendingOutbound(const std::string& sn) const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    struct StagedUnit {
        int unitId = 0;
        std::string serial;
        int productId = 0;
        QString productCode;
        QString productName;
        std::string originalStatus; // Initial status before entering batch / at creation
        std::string currentStatus;  // Staged / new status in dropdown
        std::string savedStatus;    // Currently saved status in database
        int historyId = 0;
        bool isModified = false;
    };

    struct PendingInboundUnit {
        int productId = 0;
        QString productCode;
        QString productName;
        std::string serial;
        std::string status;
        QString addedTime;
    };

    struct PendingOutboundUnit {
        int unitId = 0;
        int productId = 0;
        QString productCode;
        QString productName;
        std::string serial;
        std::string currentStatus;
        QString addedTime;
    };

    int getPendingCountForProduct(int productId) const;
    bool isSerialInPending(const std::string& sn) const;
    void removePendingUnitBySerial(const std::string& sn);

    int getPendingOutboundCountForProduct(int productId) const;
    void removePendingOutboundUnitBySerial(const std::string& sn);
    bool recordPendingOutboundSerial(int productId, const QString& serial, QString& outErrMsg);

    void setupUI();
    void loadBatchData();
    void loadAllUnitsData();
    void refreshAllUnitsTable();
    void refreshRecentUnitsTable();
    void refreshBatchOverviewTable();
    void updateSaveButtonState();

    void onDirectSerialEntered();
    void onOpenProductUnitPicker(int productId, const QString& pCode, const QString& pName, int missingQty);
    void onOpenSimulationToolsDialog();
    void onUnitStatusChanged(int unitId, const QString& newStatus);
    void onSaveChanges();

    void setupProductEntryTab();
    void refreshProductEntryTab();
    void onInboundProductComboChanged(int index);
    void onRecordInboundSerial();
    void onBatchPasteSerials();

    QStringList getAllowedStatusOptions() const;

    HubType hubType;
    Database* db;
    int logId;
    QString batchCodeStr;
    QString parentOrderCodeStr;
    std::string transactionType;
    bool isLogisticsCompleted = false;
    bool hasModifiedUnits = false;

    // Toast notification overlay
    QLabel* toastOverlayLabel = nullptr;
    QTimer* toastTimer = nullptr;

    // Structure
    QTabWidget* mainTabWidget;
    QWidget* tabAllUnits;
    QWidget* tabBatchOverview;
    QWidget* tabProductEntry = nullptr;

    // Inbound Product Entry Tab controls
    QComboBox* inboundProductCombo = nullptr;
    QComboBox* inboundTargetStatusCombo = nullptr;
    QFrame* prodProgressCard = nullptr;
    QLabel* prodTargetQtyLabel = nullptr;
    QLabel* prodActualQtyLabel = nullptr;
    QLabel* prodRemQtyLabel = nullptr;
    QProgressBar* prodProgressBar = nullptr;
    QLabel* prodProgressBadge = nullptr;
    QLabel* overallBatchProgressLabel = nullptr;

    QLineEdit* inboundSerialInput = nullptr;
    QPushButton* recordInboundSerialBtn = nullptr;
    QPushButton* batchPasteSerialsBtn = nullptr;
    QPushButton* openSimFromTab1Btn = nullptr;
    QLabel* inboundFeedbackLabel = nullptr;

    QTableWidget* prodUnitsTable = nullptr;
    QLabel* prodUnitsCountLabel = nullptr;

    // Tab 1 controls
    QLineEdit* serialSearchInput;
    QComboBox* productFilterCombo;
    QTableWidget* allUnitsTable;
    QLabel* allUnitsCountLabel;

    // Tab 2 controls
    QLineEdit* unifiedSerialInput;
    QPushButton* recordDirectSerialBtn;
    QPushButton* openSimulationToolsBtn;
    QTableWidget* batchOverviewTable;
    QTableWidget* recentUnitsTable;
    QLabel* recentUnitsLabel;

    // Bottom action controls
    QLabel* statusSummaryLabel;
    QPushButton* saveChangesBtn;
    QPushButton* closeBtn;

    SimulationToolDialog* simulationDialog = nullptr;

    std::vector<StagedUnit> stagedUnits;
    std::vector<PendingInboundUnit> pendingInboundUnits;
    std::vector<PendingOutboundUnit> pendingOutboundUnits;
    std::vector<int> recentUnitIds; // Up to 3 most recently entered/updated units
    std::vector<BatchItemOverview> batchItems;
};

#endif // ORDER_DIALOG_H
