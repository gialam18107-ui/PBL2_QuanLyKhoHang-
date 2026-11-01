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
#include <vector>
#include <string>

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
    void populateProducts();
    void populateReasons();
    void populateChecks();
    void onAddItem();
    void onRemoveItem(int row);
    void renderItemsTable();
    void onSaveMovement();

    Database* db;
    QComboBox* reasonCombo;
    QComboBox* sourceCheckCombo;
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
 * @brief Modal dialog to view adjustment details and execute state movement.
 */
class AdjustmentMovementDetailDialog : public QDialog
{
public:
    AdjustmentMovementDetailDialog(Database* database, int adjustId, QWidget* parent = nullptr);
    ~AdjustmentMovementDetailDialog() override = default;

    bool wasExecuted() const { return isExecuted; }

private:
    void setupUI();
    void loadMovementInfo();
    void loadMovementDetails();
    void onExecuteMovement();

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
    QPushButton* executeBtn;
    QPushButton* closeBtn;
};

#endif // ADJUSTMENT_MOVEMENT_DIALOG_H
