#include "networkmanager.h"
#include <QDebug>
#include <QUrlQuery>

NetworkManager::NetworkManager(QObject *parent) : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
}

void NetworkManager::setBaseUrl(const QString &baseUrl)
{
    m_baseUrl = baseUrl;
    if (m_baseUrl.endsWith('/')) {
        m_baseUrl.chop(1);
    }
}

void NetworkManager::handleJsonResponse(QNetworkReply* reply,
                                        std::function<void(bool, const QJsonDocument&, const QString&)> callback)
{
    QString errorStr;
    bool success = false;
    QJsonDocument jsonDoc;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonParseError parseError;
        jsonDoc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            // Дополнительная проверка, что ответ не пустой, если ожидается объект или массив
            if (jsonDoc.isObject() || jsonDoc.isArray()) {
                success = true;
            } else if (data.trimmed().isEmpty() && (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 204) ) { // No Content
                success = true; // Успех, но нет JSON тела
            }
            else {
                errorStr = "JSON Parse Error: Expected object or array, got: " + QString::fromUtf8(data.left(100));
            }
        } else {
            errorStr = "JSON Parse Error: " + parseError.errorString() + "\nData: " + QString::fromUtf8(data.left(100));
        }
    } else {
        errorStr = reply->errorString() + " | Response: " + QString::fromUtf8(reply->readAll().left(100));
    }

    callback(success, jsonDoc, errorStr);
    reply->deleteLater();
}


void NetworkManager::login(const QString &username, const QString &password)
{
    QJsonObject json;
    json["login"] = username;
    json["password"] = password;

    QNetworkRequest request(QUrl(m_baseUrl + "/login"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleJsonResponse(reply, [this](bool success, const QJsonDocument& doc, const QString& errorStr) {
            if (success && doc.isObject()) {
                emit loginCompleted(true, doc.object());
            } else {
                emit loginCompleted(false, QJsonObject(), errorStr.isEmpty() ? "Login failed or invalid server response" : errorStr);
            }
        });
    });
}

void NetworkManager::fetchCategories()
{
    QNetworkRequest request(QUrl(m_baseUrl + "/categories"));
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleJsonResponse(reply, [this](bool success, const QJsonDocument& doc, const QString& errorStr) {
            if (success && doc.isArray()) {
                emit categoriesFetched(true, doc.array());
            } else {
                emit categoriesFetched(false, QJsonArray(), errorStr.isEmpty() ? "Failed to fetch categories" : errorStr);
            }
        });
    });
}

void NetworkManager::fetchProducts(int categoryId)
{
    QUrl url(m_baseUrl + "/products");
    QUrlQuery query;
    query.addQueryItem("category_id", QString::number(categoryId));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleJsonResponse(reply, [this](bool success, const QJsonDocument& doc, const QString& errorStr) {
            if (success && doc.isArray()) {
                emit productsFetched(true, doc.array());
            } else {
                emit productsFetched(false, QJsonArray(), errorStr.isEmpty() ? "Failed to fetch products" : errorStr);
            }
        });
    });
}

QNetworkReply* NetworkManager::fetchImage(const QString& imageUrl)
{
    QUrl url;
    if (imageUrl.startsWith("http://", Qt::CaseInsensitive) || imageUrl.startsWith("https://", Qt::CaseInsensitive)) {
        url = QUrl(imageUrl);
    } else {
        url.setScheme("http");
        url.setHost("localhost");
        url.setPort(8080);
        url.setPath(imageUrl);
    }
    qDebug() << "NetworkManager: Fetching image from" << url.toString();
    return m_nam->get(QNetworkRequest(url));
}

