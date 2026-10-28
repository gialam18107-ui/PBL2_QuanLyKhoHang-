#ifndef CUSTOMER_DIALOG_H
#define CUSTOMER_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>

#include "database/Database.h"

/**
 * @brief Dedicated dialog for creating Individual / Business customers
 * and updating customer contact details while protecting immutable fields (Rule 4.1, 6.3, 6.3.1, 7.2).
 */
class CustomerDialog : public QDialog
{
public:
    enum class CustomerKind {
        INDIVIDUAL = 0,
        BUSINESS = 1
    };

    /// Create Mode constructor
    CustomerDialog(Database* database, CustomerKind initialKind, bool allowSwitchKind = true, QWidget* parent = nullptr);

    /// Edit Mode constructor (loads existing customer + individual_info / business_info)
    CustomerDialog(Database* database, int editCustomerId, QWidget* parent = nullptr);

    ~CustomerDialog() override = default;

    QString getCreatedCustomerCode() const { return createdCustomerCode; }

private:
    void setupUI(bool allowSwitchKind);
    void onKindChanged(int index);
    void updatePreviewCode();
    void loadExistingCustomer();
    void onSaveClicked();

    Database* db;
    bool isEditMode;
    int customerId;
    QString createdCustomerCode;

    QComboBox* kindCombo;
    QLineEdit* codePreviewEdit;
    QLabel* nameFieldLabel;
    QLineEdit* nameEdit;
    QLabel* taxFieldLabel;
    QLineEdit* taxCodeEdit;
    QLineEdit* phoneEdit;
    QLineEdit* emailEdit;
    QPushButton* saveBtn;
};

#endif // CUSTOMER_DIALOG_H

