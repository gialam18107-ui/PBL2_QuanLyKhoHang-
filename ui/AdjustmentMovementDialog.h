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
#include <vector>
#include <string>
#include <unordered_set>

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

    Database* db;
    QComboBox* reasonCombo;
    QComboBox* sourceCheckCombo;
    QPushButton* viewCheckBtn;
    QDateEdit* dateEdit;
    QLineEdit* noteEdit;

    // Item line inputs
    QComboBox* productCombo;
    QComboBox* fromStatusCombo;
    QComboBox* toStatusCombo;
    QSpinBox* quantitySpin;
    QPushButton* addItemBtn;
    QTableWidget* itemsTable;

    QPushButton* saveBtn;
    QPushButton* cancelBtn;

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

    Database* db;
    int prodId;
    QString prodCode;
    QString prodName;
    QString fromSt;
    QString toSt;
    int targetQty;
    std::vector<std::string> excludeList;
    std::string srcCode;

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

    Database* db;
    int adjId;
    QString adjCode;
    std::string srcCode;
    AdjustmentMovementDetailDialog* mainDlg;

    // Tab 1 controls
    QComboBox* lineFilterCombo;
    QLineEdit* searchSerialInput;
    QPushButton* selectAllBtn;
    QPushButton* deselectAllBtn;
    QTableWidget* candidatesTable;
    QLabel* selectionCountLabel;
    QPushButton* confirmCandidatesBtn;

    // Tab 2 controls
    QLabel* autoPickSummaryLabel;
    QPushButton* autoPickAllBtn;

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

    void onDirectSerialEntered();
    void onOpenProductUnitPicker(int adjustDetailId);
    void onOpenSimulationToolsDialog();
    void onRemoveStagedUnitBySerial(const QString& sn);
    void onConfirmSaveUnits();
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

    QLabel* codeLabel;
    QLabel* statusLabel;
    QLabel* reasonLabel;
    QLabel* dateLabel;
    QLabel* sourceCodeLabel;
    QLabel* noteLabel;

    QTableWidget* detailTable;
    QLabel* unitsCountLabel;
    QTableWidget* unitsTable;

    // Smart scan bar controls
    QLineEdit* scanSerialEdit;
    QPushButton* addSerialBtn;
    QPushButton* openSimulationBtn;

    QPushButton* confirmSaveUnitsBtn;
    QPushButton* closeBtn;

    AdjustmentSimulationDialog* simulationDialog = nullptr;

    std::vector<DetailLine> detailLines;
    std::vector<StagedUnit> stagedUnits;
    std::vector<ExecutedUnit> executedUnits;
};

#endif // ADJUSTMENT_MOVEMENT_DIALOG_H
