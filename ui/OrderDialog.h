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
#include "ui/TransactionHubWidget.h"

/**
 * @brief Coupled Order & Primary Logistics Creation Dialog (Rule 4.3, 4.4, 6.3.1, 7.2).
 * Supports both Inbound Orders (inbound_transaction + inbound_detail + delivery + delivery_detail)
 * and Outbound Orders (outbound_transaction + outbound_detail + shipment + shipment_detail).
 */
class OrderDialog : public QDialog
{
public:
    OrderDialog(HubType type, Database* database, QWidget* parent = nullptr);
    ~OrderDialog() override = default;

    bool didCreateAuxCustomer() const { return auxCustomerCreated; }

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

    void setupUI();
    void populateOrderTypes();
    void populateCustomers(const QString& selectCode = "");
    void populateActiveProducts();
    void onQuickAddCustomer();
    void onAddLineItem();
    void removeLineItem(int row);
    void renderLineItems();
    void onSaveOrder();

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

    std::vector<OrderLineItem> lineItems;
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

#endif // ORDER_DIALOG_H

