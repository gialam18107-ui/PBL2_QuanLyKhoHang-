#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>

#include "database/Database.h"
#include "QueryTableWidget.h"

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(Database* database, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    void setupUI();
    void setupSidebar();
    void setupPages();
    void refreshQuery(int index);
    void reloadDatabase();

    Database* db;

    QListWidget* sidebarList;
    QStackedWidget* stackedWidget;
    QLabel* statusLabel;
    QPushButton* reloadDbBtn;

    // 10 Query Table Widgets
    QueryTableWidget* productWidget;
    QueryTableWidget* productQuantityWidget;
    QueryTableWidget* unitInventoryWidget;
    QueryTableWidget* statusHistoryWidget;
    QueryTableWidget* inboundTxnWidget;
    QueryTableWidget* inboundDetailWidget;
    QueryTableWidget* deliveryDetailWidget;
    QueryTableWidget* outboundTxnWidget;
    QueryTableWidget* outboundDetailWidget;
    QueryTableWidget* shipmentDetailWidget;

    // Extra filters
    bool currentProductIsSold;
};

#endif // MAIN_WINDOW_H
