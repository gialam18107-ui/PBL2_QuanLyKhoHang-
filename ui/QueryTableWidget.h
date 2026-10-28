#ifndef QUERY_TABLE_WIDGET_H
#define QUERY_TABLE_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QStringList>
#include <functional>
#include <vector>
#include <string>

#include "datastructure/vector/Vector.h"
#include "database/Record.h"

// Rule 9.1: Column Filter Types
enum class FilterType {
    EXACT_TEXT,      // PK & Unique: Type exact value, hash lookup
    DATE_RANGE,      // Date: From date to date (AVL Tree range)
    SELECT_OR_TYPE   // Foreign keys, category specs, is_sold, type, status: Select or type
};

struct FilterColumnDef {
    QString displayName;       // Tên cột hiển thị cho người dùng
    std::string columnKey;     // Khóa cột trong CSDL / Query
    FilterType type;           // Loại lọc theo Rule 9.1
    QStringList options;       // Tùy chọn gợi ý cho SELECT_OR_TYPE
};

class QueryTableWidget : public QWidget
{
public:
    using FilterCallback = std::function<void(const std::string& columnKey, const QString& val1, const QString& val2, bool isRange)>;
    using RecordActionCallback = std::function<void(const Record&)>;

    explicit QueryTableWidget(const QString& title, const QString& description, QWidget* parent = nullptr);

    void setColumns(const QStringList& displayHeaders, const std::vector<std::string>& columnKeys);
    void setFilterColumns(const std::vector<FilterColumnDef>& filterDefs);
    void setData(const Vector<Record>& records);
    void setRefreshCallback(std::function<void()> callback);
    void setFilterCallback(FilterCallback callback);
    void addExtraControl(QWidget* widget);
    void updateRecordInPlace(int row, const std::string& fieldKey, const Value& newValue);

    // CRUD & Action callbacks (Rule 6.3, 7.1, 7.2)
    void setCrudCallbacks(
        std::function<void()> onAddCallback,
        RecordActionCallback onEditCallback,
        RecordActionCallback onDeleteCallback,
        const QString& addText = "➕ Thêm mới",
        const QString& editText = "✏️ Sửa",
        const QString& deleteText = "🗑️ Xóa"
    );
    void setCrudPermissions(bool canAdd, bool canEdit, bool canDelete);
    void setExtraAction(const QString& label, RecordActionCallback callback);
    void clearExtraAction();

    const Record* getSelectedRecord(bool warnIfNone = true);

    void resetFilter();
    QTableWidget* getTableWidget() const { return tableWidget; }

private:
    void setupFilterUI(QVBoxLayout* mainLayout);
    void setupToolbarUI(QVBoxLayout* mainLayout);
    void setupTableUI(QVBoxLayout* mainLayout);
    void onFilterColumnChanged(int index);
    void applyFilter();
    void renderRows();
    void filterRows(const QString& query);
    void exportToCsv();
    static QString formatValue(const Value& val);

    QString titleText;
    QString descText;
    QStringList headers;
    std::vector<std::string> keys;
    std::vector<FilterColumnDef> filterDefs;

    // Header info
    QLabel* titleLabel;
    QLabel* descLabel;

    // Filter bar controls (Rule 9.1)
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
    QHBoxLayout* controlsLayout;
    QLineEdit* searchInput;
    QLabel* countLabel;
    QPushButton* addBtn;
    QPushButton* editBtn;
    QPushButton* deleteBtn;
    QPushButton* extraActionBtn;
    QPushButton* refreshBtn;
    QPushButton* exportBtn;

    // Data table & Local Cache (Rule 11)
    QTableWidget* tableWidget;
    Vector<Record> cachedRecords;
    int currentSortColumn = -1;
    bool currentSortAscending = true;

    std::function<void()> onRefresh;
    FilterCallback onFilter;
    std::function<void()> onAdd;
    RecordActionCallback onEdit;
    RecordActionCallback onDelete;
    RecordActionCallback onExtraAction;
};

#endif // QUERY_TABLE_WIDGET_H
