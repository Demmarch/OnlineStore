#include "httpserver.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery> // Для request.query()
#include <QDebug>

HttpServer::HttpServer(DatabaseHandler* dbHandler, QObject *parent)
    : QObject(parent),
    m_httpServer(this),
    m_tcpServer(this),
    m_dbHandler(dbHandler)
{
    if (!m_dbHandler) {
        qFatal("HttpServer: DatabaseHandler instance is required!");
    }
    setupRoutes();
}

HttpServer::~HttpServer()
{
    if (m_tcpServer.isListening()) {
        m_tcpServer.close();
    }
}

bool HttpServer::startServer(quint16 port)
{
    if (!m_tcpServer.listen(QHostAddress::Any, port)) { // Сначала запуск tcpServer, после bind
        qWarning() << "HttpServer: QTcpServer failed to listen on port" << port
                   << ". Error:" << m_tcpServer.errorString();
        return false;
    }
    qDebug() << "HttpServer: QTcpServer is now listening on port" << m_tcpServer.serverPort();
    if (!m_httpServer.bind(&m_tcpServer)) {
        qWarning() << "HttpServer: Failed to bind QHttpServer to QTcpServer instance (even though QTcpServer was listening).";
        m_tcpServer.close();
        return false;
    }

    qDebug() << "HttpServer: Successfully bound QHttpServer to QTcpServer. Server is running on port" << m_tcpServer.serverPort();
    return true;
}

void HttpServer::setupRoutes()
{
    // === Общие маршруты ===
    m_httpServer.route("/login", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &req){ return handleLogin(req); });
    m_httpServer.route("/categories", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &req){ return handleGetCategories(req); });
    m_httpServer.route("/products", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &req){ return handleGetProducts(req); });
    m_httpServer.route("/images/<arg>", QHttpServerRequest::Method::Get, [this](const QString &fileName) {
        return handleServeStaticFile(fileName);
    });

    // === Маршруты для корзины ===
    m_httpServer.route("/cart", QHttpServerRequest::Method::Get, [this](const QHttpServerRequest &req){ return handleGetCart(req); });
    m_httpServer.route("/cart", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &req){ return handlePostCart(req); });
    m_httpServer.route("/cart", QHttpServerRequest::Method::Delete, [this](const QHttpServerRequest &req){ return handleRemoveFromCart(req); });
    m_httpServer.route("/order", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &req){ return handlePostOrder(req); });

    // === Маршруты для администратора ===
    m_httpServer.route("/categories", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &req){ return handlePostCategory(req); });
    m_httpServer.route("/categories/<arg>", QHttpServerRequest::Method::Delete, [this](int categoryId, const QHttpServerRequest &req){
        Q_UNUSED(req);
        return handleDeleteCategory(categoryId);
    });
    m_httpServer.route("/products", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &req){ return handlePostProducts(req); });
    m_httpServer.route("/products/<arg>", QHttpServerRequest::Method::Delete, [this](int productId, const QHttpServerRequest &req){
        Q_UNUSED(req);
        return handleDeleteProduct(productId);
    });
    m_httpServer.route("/products/<arg>", QHttpServerRequest::Method::Patch, [this](int productId, const QHttpServerRequest &req){
        return handleUpdateProduct(productId, req);
    });
    m_httpServer.route("/upload/image", QHttpServerRequest::Method::Post, [this](const QHttpServerRequest &req){ return handleImageUpload(req); });
    m_httpServer.route("/products/<arg>/category_link", QHttpServerRequest::Method::Patch, [this](int productId, const QHttpServerRequest &req){
        return handleChangeProductCategory(productId, req);
    });
}

// --- Реализации обработчиков маршрутов ---

