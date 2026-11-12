#ifndef INVENTORY_CHECK_DIALOG_H
#define INVENTORY_CHECK_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTimer>
#include <QTableWidget>
#include <QLabel>
#include <QTabWidget>
#include <QGroupBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QSplitter>
#include <QFrame>
#include <QScrollArea>
#include <QPainter>
#include <QMenu>
#include <vector>
#include <string>
#include <utility>
#include <unordered_set>
#include <unordered_map>

#include "database/Database.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserQuery.h"

/**
 * @brief Custom 2-Tier Header View for Product Reconciliation Matrix and Progress Tables.
 * Renders grouped columns:
 * Columns 0..4: STT, Mã sản phẩm, Tên sản phẩm, Tổng tồn, Thực tế (full height)
 * Columns 5..6: Khả dụng (sub: Tồn | Thực tế)
 * Columns 7..8: Bảo hành (sub: Tồn | Thực tế)
 * Columns 9..10: Hư hỏng (sub: Tồn | Thực tế)
 */
class GroupedReconcileHeaderView : public QHeaderView
{
public:
    explicit GroupedReconcileHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);
    ~GroupedReconcileHeaderView() override = default;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

/**
 * @brief Dialog to create a new Inventory Check with upfront product scope declaration.
 */
class InventoryCheckCreateDialog : public QDialog
{
public:
    explicit InventoryCheckCreateDialog(Database* database, QWidget* parent = nullptr);
    ~InventoryCheckCreateDialog() override = default;

    QString getCreatedCheckCode() const { return createdCode; }
    int getCreatedCheckId() const { return createdId; }

private:
    void setupUI();
    void loadProducts();
    void onSelectAll(bool select);
    void onCreateCheck();

    Database* db;
    QComboBox* typeCombo;
    QDateEdit* dateEdit;
    QLineEdit* noteEdit;
    QLineEdit* searchEdit;
    QTableWidget* productTable;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QLabel* countSelectedLabel;
    QPushButton* createBtn;
    QPushButton* cancelBtn;

    QString createdCode;
    int createdId;

    struct ProductItem {
        int productId;
        QString code;
        QString name;
        QString category;
        int avail;
        int warr;
        int scrap;
    };
    std::vector<ProductItem> productList;
};

/**
 * @brief Dialog to record unrecorded surplus items (Type 2 Surplus - Rule 5.3.1).
 * Increments actual product counts in inventory_check_quantity without creating premature unit_inventory rows.
 */
class UnrecordedSurplusDialog : public QDialog
{
public:
    UnrecordedSurplusDialog(Database* database, int checkId, const std::vector<std::pair<int, QString>>& products, int defaultPid = 0, QWidget* parent = nullptr);
    ~UnrecordedSurplusDialog() override = default;

    bool wasRecorded() const { return recorded; }
    int getSelectedProductId() const { return selectedPid; }
    QString getSelectedCondition() const { return selectedCondition; }
    std::vector<QString> getEnteredSerials() const { return enteredSerials; }

private:
    void setupUI();
    void onConfirm();
    void onGenerateSerial();

    Database* db;
    int checkId;
    std::vector<std::pair<int, QString>> productList;
    int defaultProductId;
    bool recorded;

    int selectedPid = 0;
    QString selectedCondition;
    std::vector<QString> enteredSerials;

    QComboBox* productCombo;
    QComboBox* conditionCombo;
    QLineEdit* serialEdit;
    QPushButton* genSerialBtn;
    QLineEdit* noteEdit;
    QPushButton* confirmBtn;
    QPushButton* cancelBtn;
};

/**
 * @brief Post-Check Abnormal Unit Summarization & Reconciliation Dialog (Rule 5.3.2).
 * Implements the 3-Tier Reconciliation Funnel:
 * Tier 1: Check-wide KPI Summary Cards & Discrepancy Filter
 * Tier 2: Product-Level Matrix & Strict No-Netting Invariant
 * Tier 3: 3 Action-Oriented Buckets (ADJ-OUT, ADJ-IN, ADJ-MOVEMENT) with Idempotent Locking
 */
