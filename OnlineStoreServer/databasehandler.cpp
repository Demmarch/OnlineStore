#include "databasehandler.h"
#include <QDebug>
#include <QJsonValue>

DatabaseHandler::DatabaseHandler(QObject *parent) : QObject(parent)
{
    if (QSqlDatabase::isDriverAvailable("QPSQL")) {
        m_db = QSqlDatabase::addDatabase("QPSQL");
    } else {
        qWarning() << "PostgreSQL driver not available!";
    }
}

DatabaseHandler::~DatabaseHandler()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseHandler::connectToDatabase(const QString& host, int port, const QString& dbName,
                                        const QString& userName, const QString& password)
{
    if (!m_db.isValid()) {
        qWarning() << "Database object is not valid (driver not loaded?).";
        return false;
    }
    m_db.setHostName(host);
    m_db.setPort(port);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(userName);
    m_db.setPassword(password);

    if (!m_db.open()) {
        qWarning() << "Failed to connect to database:" << m_db.lastError().text();
        return false;
    }
    qDebug() << "Successfully connected to database.";
    return true;
}

QJsonArray DatabaseHandler::getCategories()
{
    QJsonArray categoriesArray;
    if (!m_db.isOpen()) {
        qWarning() << "Database is not open.";
        return categoriesArray;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT category_id, category_name, number_of_products FROM vw_CategoriesWithProductCount");

    if (query.exec()) {
        while (query.next()) {
            QJsonObject category;
            category["category_id"] = query.value("category_id").toInt();
            category["category_name"] = query.value("category_name").toString();
            category["number_of_products"] = query.value("number_of_products").toInt();
            categoriesArray.append(category);
        }
    } else {
        qWarning() << "Failed to get categories:" << query.lastError().text();
    }
    return categoriesArray;
}

QJsonArray DatabaseHandler::getProductsByCategory(int categoryId)
{
    QJsonArray productsArray;
    if (!m_db.isOpen()) {
        qWarning() << "Database is not open.";
        return productsArray;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT product_id, product_name, product_price, product_description, product_image_path "
                  "FROM fn_GetProductsByCategory(:categoryId)");
    query.bindValue(":categoryId", categoryId);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject product;
            product["product_id"] = query.value("product_id").toInt();
            product["product_name"] = query.value("product_name").toString();
            product["product_price"] = query.value("product_price").toDouble();
            product["product_description"] = query.value("product_description").toString();
            product["product_image_path"] = query.value("product_image_path").toString();
            productsArray.append(product);
        }
    } else {
        qWarning() << "Failed to get products by category:" << query.lastError().text();
    }
    return productsArray;
}

