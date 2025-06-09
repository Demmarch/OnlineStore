#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <functional> // Для std::function

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);

    void setBaseUrl(const QString& baseUrl);

    // --- Методы для пользователя ---
    void login(const QString& username, const QString& password);
    void fetchCategories();
    void fetchProducts(int categoryId);
    void fetchCartContents(int userId);
    void addProductToCart(int userId, int productId);
    void removeFromCart(int userId, int productId);
    void placeOrder(int userId);
    QJsonObject getCartContents(int userId);
    QNetworkReply* fetchImage(const QString& imageUrl);

    // --- Методы для администратора ---
    void addCategory(const QString& categoryName);
    void deleteCategory(int categoryId);
    void addProduct(const QJsonObject& productData);
    void deleteProduct(int productId);
    void updateProductField(int productId, const QString& fieldName, const QVariant& value);
    QNetworkReply* uploadImage(const QByteArray& imageData, const QString& fileName); // Добавлен fileName для Content-Disposition
    void changeProductCategory(int productId, int oldCategoryId, int newCategoryId);


signals:
    // --- Сигналы для пользователя ---
    void loginCompleted(bool success, const QJsonObject& userData, const QString& errorString = "");
    void categoriesFetched(bool success, const QJsonArray& categories, const QString& errorString = "");
    void productsFetched(bool success, const QJsonArray& products, const QString& errorString = "");
    void cartContentsFetched(bool success, const QJsonObject& cartData, const QString& errorString = "");
    void cartActionCompleted(bool success, const QString& message, const QString& errorString = "");
    void productRemovedFromCart(bool success, const QString& message, const QString& errorString = "");
    void orderPlaced(bool success, const QString& message, const QString& errorString = "");

    // --- Общий сигнал для админ-действий ---
    void adminActionCompleted(bool success, const QString& action, const QString& message, const QString& errorString = "");


private:
    void handleJsonResponse(QNetworkReply* reply,
                            std::function<void(bool, const QJsonDocument&, const QString&)> callback);

    QNetworkAccessManager *m_nam;
    QString m_baseUrl = "http://localhost:8080"; // Сервер по умолчанию
};

#endif // NETWORKMANAGER_H