class InventoryCheckReconciliationDialog : public QDialog
{
public:
    InventoryCheckReconciliationDialog(Database* database, int checkId, QWidget* parent = nullptr);
    ~InventoryCheckReconciliationDialog() override = default;

    bool wasOrderGenerated() const { return orderGenerated; }

private:
    void setupUI();
    void loadReconciliationData();
    void updateLockingState();
    void onCreateOutboundAdj();
    void onCreateInboundAdj();
    void onCreateMovementAdj();

    Database* db;
    int checkId;
    QString checkCode;
    QString checkType;
    QString checkStatus;
    bool orderGenerated;

    // Tier 1: KPI Summary Cards & Filter
    QLabel* kpiSystemStockLabel;
    QLabel* kpiActualStockLabel;
    QLabel* kpiMatchedLabel;
    QLabel* kpiMissingLabel;
    QLabel* kpiSurplusLabel;
    QLabel* kpiStatusShiftLabel;
    QCheckBox* abnormalFilterCheck;

    // Tier 2: Product Matrix Table (Bảng đối soát sản phẩm - 11 columns)
    QTableWidget* matrixTable;
    QLineEdit* matrixSearchEdit = nullptr;

    // Tier 3: 3 Action Buckets (Tabs)
    QTabWidget* bucketTabWidget;

    // Bucket 1: Hàng cần nhập điều chỉnh
    QTableWidget* missingUnitTable;
    QPushButton* missingSelectAllBtn;
    QPushButton* missingDeselectAllBtn;
    QPushButton* createInboundBtn;
    QLabel* inboundStatusLabel;

    // Bucket 2: Hàng cần xuất điều chỉnh
    QTableWidget* surplusDetailTable;
    QPushButton* surplusSelectAllBtn;
    QPushButton* surplusDeselectAllBtn;
    QPushButton* createOutboundBtn;
    QLabel* outboundStatusLabel;

    // Bucket 3: Điều chỉnh trạng thái
    QTableWidget* movementDetailTable;
    QPushButton* movementSelectAllBtn;
    QPushButton* movementDeselectAllBtn;
    QPushButton* createMovementBtn;
    QLabel* movementStatusLabel;

    struct ProductReconcileItem {
        int productId;
        QString productCode;
        QString productName;
        int systemStock;
        int actualCounted;
        int matchedCount;
        int surplusCount;
        int missingCount;
        int statusShiftCount;
        int unrecordedSurplus;
        int sysAvail = 0;
        int actAvail = 0;
        int sysWarr = 0;
        int actWarr = 0;
        int sysScrap = 0;
        int actScrap = 0;
    };
    std::vector<ProductReconcileItem> productReconcileList;

    struct MissingUnitItem {
        int checkUnitId;
        int productId;
        QString serial;
        QString productCode;
        QString productName;
        QString systemStatus;
    };
    std::vector<MissingUnitItem> missingUnitList;

    struct SurplusUnitItem {
        QString type; // "REDISCOVERED_OUT" or "UNRECORDED"
        int productId;
        QString serial;
        QString productCode;
        QString productName;
        QString currentStatus;
        QString physicalCondition;
        int quantity;
    };
    std::vector<SurplusUnitItem> surplusUnitList;

    struct StatusShiftUnitItem {
        int checkUnitId;
        int productId;
        QString serial;
        QString productCode;
        QString productName;
        QString fromStatus;
        QString toStatus;
    };
    std::vector<StatusShiftUnitItem> statusShiftUnitList;
};

class InventoryCheckDetailDialog;
class InventoryCheckSimulationDialog;

/**
 * @brief Detailed modal dialog to conduct and manage an inventory check.
 * Redesigned for CHK-UNIT to feature:
 * - Master-Detail Split View (Master: Product Progress Matrix, Detail: Serial Reconciliation)
 * - Soft-Staging Architecture with In-Memory Preview & Delete Before Save (identical to Inbound)
 * - Unified Continuous Scan Bar with Out-of-Scope Rejection and Rediscovered OUT Prompt
 * - Unrecorded Strange Unit Registration with Physical Serial Tracking (Rule 5.3.1)
 * - Color-Coded Discrepancy Badges (Matched, Missing, Surplus, Status Shift)
 * - High-Performance Simulation & Hardware Test Companion Window
 * - Integrated 3-Tier Post-Completion Reconciliation Funnel
 */