bool DatabaseHandler::addProduct(const QJsonObject& productData)
{
    if (!m_db.isOpen()) {
        qWarning() << "DatabaseHandler: Database is not open for addProduct.";
        return false;
    }

    // === НАЧАЛО ТРАНЗАКЦИИ ===
    if (!m_db.transaction()) {
        qWarning() << "DatabaseHandler: Failed to start transaction for addProduct:" << m_db.lastError().text();
        return false;
    }

    bool success = false; // Флаг общего успеха операции

    // 1. Извлечение данных из JSON (как и было)
    QString name = productData.value("product_name").toString();
    double price = productData.value("product_price").toDouble();
    QString description = productData.value("product_description").toString();
    QString imagePath = productData.value("product_image_path").toString();
    QJsonArray categoryIdsJson = productData.value("category_ids").toArray();

    if (name.isEmpty() || price <= 0) {
        qWarning() << "DatabaseHandler: Invalid product data (name or price).";
        m_db.rollback();
        return false;
    }

    // 2. Вставка товара в таблицу Products и получение ID
    QSqlQuery productInsertQuery(m_db);
    productInsertQuery.prepare("INSERT INTO Products (product_name, product_price, product_description, product_image_path) "
                               "VALUES (:name, :price, :description, :imagePath) "
                               "RETURNING product_id");
    productInsertQuery.bindValue(":name", name);
    productInsertQuery.bindValue(":price", price);
    productInsertQuery.bindValue(":description", description);
    productInsertQuery.bindValue(":imagePath", imagePath);
    int newProductId = -1;
    if (productInsertQuery.exec()) {
        if (productInsertQuery.next()) {
            newProductId = productInsertQuery.value(0).toInt();
            qDebug() << "DatabaseHandler: Product inserted with ID:" << newProductId;
        } else {
            qWarning() << "DatabaseHandler: Failed to retrieve new product_id after insert.";
        }
    } else {
        qWarning() << "DatabaseHandler: Failed to insert into Products table. Error:" << productInsertQuery.lastError().text();
    }

    if (newProductId == -1) {
        m_db.rollback();
        return false; // Не удалось вставить товар или получить ID
    }

    // 3. Привязка товара к категориям в Products_Categories
    bool allCategoryLinksSuccessful = true;
    if (!categoryIdsJson.isEmpty()) {
        QSqlQuery ProductCategoryInsertQuery(m_db);
        // Сначала проверим существование категорий (как в процедуре)
        for (const QJsonValue& val : categoryIdsJson) {
            int categoryId = val.toInt();
            if (categoryId <= 0) continue; // Пропускаем невалидные ID

            // Проверка существования категории
            QSqlQuery checkCategoryQuery(m_db);
            checkCategoryQuery.prepare("SELECT 1 FROM Categories WHERE category_id = :catId");
            checkCategoryQuery.bindValue(":catId", categoryId);
            if (!checkCategoryQuery.exec() || !checkCategoryQuery.next()) {
                qWarning() << "DatabaseHandler: Category with ID" << categoryId << "not found. Skipping link for product ID" << newProductId;
                continue; // Пропускаем эту категорию, но не прерываем всю операцию (согласно RAISE WARNING в процедуре)
            }

            // Категория существует, пытаемся вставить связь
            ProductCategoryInsertQuery.prepare("INSERT INTO Products_Categories (product_id, category_id) "
                                               "VALUES (:productId, :categoryId) "
                                               "ON CONFLICT (product_id, category_id) DO NOTHING");
            ProductCategoryInsertQuery.bindValue(":productId", newProductId);
            ProductCategoryInsertQuery.bindValue(":categoryId", categoryId);

            if (!ProductCategoryInsertQuery.exec()) {
                qWarning() << "DatabaseHandler: Failed to insert into Products_Categories for product_id"
                           << newProductId << "and category_id" << categoryId
                           << ". Error:" << ProductCategoryInsertQuery.lastError().text();
                allCategoryLinksSuccessful = false; // Отмечаем, что не все связи успешны
                // В оригинальной процедуре это был RAISE WARNING, операция не откатывалась полностью из-за этого.
                // Если нужна строгая атомарность (все или ничего), здесь можно сделать break и откатить.
                // Пока что продолжаем, как в процедуре (предупреждение и продолжение).
            } else {
                qDebug() << "DatabaseHandler: Linked product" << newProductId << "to category" << categoryId;
            }
        }
    }
    // В данном случае, даже если не все связи с категориями удались (были WARNINGS),
    // сам товар добавлен. Логика процедуры не предполагала откат из-за отсутствия категории.
    success = true;


    if (success) {
        if (!m_db.commit()) {
            qWarning() << "DatabaseHandler: Failed to commit transaction for addProduct:" << m_db.lastError().text();
            m_db.rollback(); // Попытка отката, если commit не удался
            return false;
        }
        qDebug() << "DatabaseHandler: Product added successfully (ID:" << newProductId << ") and transaction committed.";
        return true;
    } else {
        qWarning() << "DatabaseHandler: addProduct failed, rolling back transaction.";
        m_db.rollback();
        return false;
    }
}

