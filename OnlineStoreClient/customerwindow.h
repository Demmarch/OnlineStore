#ifndef CUSTOMERWINDOW_H
#define CUSTOMERWINDOW_H

#include <QMainWindow>
#include <QSet>
#include "networkmanager.h"
#include "productcard.h"

QT_BEGIN_NAMESPACE
class QListWidgetItem;
namespace Ui { class CustumerWindow; }
QT_END_NAMESPACE

class CustomerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CustomerWindow(QWidget *parent = nullptr);
    ~CustomerWindow();

    void setCurrentUser(int userId);

private slots:
    // Слоты для NetworkManager
    void handleCategoriesFetched(bool success, const QJsonArray& categories, const QString& errorString);
    void handleProductsFetched(bool success, const QJsonArray& products, const QString& errorString);
    void handleCartActionCompleted(bool success, const QString& message, const QString& errorString);
    void handleCartContentsFetched(bool success, const QJsonObject& cartData, const QString& errorString);

    // Слоты для UI
    void on_categoriesListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_actionViewCart_triggered();
    void onProductCardAddToCartClicked(int productId);
    void refreshProducts();

private:
    Ui::CustumerWindow *ui; // Указатель на UI форму CustumerWindow
    NetworkManager m_networkManager;
    int m_currentUserId;
    QString m_currentUserRole;

    QJsonArray m_currentProducts;      // Хранит ответ от /products
    QSet<int>  m_cartProductIds;       // Хранит ID товаров в корзине
    bool       m_productsRequestFinished = false; // Флаг завершения запроса товаров
    bool       m_cartRequestFinished = false;     // Флаг завершения запроса корзины

    // Вспомогательные методы
    void populateCategories(const QJsonArray& categories);
    void clearProductLayout(); // Очистка карточек товаров
    void tryRenderFilteredProducts();
};

#endif // CUSTOMERWINDOW_H