class InventoryCheckDetailDialog : public QDialog
{
public:
    InventoryCheckDetailDialog(Database* database, int checkId, QWidget* parent = nullptr);
    ~InventoryCheckDetailDialog() override = default;

    bool wasModified() const { return dataModified; }
    void showToast(const QString& message, bool isSuccess = true);

    struct StagedCheckUnit {
        int checkUnitId = 0;
        int checkQuantityId = 0;
        int productId = 0;
        QString serial;
        QString productCode;
        QString productName;
        QString systemStatus;       // empty string if brand new unrecorded unit (NULL in DB)
        QString savedScannedStatus; // status saved in DB
        QString currentScannedStatus; // status staged on UI
        bool isNew = false;         // newly added in this session (can be deleted before saving)
        bool isModified = false;    // modified in memory
        QString addedTime;
        uint64_t lastTouchSeq = 0;  // action sequence counter for ordering newly touched units at the top
    };

    // Methods for Simulation Companion Dialog communication
    void onOpenSimulationToolsDialog();
    void fillSerialInput(const QString& sn);
    bool recordSingleSerialFromSim(const QString& sn, int pid, const QString& status, QString& outErr);
    void applyBatchSimulationUpdates(const std::vector<std::pair<QString, QString>>& statusUpdates, const std::vector<StagedCheckUnit>& newUnits);
    std::vector<std::pair<int, QString>> getCheckProductsList() const;
    const std::vector<StagedCheckUnit>& getStagedUnits() const { return stagedUnits; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void setupUI();
    void loadCheckInfo();
    void loadQuantityData();
    void loadUnitData();
    void refreshUnitTable();
    void refreshQuantityTablePreview();
    void updateSaveButtonState();
    bool hasPendingChanges() const;
    int getPendingNewCount() const;
    int getModifiedCount() const;

    void onSaveChanges();
    void onSaveQuantityCounts();
    void onRecordUnitStatus();
    void onAddUnrecordedSurplus();
    void onOpenReconciliationDialog();
    void onCompleteCheck();
    void onStatusComboChanged(int index);
    void onEditCheckNoteClicked();
    void onEditRowNoteClicked();

    void filterUnitsByProduct(int pid, const QString& pCode, const QString& pName);
    void applyUnitTableFilters();
    bool processUnitScan(const QString& sn, const QString& chosenMode, QLabel* feedbackLabel, QLineEdit* inputLine);
    void removeStagedUnitBySerial(const QString& sn);
    void revertStagedUnitBySerial(const QString& sn);
    void rebuildSerialIndexMap();
    bool unitMatchesCurrentFilter(const StagedCheckUnit& u) const;
    void populateTableRow(int row, int stagedIdx);
    void updateTableRowInPlace(int row, int stagedIdx);
    void insertUnitRowAtTop(int stagedIdx);
    void removeTableRowInPlace(int row, const QString& sn);
    void updateSummaryLabelsOnly();

    Database* db = nullptr;
    int checkId = 0;
    QString checkCode;
    QString checkType;
    QString checkStatus;
    QString checkDate;
    QString checkNote;
    bool dataModified = false;

    // Toast overlay
    QLabel* toastOverlayLabel = nullptr;
    QTimer* toastTimer = nullptr;

    // Header info labels
    QLabel* codeLabel = nullptr;
    QLabel* typeLabel = nullptr;
    QLabel* statusLabel = nullptr;
    QComboBox* statusCombo = nullptr;
    QLabel* dateLabel = nullptr;
    QLabel* noteLabel = nullptr;
    QPushButton* editCheckNoteBtn = nullptr;

    // Splitter for UNIT mode Master-Detail View
    QSplitter* masterDetailSplitter = nullptr;

    // Master Product Table & Controls
    QWidget* masterWidget = nullptr;
    QLineEdit* quantitySearchEdit = nullptr;
    QTableWidget* quantityTable = nullptr;
    QPushButton* saveQuantityBtn = nullptr;
    QPushButton* editRowNoteBtn = nullptr;
    QPushButton* addUnrecordedSurplusBtn = nullptr;
    QPushButton* showAllProductsBtn = nullptr;
    QLabel* quantitySummaryLabel = nullptr;

    // Detail Unit Panel & Controls
    QWidget* detailWidget = nullptr;
    QGroupBox* scanGroup = nullptr;
    QLineEdit* serialSearchInput = nullptr;
    QLabel* serialFeedbackLabel = nullptr;
    QComboBox* unitStatusCombo = nullptr;
    QPushButton* applyUnitStatusBtn = nullptr;
    QPushButton* batchTestBtn = nullptr;

    // Detail Filter Bar
    QLabel* filterScopeLabel = nullptr;
    QPushButton* clearProductFilterBtn = nullptr;
    QLineEdit* unitSearchEdit = nullptr;
    QComboBox* unitStatusFilterCombo = nullptr;
    QCheckBox* abnormalOnlyCheck = nullptr;
    QLabel* unitSummaryLabel = nullptr;

    QTableWidget* unitTable = nullptr;

    // Bottom action controls
    QLabel* pendingStatusSummaryLabel = nullptr;
    QPushButton* saveChangesBtn = nullptr;
    QPushButton* refreshBtn = nullptr;
    QPushButton* reconciliationBtn = nullptr;
    QPushButton* completeCheckBtn = nullptr;
    QPushButton* closeBtn = nullptr;

    InventoryCheckSimulationDialog* simulationDialog = nullptr;

    std::vector<StagedCheckUnit> stagedUnits;
    QHash<QString, int> serialToIndexMap;
    QHash<QString, QTableWidgetItem*> serialToTableItemMap;
    uint64_t actionCounter = 0;

    struct ProductCheckItem {
        int checkQuantityId;
        int productId;
        QString productCode;
        QString productName;
        int sysAvail;
        int sysWarr;
        int sysScrap;
        int actAvail;
        int actWarr;
        int actScrap;
        QString note;
    };
    std::vector<ProductCheckItem> checkProductItems;

    int currentFilterProductId;
};

/**
 * @brief Standalone companion window for Testing & Hardware Simulation (Rule 7.2).
 * Runs parallel/side-by-side with InventoryCheckDetailDialog:
 * - Tab 1: Candidate units picker with multi-select and batch status application.
 * - Tab 2: Single random serial generator & batch auto-simulation (100% matched, realistic discrepancy, strange unit inject).
 */
class InventoryCheckSimulationDialog : public QDialog
{
public:
    InventoryCheckSimulationDialog(
        Database* database,
        int checkId,
        const std::vector<std::pair<int, QString>>& products,
        InventoryCheckDetailDialog* parentDialog,
        QWidget* parent = nullptr
    );
    ~InventoryCheckSimulationDialog() override = default;