bool DatabaseHandler::addToCart(int userId, int productId)
{
    if (!m_db.isOpen()) {
        qWarning() << "DatabaseHandler: Database is not open.";
        return false;
    }

    QSqlQuery checkUserQuery(m_db);
    checkUserQuery.prepare("SELECT 1 FROM Users WHERE user_id = :userId");
    checkUserQuery.bindValue(":userId", userId);
    if (!checkUserQuery.exec()) {
        qWarning() << "DatabaseHandler: addToCart - Failed to check user existence. Error:" << checkUserQuery.lastError().text();
        return false;
    }
    if (!checkUserQuery.next()) { // Пользователь не найден
        qWarning() << "DatabaseHandler: addToCart - User with ID" << userId << "not found.";
        return false;
    }

    QSqlQuery checkProductQuery(m_db);
    checkProductQuery.prepare("SELECT 1 FROM Products WHERE product_id = :productId");
    checkProductQuery.bindValue(":productId", productId);
    if (!checkProductQuery.exec()) {
        qWarning() << "DatabaseHandler: addToCart - Failed to check product existence. Error:" << checkProductQuery.lastError().text();
        return false;
    }
    if (!checkProductQuery.next()) { // Товар не найден
        qWarning() << "DatabaseHandler: addToCart - Product with ID" << productId << "not found or no longer available.";
        return false;
    }

    // 3. Вставка в корзину
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO Cart (user_id, product_id) VALUES (:userId, :productId) "
                        "ON CONFLICT (user_id, product_id) DO NOTHING");
    insertQuery.bindValue(":userId", userId);
    insertQuery.bindValue(":productId", productId);

    qDebug() << "DatabaseHandler: Attempting to add to cart (direct SQL). UserID:" << userId << "ProductID:" << productId;
    qDebug() << "DatabaseHandler: Executing insert query:" << insertQuery.lastQuery(); // Показывает запрос с плейсхолдерами

    if (insertQuery.exec()) {
        qDebug() << "DatabaseHandler: INSERT INTO Cart executed for UserID:" << userId << "ProductID:" << productId
                 << ". Rows affected:" << insertQuery.numRowsAffected();
        return true;
    } else {
        qWarning() << "DatabaseHandler: Failed to add to cart (direct SQL insert). Error:" << insertQuery.lastError().text()
        << "Native Error Code:" << insertQuery.lastError().nativeErrorCode()
        << "Prepared Query:" << insertQuery.lastQuery();
        return false;
    }
}