void NetworkManager::addProductToCart(int userId, int productId)
{
    QJsonObject json;
    json["user_id"] = userId;
    json["product_id"] = productId;

    QNetworkRequest request(QUrl(m_baseUrl + "/cart"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString errorMsg;
        bool success = false;
        QString successMessage;

        if (reply->error() == QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode >= 200 && statusCode < 300) { // 200 OK, 201 Created, etc.
                success = true;
                successMessage = QString::fromUtf8(reply->readAll());
                if (successMessage.isEmpty()) successMessage = "Товар успешно добавлен в корзину!";
            } else {
                errorMsg = "Ошибка сервера: " + QString::number(statusCode) + " " +
                           reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString() +
                           " | " + QString::fromUtf8(reply->readAll().left(200));
            }
        } else {
            errorMsg = reply->errorString() + " | " + QString::fromUtf8(reply->readAll().left(200));
        }
        emit cartActionCompleted(success, successMessage, errorMsg);
        reply->deleteLater();
    });
}

void NetworkManager::fetchCartContents(int userId)
{
    QUrl url(m_baseUrl + "/cart");
    QUrlQuery query;
    query.addQueryItem("user_id", QString::number(userId));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleJsonResponse(reply, [this](bool success, const QJsonDocument& doc, const QString& errorStr) {
            if (success && doc.isObject()) {
                emit cartContentsFetched(true, doc.object());
            } else {
                emit cartContentsFetched(false, QJsonObject(), errorStr.isEmpty() ? "Failed to fetch cart" : errorStr);
            }
        });
    });
}

void NetworkManager::removeFromCart(int userId, int productId)
{
    QUrl url(m_baseUrl + "/cart");
    QUrlQuery query;
    query.addQueryItem("user_id", QString::number(userId));
    query.addQueryItem("product_id", QString::number(productId));
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_nam->deleteResource(request); // Используем deleteResource для метода DELETE

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // Обработка текстового ответа или просто кода состояния 200 OK
        QString errorMsg;
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);

        if (success) {
            emit productRemovedFromCart(true, "Товар удален из корзины", "");
        } else {
            errorMsg = reply->errorString();
            emit productRemovedFromCart(false, "", errorMsg);
        }
        reply->deleteLater();
    });
}

void NetworkManager::placeOrder(int userId)
{
    // 1. Создаем JSON-тело запроса
    QJsonObject json;
    json["user_id"] = userId;

    // 2. Создаем и настраиваем запрос
    QNetworkRequest request(QUrl(m_baseUrl + "/order"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 3. Отправляем POST-запрос
    QNetworkReply *reply = m_nam->post(request, QJsonDocument(json).toJson());

    // 4. Соединяем сигнал о завершении запроса с обработчиком
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString errorMsg;
        bool success = false;
        QString successMessage;

        // Проверяем на наличие сетевых ошибок
        if (reply->error() == QNetworkReply::NoError) {
            // Сетевой ошибки нет, проверяем HTTP статус-код ответа
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode >= 200 && statusCode < 300) { // Успешные статусы (200 OK, 201 Created, и т.д.)
                success = true;
                successMessage = QString::fromUtf8(reply->readAll());
                if (successMessage.isEmpty()) {
                    successMessage = "Заказ успешно оформлен!";
                }
            } else {
                // Сервер вернул ошибку (например, 400, 404, 500)
                errorMsg = "Ошибка сервера: " + QString::number(statusCode) + " " +
                           reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString() +
                           " | " + QString::fromUtf8(reply->readAll().left(200)); // Читаем тело ошибки
            }
        } else {
            // Произошла сетевая ошибка (например, нет соединения с сервером)
            errorMsg = reply->errorString() + " | " + QString::fromUtf8(reply->readAll().left(200));
        }

        // 5. Испускаем сигнал с результатом операции
        emit orderPlaced(success, successMessage, errorMsg);

        // 6. Очищаем память
        reply->deleteLater();
    });
}

void NetworkManager::addCategory(const QString& categoryName)
{
    QJsonObject json;
    json["category_name"] = categoryName;

    QNetworkRequest request(QUrl(m_baseUrl + "/categories"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, categoryName]() {
        QString errorMsg;
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 201); // 201 Created

        if (!success) errorMsg = reply->errorString();
        emit adminActionCompleted(success, "add_category", "Категория '" + categoryName + "' создана.", errorMsg);
        reply->deleteLater();
    });
}