    void refreshCandidateList();

private:
    struct SimCandidateItem {
        int checkUnitId;
        QString serial;
        int productId;
        QString productCode;
        QString productName;
        QString systemStatus;
        QString currentStatus;
    };

    void setupUI();
    void loadCandidates();
    void filterTable();
    void onSelectAll(bool select);
    void onSelectUncheckedOnly();
    void onApplyBatchCandidates();

    // Tab 2 actions
    void onGenerateSingleSerial();
    void onFillSerialToEntry();
    void onDirectRecordGeneratedSerial();
    void onRunFullAutoSimulation();

    Database* db;
    int checkId;
    std::vector<std::pair<int, QString>> products;
    InventoryCheckDetailDialog* checkDialog;

    // Tab 1 controls
    QComboBox* productFilterCombo;
    QComboBox* statusFilterCombo;
    QLineEdit* searchEdit;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QPushButton* selectUncheckedBtn;
    QTableWidget* candidateTable;
    QLabel* selectionSummaryLabel;
    QComboBox* targetModeCombo;
    QPushButton* applyBatchBtn;

    // Tab 2 controls
    QLineEdit* generatedSerialDisplay;
    QComboBox* genProductCombo;
    QComboBox* genStatusCombo;
    QPushButton* genSingleBtn;
    QPushButton* fillToEntryBtn;
    QPushButton* recordNowBtn;

