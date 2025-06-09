#ifndef ADMINWINDOW_H
#define ADMINWINDOW_H

#include <QMainWindow>
#include <QJsonArray>

#include "networkmanager.h"

QT_BEGIN_NAMESPACE
class QListWidgetItem;
namespace Ui { class AdminWindow; }
QT_END_NAMESPACE

class AdminWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminWindow(QWidget *parent = nullptr);
    ~AdminWindow();

    void setAdminId(int adminId);

private slots:
    // Слоты для UI кнопок
    void on_btnCreateCategory_clicked();
    void on_btnDeleteCategory_clicked();
    void on_btnCreateProduct_clicked();
    void on_listWidgetCategories_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);

    // Слоты для NetworkManager
    void handleCategoriesFetched(bool success, const QJsonArray& categories, const QString& errorString);
    void handleProductsFetched(bool success, const QJsonArray& products, const QString& errorString);
    void handleAdminAction(bool success, const QString& action, const QString& message, const QString& errorString);

private:
    Ui::AdminWindow *ui;
    NetworkManager m_networkManager;
    int m_adminId;
    int m_selectedCategoryId = -1;
    QJsonArray m_currentCategories; // Для хранения списка категорий

    void refreshCategories();
    void refreshProducts();
    void populateCategories(const QJsonArray& categories);
    void populateProducts(const QJsonArray& products);
    void clearProductLayout();
};

#endif // ADMINWINDOW_H