QHttpServerResponse HttpServer::handleLogin(const QHttpServerRequest &request)
{
    if (request.method() != QHttpServerRequest::Method::Post) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::MethodNotAllowed);
    }

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(request.body(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        return QHttpServerResponse("Bad Request: Invalid JSON body for login. " + parseError.errorString(),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonObject loginData = jsonDoc.object();
    QString login = loginData.value("login").toString();
    QString password = loginData.value("password").toString();

    if (login.isEmpty() || password.isEmpty()) {
        return QHttpServerResponse("Bad Request: Missing login or password.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonObject authResult = m_dbHandler->authenticateUser(login, password);
    if (!authResult.isEmpty()) {
        return QHttpServerResponse(authResult, QHttpServerResponse::StatusCode::Ok);
    } else {
        return QHttpServerResponse("Unauthorized: Invalid credentials",
                                   QHttpServerResponse::StatusCode::Unauthorized);
    }
}

QHttpServerResponse HttpServer::handleGetCategories(const QHttpServerRequest &request)
{
    if (request.method() != QHttpServerRequest::Method::Get) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::MethodNotAllowed);
    }
    QJsonArray categories = m_dbHandler->getCategories();
    return QHttpServerResponse(categories, QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse HttpServer::handleGetProducts(const QHttpServerRequest &request)
{
    if (request.method() != QHttpServerRequest::Method::Get) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::MethodNotAllowed);
    }
    const QUrlQuery queryParams = request.query();
    if (queryParams.hasQueryItem("category_id")) {
        bool ok;
        int categoryId = queryParams.queryItemValue("category_id").toInt(&ok);
        if (ok) {
            QJsonArray products = m_dbHandler->getProductsByCategory(categoryId);
            return QHttpServerResponse(products, QHttpServerResponse::StatusCode::Ok);
        } else {
            return QHttpServerResponse("Bad Request: Invalid category_id",
                                       QHttpServerResponse::StatusCode::BadRequest);
        }
    } else {
        return QHttpServerResponse("Bad Request: category_id is required",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }
}

QHttpServerResponse HttpServer::handlePostProducts(const QHttpServerRequest &request)
{
    if (request.method() != QHttpServerRequest::Method::Post) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::MethodNotAllowed);
    }
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(request.body(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        return QHttpServerResponse("Bad Request: Invalid JSON body. " + parseError.errorString(),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    if (m_dbHandler->addProduct(jsonDoc.object())) {
        return QHttpServerResponse("Product Created", QHttpServerResponse::StatusCode::Created);
    } else {
        return QHttpServerResponse("Internal Server Error: Could not add product",
                                   QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse HttpServer::handlePostCart(const QHttpServerRequest &request)
{
    if (request.method() != QHttpServerRequest::Method::Post) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::MethodNotAllowed);
    }
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(request.body(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        return QHttpServerResponse("Bad Request: Invalid JSON body.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject cartData = jsonDoc.object();
    if (!cartData.contains("user_id") || !cartData.contains("product_id")) {
        return QHttpServerResponse("Bad Request: Missing user_id or product_id.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }
    int userId = cartData.value("user_id").toInt();
    int productId = cartData.value("product_id").toInt();

    if (userId <= 0 || productId <= 0) {
        return QHttpServerResponse("Bad Request: Invalid user_id or product_id.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    if (m_dbHandler->addToCart(userId, productId)) {
        return QHttpServerResponse("Product added to cart", QHttpServerResponse::StatusCode::Ok);
    } else {
        return QHttpServerResponse("Internal Server Error: Could not add to cart",
                                   QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse HttpServer::handlePostOrder(const QHttpServerRequest &request)
{
    if (request.method() != QHttpServerRequest::Method::Post) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::MethodNotAllowed);
    }
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(request.body(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        return QHttpServerResponse("Bad Request: Invalid JSON body.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }
    QJsonObject orderData = jsonDoc.object();
    if (!orderData.contains("user_id")) {
        return QHttpServerResponse("Bad Request: Missing user_id.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }
    int userId = orderData.value("user_id").toInt();
    if (userId <= 0) {
        return QHttpServerResponse("Bad Request: Invalid user_id.",
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    if (m_dbHandler->placeOrder(userId)) {
        return QHttpServerResponse("Order placed", QHttpServerResponse::StatusCode::Ok);
    } else {
        return QHttpServerResponse("Internal Server Error or Order cannot be placed (e.g. empty cart)",
                                   QHttpServerResponse::StatusCode::InternalServerError);
    }
}
QHttpServerResponse HttpServer::handleGetCart(const QHttpServerRequest &request)
{
    if (!request.query().hasQueryItem("user_id")) {
        return QHttpServerResponse("Bad Request: Missing user_id", QHttpServerResponse::StatusCode::BadRequest);
    }
    bool ok;
    int userId = request.query().queryItemValue("user_id").toInt(&ok);
    if (!ok || userId <= 0) {
        return QHttpServerResponse("Bad Request: Invalid user_id", QHttpServerResponse::StatusCode::BadRequest);
    }

    QJsonObject cartData = m_dbHandler->getCartContents(userId);
    if (cartData.contains("error")) {
        return QHttpServerResponse("Internal Server Error", QHttpServerResponse::StatusCode::InternalServerError);
    }
    return QHttpServerResponse(cartData, QHttpServerResponse::StatusCode::Ok);
}

QHttpServerResponse HttpServer::handleRemoveFromCart(const QHttpServerRequest &request)
{
    const QUrlQuery params = request.query();
    if (!params.hasQueryItem("user_id") || !params.hasQueryItem("product_id")) {
        return QHttpServerResponse("Bad Request: Missing user_id or product_id", QHttpServerResponse::StatusCode::BadRequest);
    }
    bool userIdOk, productIdOk;
    int userId = params.queryItemValue("user_id").toInt(&userIdOk);
    int productId = params.queryItemValue("product_id").toInt(&productIdOk);

    if (!userIdOk || !productIdOk || userId <= 0 || productId <= 0) {
        return QHttpServerResponse("Bad Request: Invalid user_id or product_id", QHttpServerResponse::StatusCode::BadRequest);
    }

    if (m_dbHandler->removeFromCart(userId, productId)) {
        return QHttpServerResponse("Product removed from cart", QHttpServerResponse::StatusCode::Ok);
    } else {
        return QHttpServerResponse("Internal Server Error", QHttpServerResponse::StatusCode::InternalServerError);
    }
}

QHttpServerResponse HttpServer::handlePostCategory(const QHttpServerRequest &request)
{
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(request.body(), &error);
    if (error.error || !json.isObject())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

    QString categoryName = json.object().value("category_name").toString();
    if (categoryName.isEmpty())
        return QHttpServerResponse("Bad Request: category_name is empty", QHttpServerResponse::StatusCode::BadRequest);

    if (m_dbHandler->addCategory(categoryName))
        return QHttpServerResponse("Category created", QHttpServerResponse::StatusCode::Created);
    else
        return QHttpServerResponse("Internal Server Error", QHttpServerResponse::StatusCode::InternalServerError);
}

QHttpServerResponse HttpServer::handleDeleteCategory(int categoryId)
{
    if (m_dbHandler->deleteCategory(categoryId))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NoContent);
    else
        return QHttpServerResponse("Internal Server Error", QHttpServerResponse::StatusCode::InternalServerError);
}

QHttpServerResponse HttpServer::handleDeleteProduct(int productId)
{
    if (m_dbHandler->deleteProduct(productId))
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NoContent);
    else
        return QHttpServerResponse("Internal Server Error", QHttpServerResponse::StatusCode::InternalServerError);
}

QHttpServerResponse HttpServer::handleUpdateProduct(int productId, const QHttpServerRequest &request)
{
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(request.body(), &error);
    if (error.error || !json.isObject())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

    QJsonObject jsonObj = json.object();
    if (jsonObj.keys().size() != 1)
        return QHttpServerResponse("Bad Request: PATCH should contain exactly one field to update.", QHttpServerResponse::StatusCode::BadRequest);

    QString fieldName = jsonObj.keys().first();
    QVariant value = jsonObj.value(fieldName).toVariant();

    if (m_dbHandler->updateProductField(productId, fieldName, value))
        return QHttpServerResponse("Product updated", QHttpServerResponse::StatusCode::Ok);
    else
        return QHttpServerResponse("Internal Server Error or Invalid Field", QHttpServerResponse::StatusCode::InternalServerError);
}

QHttpServerResponse HttpServer::handleImageUpload(const QHttpServerRequest &request)
{
    // Создаем директорию, если ее нет. Директория будет создана там, где запущен сервер.
    QDir dir("images");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Генерируем уникальное имя файла на основе UUID, чтобы избежать коллизий.
    // Расширение можно определять по Content-Type, но для простоты предположим, что это .jpg
    QString fileName = QUuid::createUuid().toString(QUuid::WithoutBraces) + ".jpg";
    QString filePath = dir.filePath(fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(request.body()); // Пишем сырые байты из тела запроса в файл
        file.close();

        QJsonObject response;
        response["status"] = "success";
        // Возвращаем клиенту относительный путь, который он будет использовать для сохранения в БД
        response["image_path"] = "/images/" + fileName;
        return QHttpServerResponse(response);
    }

    return QHttpServerResponse("Failed to save image", QHttpServerResponse::StatusCode::InternalServerError);
}

QHttpServerResponse HttpServer::handleChangeProductCategory(int productId, const QHttpServerRequest &request)
{
    QJsonParseError error;
    const auto json = QJsonDocument::fromJson(request.body(), &error);
    if (error.error || !json.isObject())
        return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

    QJsonObject jsonObj = json.object();
    if (!jsonObj.contains("old_category_id") || !jsonObj.contains("new_category_id"))
        return QHttpServerResponse("Bad Request: Missing old or new category id.", QHttpServerResponse::StatusCode::BadRequest);

    int oldCatId = jsonObj["old_category_id"].toInt();
    int newCatId = jsonObj["new_category_id"].toInt();

    if (m_dbHandler->changeProductCategory(productId, oldCatId, newCatId))
        return QHttpServerResponse("Product category updated", QHttpServerResponse::StatusCode::Ok);
    else
        return QHttpServerResponse("Internal Server Error", QHttpServerResponse::StatusCode::InternalServerError);
}

QHttpServerResponse HttpServer::handleServeStaticFile(const QString &fileName)
{
    if (fileName.contains("..")) {
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
    }

    QString filePath = "images/" + fileName;

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "HttpServer: Static file not found at" << QDir::current().absoluteFilePath(filePath);
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    }

    QByteArray fileData = file.readAll();
    file.close();

    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    qDebug() << "HttpServer: Serving file" << filePath << "with MIME type" << mimeType.name();

    // ============ КОРРЕКТНЫЙ КОД ============
    // 1. Создаем ответ, используя конструктор с телом и статус-кодом.
    QHttpServerResponse response(fileData, QHttpServerResponse::StatusCode::Ok);

    // 2. Получаем объект QHttpHeaders и используем метод setValue() для установки заголовка.
    response.headers().append(QHttpHeaders::WellKnownHeader::ContentType, mimeType.name().toUtf8());
    // ==========================================

    return response;
}