    QLabel* autoSimSummaryLabel;
    class QRadioButton* presetMatchedRadio = nullptr;
    class QRadioButton* presetRealisticRadio = nullptr;
    class QRadioButton* presetStrangeRadio = nullptr;
    QPushButton* runAutoSimBtn;

    std::vector<SimCandidateItem> candidateList;
};

/**
 * @brief Modal dialog to inspect inventory check discrepancy and details
 * during order creation or adjustment creation.
 */
class InventoryCheckInspectDialog : public QDialog
{
public:
    using ProductSelectedCallback = std::function<void(int pid, int diffAvail, int diffWarr, int diffScrap)>;

    InventoryCheckInspectDialog(Database* database, const QString& checkCodeOrId, QWidget* parent = nullptr, ProductSelectedCallback onSelect = nullptr, const QString& initialPurpose = "");
    ~InventoryCheckInspectDialog() override = default;

    static void openInspector(Database* database, const QString& checkCodeOrId, QWidget* parent = nullptr, ProductSelectedCallback onSelect = nullptr, const QString& initialPurpose = "");

    int getSelectedProductId() const { return selectedPid; }
    QString getSelectedProductCode() const { return selectedPCode; }
    QString getSelectedProductName() const { return selectedPName; }
    int getSelectedDiffAvailable() const { return selectedDiffAvail; }
    int getSelectedDiffWarranty() const { return selectedDiffWarr; }
    int getSelectedDiffScrapped() const { return selectedDiffScrap; }

private:
    void setupUI();
    void loadCheckData();
    void renderMatrixTable();
    void renderUnitTable();
    void renderSummaryTable();

    Database* db;
    QString targetCheckCode;
    ProductSelectedCallback selectCallback;
    QString initialPurposeMode;
    int checkId = 0;
    QString checkCode;
    QString checkType;
    QString checkStatus;
    QString checkedAt;
    QString note;

    QLabel* infoLabel = nullptr;
    QLineEdit* matrixSearchEdit = nullptr;
    QCheckBox* diffOnlyCheck = nullptr;
    QCheckBox* abnormalUnitsOnlyCheck = nullptr;
    QTableWidget* matrixTable = nullptr;
    QTableWidget* unitTable = nullptr;
    QTableWidget* summaryTable = nullptr;
    QComboBox* purposeModeCombo = nullptr;
    QTabWidget* tabWidget = nullptr;
    QPushButton* fillBtn = nullptr;
    QPushButton* closeBtn = nullptr;

    struct SummaryRow {
        int productId;
        QString pCode;
        QString pName;
        int qtyAvail = 0;
        int qtyWarr = 0;
        int qtyScrap = 0;
        int totalQty = 0;
        QString fromStatus;
        QString toStatus;
        QString detailNote;
    };
    std::vector<SummaryRow> summaryList;

    struct MatrixItem {
        int productId;
        QString code;
        QString name;
        int sysAvail;
        int actAvail;
        int diffAvail;
        int sysWarr;
        int actWarr;
        int diffWarr;
        int sysScrap;
        int actScrap;
        int diffScrap;
        QString note;
    };
    std::vector<MatrixItem> matrixList;

    struct UnitItem {
        QString serial;
        int productId;
        QString pCode;
        QString pName;
        QString sysStatus;
        QString actStatus;
        QString eval;
    };
    std::vector<UnitItem> unitList;

    int selectedPid = 0;
    QString selectedPCode;
    QString selectedPName;
    int selectedDiffAvail = 0;
    int selectedDiffWarr = 0;
    int selectedDiffScrap = 0;
};

#endif // INVENTORY_CHECK_DIALOG_H
