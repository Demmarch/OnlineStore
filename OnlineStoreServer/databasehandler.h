#ifndef DATABASEHANDLER_H
#define DATABASEHANDLER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>

class DatabaseHandler : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseHandler(QObject *parent = nullptr);
    ~DatabaseHandler();

    bool connectToDatabase(const QString& host, int port, const QString& dbName,
                           const QString& userName, const QString& password);

    // Методы для всех ролей
    QJsonArray getCategories();
    QJsonArray getProductsByCategory(int categoryId);
    QJsonObject authenticateUser(const QString& login, const QString& password);

    // Методы для корзины
    QJsonObject getCartContents(int userId);
    bool addToCart(int userId, int productId);
    bool removeFromCart(int userId, int productId);
    bool placeOrder(int userId);

    // Методы для администратора
    bool addCategory(const QString& categoryName);
    bool deleteCategory(int categoryId);
    bool addProduct(const QJsonObject& productData);
    bool deleteProduct(int productId);
    bool updateProductField(int productId, const QString& fieldName, const QVariant& value);
    bool changeProductCategory(int productId, int oldCategoryId, int newCategoryId);

private:
    QSqlDatabase m_db;
};

#endif // DATABASEHANDLER_H
