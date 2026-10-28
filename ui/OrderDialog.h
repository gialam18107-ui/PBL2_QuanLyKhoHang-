#ifndef ORDER_DIALOG_H
#define ORDER_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>
#include <QLabel>
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
    };

    struct MultiBatchDef {
        QString expectedDate;
        std::vector<int> allocatedQuantities; // Corresponding to lineItems
    };

    void setupUI();
    void populateOrderTypes();
    void populateCustomers(const QString& selectCode = "");
    void populateActiveProducts();
    void onQuickAddCustomer();
    void onAddLineItem();
    void removeLineItem(int row);
    void renderLineItems();
    void onSaveOrder();

    // Multi-batch schedule helpers
    void setupMultiBatchUI(QVBoxLayout* parentLayout);
    void onToggleMultiBatch(bool checked);
    void onAddScheduleBatch();
    void onRemoveScheduleBatch();
    void renderMultiBatchTable();

    HubType hubType;
    Database* db;
    bool auxCustomerCreated;

    QComboBox* typeCombo;
    QComboBox* customerCombo;
    QPushButton* addCustomerBtn;
    QDateEdit* orderDateEdit;
    QDateEdit* expectedDateEdit;
    QLineEdit* noteEdit;

    QComboBox* productCombo;
    QSpinBox* quantitySpin;
    QPushButton* addItemBtn;
    QTableWidget* itemsTable;
    QLabel* itemsSummaryLabel;

    // Multi-batch logistics allocation controls (Rule 4.3)
    QCheckBox* multiBatchCheck;
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

private:
    void onSave();

    HubType hubType;
    Database* db;
    int txnId;
    QComboBox* statusCombo;
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

private:
    void onSave();

    HubType hubType;
    Database* db;
    int logId;
    QString orderDateStr;
    QComboBox* statusCombo;
    QCheckBox* hasActualDateCheck;
    QDateEdit* actualDateEdit;
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
    ~UnitEntryDialog() override = default;

    bool didModifyUnits() const { return hasModifiedUnits; }

private:
    struct LogisticsItemInfo {
        int productId;
        QString productCode;
        QString productName;
        int receivedQty;
        int actualQty;
        int remainingQty;
    };

    struct UnitRowInfo {
        int unitId;
        std::string serial;
        std::string status;
        int productId;
        bool isEligible;
        QString reasonText;
    };

    void setupUI();
    void loadLogisticsItems();
    void onProductChanged(int index);
    void onAutoGenerateSerial();
    void onExecuteUnitOperation();
    void loadExistingUnitsForCurrentProduct();
    void filterUnitsTable();
    void onUnitTableSelectionChanged();

    bool isUnitEligibleForCurrentOp(const std::string& status) const;
    QString getEligibilityReason(const std::string& status) const;

    HubType hubType;
    Database* db;
    int logId;
    QString batchCodeStr;
    QString parentOrderCodeStr;
    std::string transactionType;

    QLabel* ruleTipLabel;
    QComboBox* productCombo;
    QLabel* progressLabel;
    QComboBox* targetStatusCombo;
    QLabel* targetStatusFixedLabel;
    QSpinBox* quantitySpin;
    QLineEdit* serialInput;
    QPushButton* autoGenSerialBtn;
    QPushButton* executeBtn;

    // Table & filter controls
    QLineEdit* serialSearchInput;
    QCheckBox* filterEligibleCheck;
    QLabel* selectionSummaryLabel;
    QTableWidget* existingUnitsTable;

    std::vector<LogisticsItemInfo> items;
    std::vector<UnitRowInfo> cachedUnits;
    bool hasModifiedUnits = false;
};

#endif // ORDER_DIALOG_H