bool DatabaseHandler::placeOrder(int userId)
{
    if (!m_db.isOpen()) {
        qWarning() << "DatabaseHandler: Database is not open for placeOrder.";
        return false;
    }

    // === НАЧАЛО ТРАНЗАКЦИИ (Обязательно) ===
    if (!m_db.transaction()) {
        qWarning() << "DatabaseHandler: Failed to start transaction for placeOrder:" << m_db.lastError().text();
        return false;
    }

    // 1. Получаем ID товаров в корзине
    QSqlQuery getCartQuery(m_db);
    getCartQuery.prepare("SELECT product_id FROM Cart WHERE user_id = :userId");
    getCartQuery.bindValue(":userId", userId);

    if (!getCartQuery.exec()) {
        qWarning() << "DatabaseHandler: Failed to query cart for placeOrder. Error:" << getCartQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    QList<int> productIdsInCart;
    while (getCartQuery.next()) {
        productIdsInCart.append(getCartQuery.value(0).toInt());
    }

    if (productIdsInCart.isEmpty()) {
        qDebug() << "DatabaseHandler: Cart is empty for user" << userId << ". Order cannot be placed.";
        m_db.commit(); // Завершаем "пустую" транзакцию
        return true;
    }

    qDebug() << "DatabaseHandler: Placing order for user" << userId << ". Deleting products:" << productIdsInCart;

    // 2. Удаление каждого товара из таблицы Products
    QSqlQuery deleteProductQuery(m_db);
    for (int productId : productIdsInCart) {
        deleteProductQuery.prepare("DELETE FROM Products WHERE product_id = :productId");
        deleteProductQuery.bindValue(":productId", productId);
        if (!deleteProductQuery.exec()) {
            qWarning() << "DatabaseHandler: Failed to delete product ID" << productId << ". Error:" << deleteProductQuery.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    // 3. Очистка корзины пользователя (это может быть избыточно, если настроено каскадное удаление, но для надежности оставляем)
    QSqlQuery clearCartQuery(m_db);
    clearCartQuery.prepare("DELETE FROM Cart WHERE user_id = :userId");
    clearCartQuery.bindValue(":userId", userId);
    if (!clearCartQuery.exec()) {
        qWarning() << "DatabaseHandler: Failed to clear cart for user ID" << userId << ". Error:" << clearCartQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // === ЗАВЕРШЕНИЕ ТРАНЗАКЦИИ ===
    if (!m_db.commit()) {
        qWarning() << "DatabaseHandler: Failed to commit transaction for placeOrder:" << m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    qDebug() << "DatabaseHandler: Order placed successfully for user" << userId;
    return true;
}

QJsonObject DatabaseHandler::authenticateUser(const QString& login, const QString& password)
{
    QJsonObject userData;
    if (!m_db.isOpen()) {
        qWarning() << "Database is not open.";
        return userData;
    }

    QSqlQuery query(m_db);
    // Вызываем функцию fn_AuthenticateUser
    query.prepare("SELECT user_id, user_role FROM fn_AuthenticateUser(:login, :password)");
    query.bindValue(":login", login);
    query.bindValue(":password", password);

    if (query.exec()) {
        if (query.next()) { // Если пользователь найден и пароль верный
            userData["user_id"] = query.value("user_id").toInt();
            userData["user_role"] = query.value("user_role").toString();
        } else {
            qDebug() << "Authentication failed for user:" << login;
        }
    } else {
        qWarning() << "Authentication query failed:" << query.lastError().text();
    }
    return userData;
}

bool DatabaseHandler::addCategory(const QString& categoryName)
{
    if (categoryName.isEmpty()) return false;
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO Categories (category_name) VALUES (:name) ON CONFLICT (category_name) DO NOTHING");
    query.bindValue(":name", categoryName);
    if (!query.exec()) {
        qWarning() << "DatabaseHandler: Failed to add category. Error:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonObject DatabaseHandler::getCartContents(int userId)
{
    QJsonObject result;
    QJsonArray itemsArray;
    double totalPrice = 0.0;

    if (!m_db.isOpen()) {
        qWarning() << "DatabaseHandler: Database is not open for getCartContents.";
        result["error"] = "Database connection failed.";
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT p.product_id, p.product_name, p.product_price, p.product_image_path "
                  "FROM Cart c JOIN Products p ON c.product_id = p.product_id "
                  "WHERE c.user_id = :userId");
    query.bindValue(":userId", userId);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject item;
            item["product_id"] = query.value("product_id").toInt();
            item["product_name"] = query.value("product_name").toString();
            double price = query.value("product_price").toDouble();
            item["product_price"] = price;
            item["product_image_path"] = query.value("product_image_path").toString();
            itemsArray.append(item);
            totalPrice += price;
        }
        result["items"] = itemsArray;
        result["total_price"] = totalPrice;
    } else {
        qWarning() << "DatabaseHandler: Failed to get cart contents. Error:" << query.lastError().text();
        result["error"] = "Failed to query cart contents.";
    }
    return result;
}

bool DatabaseHandler::removeFromCart(int userId, int productId)
{
    if (!m_db.isOpen()) {
        qWarning() << "DatabaseHandler: Database is not open for removeFromCart.";
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM Cart WHERE user_id = :userId AND product_id = :productId");
    query.bindValue(":userId", userId);
    query.bindValue(":productId", productId);

    if (query.exec()) {
        qDebug() << "DatabaseHandler: removeFromCart executed for UserID:" << userId << "ProductID:" << productId
                 << ". Rows affected:" << query.numRowsAffected();
        return true;
    } else {
        qWarning() << "DatabaseHandler: Failed to remove from cart. Error:" << query.lastError().text();
        return false;
    }
}

bool DatabaseHandler::deleteProduct(int productId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM Products WHERE product_id = :id");
    query.bindValue(":id", productId);
    if (!query.exec()) {
        qWarning() << "DatabaseHandler: Failed to delete product. Error:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool DatabaseHandler::deleteCategory(int categoryId)
{
    // Требование: каскадное удаление товаров, которые находятся ТОЛЬКО в этой категории
    if (!m_db.transaction()) {
        qWarning() << "DatabaseHandler: Failed to start transaction for deleteCategory.";
        return false;
    }

    // 1. Находим продукты, которые состоят только в удаляемой категории
    QSqlQuery findProductsQuery(m_db);
    findProductsQuery.prepare(
        "SELECT p.product_id FROM Products p "
        "JOIN Products_Categories pc ON p.product_id = pc.product_id "
        "LEFT JOIN Products_Categories pc2 ON p.product_id = pc2.product_id AND pc2.category_id != :cat_id "
        "WHERE pc.category_id = :cat_id "
        "GROUP BY p.product_id "
        "HAVING COUNT(pc2.category_id) = 0"
        );
    findProductsQuery.bindValue(":cat_id", categoryId);
    if (!findProductsQuery.exec()) {
        qWarning() << "DatabaseHandler: Failed to find unique products for category. Error:" << findProductsQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    QList<int> productsToDelete;
    while (findProductsQuery.next()) {
        productsToDelete.append(findProductsQuery.value(0).toInt());
    }

    // 2. Удаляем эти продукты
    if (!productsToDelete.isEmpty()) {
        for (int prodId : productsToDelete) {
            if (!deleteProduct(prodId)) { // Используем уже созданный метод
                qWarning() << "DatabaseHandler: Failed during cascade delete of product" << prodId;
                m_db.rollback();
                return false;
            }
        }
    }

    // 3. Удаляем саму категорию (связи в Products_Categories удалятся каскадно благодаря FK)
    QSqlQuery deleteCatQuery(m_db);
    deleteCatQuery.prepare("DELETE FROM Categories WHERE category_id = :id");
    deleteCatQuery.bindValue(":id", categoryId);
    if (!deleteCatQuery.exec()) {
        qWarning() << "DatabaseHandler: Failed to delete category itself. Error:" << deleteCatQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    return m_db.commit();
}


bool DatabaseHandler::updateProductField(int productId, const QString& fieldName, const QVariant& value)
{
    // Внимание: динамическое формирование имени столбца - потенциально небезопасно.
    // Убедимся, что fieldName - одно из разрешенных полей.
    const QSet<QString> allowedFields = {"product_name", "product_description", "product_price", "product_image_path"};
    if (!allowedFields.contains(fieldName)) {
        qWarning() << "DatabaseHandler: Attempt to update a non-allowed field:" << fieldName;
        return false;
    }

    QSqlQuery query(m_db);
    // Формируем запрос с плейсхолдером для имени столбца
    query.prepare(QString("UPDATE Products SET %1 = :value WHERE product_id = :id").arg(fieldName));
    query.bindValue(":value", value);
    query.bindValue(":id", productId);

    if (!query.exec()) {
        qWarning() << "DatabaseHandler: Failed to update product field" << fieldName << ". Error:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseHandler::changeProductCategory(int productId, int oldCategoryId, int newCategoryId)
{
    if (oldCategoryId == newCategoryId) return true; // Категория не изменилась

    if (!m_db.transaction()) {
        qWarning() << "DatabaseHandler: Failed to start transaction for changeProductCategory.";
        return false;
    }

    // 1. Удаляем старую связь
    QSqlQuery deleteQuery(m_db);
    deleteQuery.prepare("DELETE FROM Products_Categories WHERE product_id = :prodId AND category_id = :oldCatId");
    deleteQuery.bindValue(":prodId", productId);
    deleteQuery.bindValue(":oldCatId", oldCategoryId);

    if (!deleteQuery.exec()) {
        qWarning() << "changeProductCategory: Failed to delete old category link. Error:" << deleteQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    // 2. Добавляем новую связь (с проверкой на существование, если нужно)
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO Products_Categories (product_id, category_id) VALUES (:prodId, :newCatId) "
                        "ON CONFLICT (product_id, category_id) DO NOTHING");
    insertQuery.bindValue(":prodId", productId);
    insertQuery.bindValue(":newCatId", newCategoryId);

    if (!insertQuery.exec()) {
        qWarning() << "changeProductCategory: Failed to insert new category link. Error:" << insertQuery.lastError().text();
        m_db.rollback();
        return false;
    }

    qDebug() << "DatabaseHandler: Changed category for product" << productId << "from" << oldCategoryId << "to" << newCategoryId;
    return m_db.commit();
}
