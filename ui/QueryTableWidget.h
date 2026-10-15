#ifndef QUERY_TABLE_WIDGET_H
#define QUERY_TABLE_WIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>
#include <functional>
#include <vector>
#include <string>

#include "datastructure/vector/Vector.h"
#include "database/Record.h"

class QueryTableWidget : public QWidget
{
public:
    explicit QueryTableWidget(const QString& title, const QString& description, QWidget* parent = nullptr);

    void setColumns(const QStringList& displayHeaders, const std::vector<std::string>& columnKeys);
    void setData(const Vector<Record>& records);
    void setRefreshCallback(std::function<void()> callback);
    void addExtraControl(QWidget* widget);

    QTableWidget* getTableWidget() const { return tableWidget; }

private:
    void filterRows(const QString& query);
    void exportToCsv();
    static QString formatValue(const Value& val);

    QString titleText;
    QString descText;
    QStringList headers;
    std::vector<std::string> keys;

    QLabel* titleLabel;
    QLabel* descLabel;
    QLabel* countLabel;
    QLineEdit* searchInput;
    QPushButton* refreshBtn;
    QPushButton* exportBtn;
    QHBoxLayout* controlsLayout;
    QTableWidget* tableWidget;

    std::function<void()> onRefresh;
};

#endif // QUERY_TABLE_WIDGET_H