void NetworkManager::deleteCategory(int categoryId)
{
    QNetworkRequest request(QUrl(m_baseUrl + "/categories/" + QString::number(categoryId)));
    QNetworkReply *reply = m_nam->deleteResource(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, categoryId]() {
        QString errorMsg;
        // 204 No Content - стандартный успешный ответ на DELETE
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 204);

        if (!success) errorMsg = reply->errorString();
        emit adminActionCompleted(success, "delete_category", "Категория (ID: " + QString::number(categoryId) + ") удалена.", errorMsg);
        reply->deleteLater();
    });
}

void NetworkManager::addProduct(const QJsonObject& productData)
{
    QNetworkRequest request(QUrl(m_baseUrl + "/products"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(productData).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QString errorMsg;
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 201); // 201 Created

        if (!success) errorMsg = reply->errorString();
        emit adminActionCompleted(success, "add_product", "Новый товар успешно создан.", errorMsg);
        reply->deleteLater();
    });
}

void NetworkManager::deleteProduct(int productId)
{
    QNetworkRequest request(QUrl(m_baseUrl + "/products/" + QString::number(productId)));
    QNetworkReply *reply = m_nam->deleteResource(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, productId]() {
        QString errorMsg;
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 204); // 204 No Content

        if (!success) errorMsg = reply->errorString();
        emit adminActionCompleted(success, "delete_product", "Товар (ID: " + QString::number(productId) + ") удален.", errorMsg);
        reply->deleteLater();
    });
}

void NetworkManager::updateProductField(int productId, const QString& fieldName, const QVariant& value)
{
    QJsonObject json;
    json[fieldName] = QJsonValue::fromVariant(value);

    QNetworkRequest request(QUrl(m_baseUrl + "/products/" + QString::number(productId)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Для метода PATCH используем sendCustomRequest
    QNetworkReply *reply = m_nam->sendCustomRequest(request, "PATCH", QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, productId, fieldName]() {
        QString errorMsg;
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);

        if (!success) errorMsg = reply->errorString();
        emit adminActionCompleted(success, "update_product", "Поле '" + fieldName + "' товара (ID: " + QString::number(productId) + ") обновлено.", errorMsg);
        reply->deleteLater();
    });
}

QNetworkReply* NetworkManager::uploadImage(const QByteArray& imageData, const QString& fileName)
{
    Q_UNUSED(fileName); // fileName может пригодиться для Content-Disposition, но пока не используем

    QNetworkRequest request(QUrl(m_baseUrl + "/upload/image"));
    // Устанавливаем тип контента, чтобы сервер знал, что это изображение.
    // Можно использовать "application/octet-stream" для любых бинарных данных.
    request.setHeader(QNetworkRequest::ContentTypeHeader, "image/jpeg");

    // Отправляем сырые байты изображения в теле POST-запроса
    // Ответ (QNetworkReply) возвращается напрямую, чтобы вызывающий код мог
    // подключиться к его сигналу finished() и получить путь к загруженному файлу.
    return m_nam->post(request, imageData);
}

void NetworkManager::changeProductCategory(int productId, int oldCategoryId, int newCategoryId)
{
    QJsonObject json;
    json["old_category_id"] = oldCategoryId;
    json["new_category_id"] = newCategoryId;

    QString url = m_baseUrl + "/products/" + QString::number(productId) + "/category_link";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_nam->sendCustomRequest(request, "PATCH", QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, productId]() {
        QString errorMsg;
        bool success = (reply->error() == QNetworkReply::NoError &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200);

        if (!success) errorMsg = reply->errorString();
        emit adminActionCompleted(success, "update_product_category", "Категория товара (ID: " + QString::number(productId) + ") обновлена.", errorMsg);
        reply->deleteLater();
    });
}
