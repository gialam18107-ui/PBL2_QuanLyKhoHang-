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
#include <QMenu>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

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
    QStackedWidget* stepStack = nullptr;
    QPushButton* step1Btn = nullptr;
    QPushButton* step2Btn = nullptr;
    QPushButton* step3Btn = nullptr;
    QPushButton* prevBtn = nullptr;
    QPushButton* nextBtn = nullptr;
    QPushButton* saveBtn = nullptr;
    QPushButton* cancelBtn = nullptr;

    QComboBox* typeCombo = nullptr;
    QLabel* customerLabel = nullptr;
    QWidget* customerContainer = nullptr;
    QComboBox* customerCombo = nullptr;
    QPushButton* addCustomerBtn = nullptr;
    QLabel* sourceCodeLabel = nullptr;
    QComboBox* sourceCodeCombo = nullptr;
    QPushButton* viewCheckBtn = nullptr;
    QDateEdit* orderDateEdit = nullptr;
    QDateEdit* expectedDateEdit = nullptr;
    QLineEdit* noteEdit = nullptr;

    QComboBox* productCombo = nullptr;
    QSpinBox* quantitySpin = nullptr;
    QPushButton* addItemBtn = nullptr;
    QPushButton* pickUnitsBtn = nullptr;
    QLabel* productStockHintLabel = nullptr;
    QTableWidget* itemsTable = nullptr;
    QLabel* itemsSummaryLabel = nullptr;

    void updateProductStockHint(int idx);


    // Logistics allocation controls (Rule 4.3)
    QRadioButton* singleBatchRadio = nullptr;
    QRadioButton* multiBatchRadio = nullptr;
    QWidget* singleBatchContainer = nullptr;
    QLabel* singleBatchSummaryLabel = nullptr;
    QWidget* multiBatchContainer = nullptr;
    QTableWidget* multiBatchTable = nullptr;
    QPushButton* addBatchBtn = nullptr;
    QPushButton* removeBatchBtn = nullptr;
    QLabel* multiBatchStatusLabel = nullptr;

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
    Database* db = nullptr;
    int txnId = 0;
    QString orderCodeStr;
    QString initialStatus;
    QComboBox* statusCombo = nullptr;
    int totalActQty = 0;
    int totalPlanQty = 0;
};

/**
 * @brief Modal confirmation dialog required before transitioning an order or logistics batch to COMPLETED (Rule 4.5 & Rule 5.6).
 * Features explicit warning, a mandatory confirmation tick-box, and Confirm/Cancel buttons.
 */
class CompletionConfirmationDialog : public QDialog
{
public:
    CompletionConfirmationDialog(
        const QString& itemCode,
        const QString& itemDescription = "đợt giao vận",
        QWidget* parent = nullptr,
        bool isUnderFulfilled = false,
        int actualQuantity = 0,
        int plannedQuantity = 0
    );
    ~CompletionConfirmationDialog() override = default;

private:
    QCheckBox* confirmCheck = nullptr;
    QPushButton* confirmBtn = nullptr;
    QPushButton* cancelBtn = nullptr;
};

/**
 * @brief Modal confirmation dialog required before transitioning an order or logistics batch to CANCELLED (Rule 5.6).
 * Features explicit warning, a mandatory confirmation tick-box, and Confirm/Cancel buttons.
 */
class CancellationConfirmationDialog : public QDialog
{
public:
    CancellationConfirmationDialog(
        const QString& itemCode,
        const QString& itemDescription = "đơn hàng",
        QWidget* parent = nullptr
    );
    ~CancellationConfirmationDialog() override = default;

private:
    QCheckBox* confirmCheck = nullptr;
    QPushButton* confirmBtn = nullptr;
    QPushButton* cancelBtn = nullptr;
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
    Database* db = nullptr;
    int logId = 0;
    QString batchCodeStr;
    QString orderDateStr;
    QString initialStatusStr;
    QComboBox* statusCombo = nullptr;
    QCheckBox* hasActualDateCheck = nullptr;
    QDateEdit* actualDateEdit = nullptr;
    QPushButton* saveBtn = nullptr;
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
    Database* db = nullptr;
    QString batchCodeStr;
    QTableWidget* summaryTable = nullptr;
    QLabel* totalCountLabel = nullptr;
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

