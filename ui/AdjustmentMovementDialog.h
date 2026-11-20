#ifndef ADJUSTMENT_MOVEMENT_DIALOG_H
#define ADJUSTMENT_MOVEMENT_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCheckBox>
#include <QTabWidget>
#include <QMenu>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>

#include "database/Database.h"
#include "schema/UserCreate.h"
#include "schema/UserUpdate.h"
#include "schema/UserQuery.h"

/**
 * @brief Modal dialog to create an Adjustment Movement with upfront detail declaration.
 */
class AdjustmentMovementCreateDialog : public QDialog
{
public:
    explicit AdjustmentMovementCreateDialog(Database* database, QWidget* parent = nullptr);
    ~AdjustmentMovementCreateDialog() override = default;

    QString getCreatedAdjustCode() const { return createdCode; }
    int getCreatedAdjustId() const { return createdId; }

private:
    void setupUI();
    void populateProducts(const QString& filterCheckCode = "");
    void populateReasons();
    void populateChecks();
    void onReasonChanged();
    void onCheckChanged();
    void onAddItem();
    void onRemoveItem(int row);
    void renderItemsTable();
    void onSaveMovement();

    Database* db = nullptr;
    QComboBox* reasonCombo = nullptr;
    QComboBox* sourceCheckCombo = nullptr;
    QPushButton* viewCheckBtn = nullptr;
    QDateEdit* dateEdit = nullptr;
    QLineEdit* noteEdit = nullptr;

    // Item line inputs
    QComboBox* productCombo = nullptr;
    QComboBox* fromStatusCombo = nullptr;
    QComboBox* toStatusCombo = nullptr;
    QSpinBox* quantitySpin = nullptr;
    QPushButton* addItemBtn = nullptr;
    QTableWidget* itemsTable = nullptr;

    QPushButton* saveBtn = nullptr;
    QPushButton* cancelBtn = nullptr;

    struct StagedItem {
        int productId;
        QString productCode;
        QString productName;
        QString fromStatus;
        QString toStatus;
        int quantity;
    };
    std::vector<StagedItem> stagedItems;

    QString createdCode;
    int createdId;
};

/**
 * @brief Modal dialog to pick candidate units from warehouse inventory for a specific adjustment detail line.
 */
class AdjustmentProductUnitPickerDialog : public QDialog
{
public:
    AdjustmentProductUnitPickerDialog(
        Database* database,
        int productId,
        const QString& productCode,
        const QString& productName,
        const QString& fromStatus,
        const QString& toStatus,
        int targetQuantity = 0,
        const std::vector<std::string>& excludedSerials = {},
        const std::string& sourceCode = "",
        QWidget* parent = nullptr
    );
    ~AdjustmentProductUnitPickerDialog() override = default;

    std::vector<std::string> getSelectedSerials() const { return selectedSerials; }

private:
    struct CandidateRow {
        int unitId;
        std::string serial;
        std::string status;
        bool isFromCheck = false;
    };

    void setupUI();
    void loadCandidates();
    void filterTable();
    void updateSelectionCount();

    Database* db = nullptr;
    int prodId = 0;
    QString prodCode;
    QString prodName;
    QString fromSt;
    QString toSt;
    int targetQty = 0;
    std::vector<std::string> excludeList;
    std::string srcCode;

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

class AdjustmentMovementDetailDialog;

/**
 * @brief Companion simulation & hardware test window for Adjustment Movement.
 */
class AdjustmentSimulationDialog : public QDialog
{
public:
    AdjustmentSimulationDialog(
        Database* database,
        int adjustId,
        const QString& adjustCode,
        const std::string& sourceCode,
        AdjustmentMovementDetailDialog* detailDlg,
        QWidget* parent = nullptr
    );
    ~AdjustmentSimulationDialog() override = default;

    void refreshData();

private:
    struct LineCandidate {
        int unitId;
        std::string serial;
        std::string status;
        bool isFromCheck;
    };

    void setupUI();
    void loadCandidatesForSelectedLine();
    void filterCandidates();
    void onSelectionChanged();
    void onSelectAll();
    void onDeselectAll();
    void onConfirmCandidates();
    void onExecuteAutoPickAll();

    Database* db = nullptr;
    int adjId = 0;
    QString adjCode;
    std::string srcCode;
    AdjustmentMovementDetailDialog* mainDlg = nullptr;