    QDateEdit* expectedDateEdit = nullptr;
    QTableWidget* itemsTable = nullptr;
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
        QWidget* parent = nullptr,
        const std::string& sourceCode = ""
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
    std::string sourceCode;

    QLineEdit* searchInput = nullptr;
    QPushButton* quickSelectBtn = nullptr;
    QPushButton* selectAllBtn = nullptr;
    QPushButton* deselectAllBtn = nullptr;
    QTableWidget* unitsTable = nullptr;
    QLabel* selectionCountLabel = nullptr;
    QPushButton* confirmBtn = nullptr;
    QPushButton* cancelBtn = nullptr;

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
    QComboBox* productFilterCombo = nullptr;
    QCheckBox* onlyEligibleCheck = nullptr;
    QLineEdit* searchSerialInput = nullptr;
    QPushButton* selectAllBtn = nullptr;
    QPushButton* deselectAllBtn = nullptr;
    QTableWidget* unitsTable = nullptr;
    QLabel* selectionSummaryLabel = nullptr;
    QPushButton* confirmBtn = nullptr;

    // Tab 2 controls (Generators)
    QLineEdit* generatedSerialDisplay = nullptr;
    QPushButton* genSingleSerialBtn = nullptr;
    QPushButton* fillToEntryBtn = nullptr;
    QPushButton* recordNowBtn = nullptr;
    QLabel* batchMissingSummaryLabel = nullptr;
    QPushButton* batchAutoReceiveBtn = nullptr;

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

    bool recordPendingInboundSerial(int productId, const QString& serial, const QString& status, QString& outErrMsg, bool refreshViews = true);
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
    void refreshBatchUnitsTable(bool scrollToBottom = false);
    void refreshBatchOverviewTable();
    void refreshProductEntryTab(bool scrollToBottom = false);
    void refreshAllViews(bool scrollToBottom = false);
    void updateSaveButtonState();

    void onDirectSerialEntered();
    void onOpenProductUnitPicker(int productId, const QString& pCode, const QString& pName, int missingQty);
    void onOpenSimulationToolsDialog();
    void onUnitStatusChanged(int unitId, const QString& newStatus);
    void onSaveChanges();

    void setupProductEntryTab();
    void onInboundProductComboChanged(int index);
    void onRecordInboundSerial();
    void onBatchPasteSerials();

    QStringList getAllowedStatusOptions() const;

    HubType hubType;
    Database* db;
    int logId;
    QString batchCodeStr;
    QString parentOrderCodeStr;
    std::string orderSourceCode;
    std::string transactionType;
    std::string parentOrderStatus;
    bool isLogisticsCompleted = false;
    bool hasModifiedUnits = false;

    // Toast notification overlay
    QLabel* toastOverlayLabel = nullptr;
    QTimer* toastTimer = nullptr;

    // Structure
    QTabWidget* mainTabWidget = nullptr;
    QWidget* tabBatchOverview = nullptr;
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

    // Tab 2 controls
    QLineEdit* unifiedSerialInput = nullptr;
    QPushButton* recordDirectSerialBtn = nullptr;
    QPushButton* openSimulationToolsBtn = nullptr;
    QTableWidget* batchOverviewTable = nullptr;
    QTableWidget* batchUnitsTable = nullptr;
    QLabel* batchUnitsCountLabel = nullptr;

    // Bottom action controls
    QLabel* statusSummaryLabel = nullptr;
    QPushButton* saveChangesBtn = nullptr;
    QPushButton* closeBtn = nullptr;

    SimulationToolDialog* simulationDialog = nullptr;

    void syncCache();
    std::unordered_set<std::string> stagedSerialSet;
    std::unordered_set<std::string> pendingInboundSerialSet;
    std::unordered_set<std::string> pendingOutboundSerialSet;
    std::unordered_map<int, int> pendingInboundCountMap;
    std::unordered_map<int, int> pendingOutboundCountMap;

    std::vector<StagedUnit> stagedUnits;
    std::vector<PendingInboundUnit> pendingInboundUnits;
    std::vector<PendingOutboundUnit> pendingOutboundUnits;
    std::vector<int> recentUnitIds; // Up to 3 most recently entered/updated units
    std::vector<BatchItemOverview> batchItems;
};

#endif // ORDER_DIALOG_H