    // Tab 1 controls
    QComboBox* lineFilterCombo = nullptr;
    QLineEdit* searchSerialInput = nullptr;
    QPushButton* selectAllBtn = nullptr;
    QPushButton* deselectAllBtn = nullptr;
    QTableWidget* candidatesTable = nullptr;
    QLabel* selectionCountLabel = nullptr;
    QPushButton* confirmCandidatesBtn = nullptr;

    // Tab 2 controls
    QLabel* autoPickSummaryLabel = nullptr;
    QPushButton* autoPickAllBtn = nullptr;

    std::vector<LineCandidate> currentCandidates;
};

/**
 * @brief Modal dialog to view adjustment details and execute state movement with live progress and unit-level auditing.
 */
class AdjustmentMovementDetailDialog : public QDialog
{
public:
    AdjustmentMovementDetailDialog(Database* database, int adjustId, QWidget* parent = nullptr);
    ~AdjustmentMovementDetailDialog() override = default;

    bool wasExecuted() const { return isExecuted; }

    struct DetailLine {
        int adjustDetailId;
        int productId;
        QString productCode;
        QString productName;
        QString fromStatus;
        QString toStatus;
        int adjustQuantity;
        int actualQuantity;
    };

    struct StagedUnit {
        int adjustDetailId;
        int productId;
        QString productCode;
        QString productName;
        QString serial;
        QString fromStatus;
        QString toStatus;
    };

    struct ExecutedUnit {
        int unitId;
        int productId;
        QString productCode;
        QString productName;
        QString serial;
        QString fromStatus;
        QString toStatus;
    };

    const std::vector<DetailLine>& getDetailLines() const { return detailLines; }
    int getSelectedDetailId() const;
    int getStagedCountForDetail(int detailId) const;
    bool isSerialStaged(const std::string& sn) const;
    bool isSerialExecuted(const std::string& sn) const;
    void stageUnitsBatch(int detailId, const std::vector<std::string>& serials);
    void autoPickAllAccordingToPlan();

private:
    void setupUI();
    void loadMovementInfo();
    void loadMovementDetails();
    void loadExecutedUnits();

    void refreshAllViews(bool scrollToBottom = false);
    void refreshDetailTable();
    void refreshUnitsTable(bool scrollToBottom = false);
    void applyDetailTableFilter();
    void applyUnitsTableFilter();

    void onDirectSerialEntered();
    void onDetailSelectionChanged();
    void onCancelMovement();
    void onStatusComboChanged(int index);
    void onOpenProductUnitPicker(int adjustDetailId);
    void onOpenSimulationToolsDialog();
    void onRemoveStagedUnitBySerial(const QString& sn);
    void onConfirmSaveUnits();
    void onFinalizeMovement();
    void preloadUnitsFromCheck();

    Database* db;
    int adjustId;
    QString adjustCode;
    QString adjustStatus;
    QString adjustReason;
    QString adjustedAt;
    QString sourceCode;
    QString note;
    bool isExecuted;

    QLabel* codeLabel = nullptr;
    QComboBox* statusCombo = nullptr;
    QLabel* reasonLabel = nullptr;
    QLabel* dateLabel = nullptr;
    QLabel* sourceCodeLabel = nullptr;
    QLabel* noteLabel = nullptr;

    QTableWidget* detailTable = nullptr;
    QLineEdit* detailSearchEdit = nullptr;
    QLabel* unitsCountLabel = nullptr;
    QTableWidget* unitsTable = nullptr;
    QLineEdit* unitsSearchEdit = nullptr;

    // Smart scan bar controls
    QLabel* selectedPlanLabel = nullptr;
    QLineEdit* scanSerialEdit = nullptr;
    QPushButton* addSerialBtn = nullptr;
    QPushButton* openSimulationBtn = nullptr;

    QPushButton* confirmSaveUnitsBtn = nullptr;
    QPushButton* finalizeMovementBtn = nullptr;
    QPushButton* closeBtn = nullptr;

    AdjustmentSimulationDialog* simulationDialog = nullptr;

    void syncStagedCache();
    std::unordered_set<std::string> stagedSerialSet;
    std::unordered_set<std::string> executedSerialSet;
    std::unordered_map<int, int> detailStagedCountMap;

    std::vector<DetailLine> detailLines;
    std::vector<StagedUnit> stagedUnits;
    std::vector<ExecutedUnit> executedUnits;
};

#endif // ADJUSTMENT_MOVEMENT_DIALOG_H
