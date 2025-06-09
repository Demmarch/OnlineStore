#include "customerwindow.h"
#include "ui_customerwindow.h"
#include "productcard.h"
#include <QMessageBox>
#include <QDebug>
#include <QListWidgetItem>
#include <QLabel>
#include <QVBoxLayout> // Уже включен через ui, но для ясности
#include <QScrollArea>
#include "cartwindow.h"


CustomerWindow::CustomerWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CustumerWindow), // Используем имя класса из .ui файла
    m_currentUserId(-1)
{
    ui->setupUi(this);
    setWindowTitle("Каталог товаров");

    // Соединение сигналов от NetworkManager
    connect(&m_networkManager, &NetworkManager::categoriesFetched, this, &CustomerWindow::handleCategoriesFetched);
    connect(&m_networkManager, &NetworkManager::productsFetched, this, &CustomerWindow::handleProductsFetched);
    connect(&m_networkManager, &NetworkManager::cartActionCompleted, this, &CustomerWindow::handleCartActionCompleted);
    connect(&m_networkManager, &NetworkManager::cartContentsFetched, this, &CustomerWindow::handleCartContentsFetched);

    // Соединение сигнала выбора категории
    connect(ui->categoriesListWidget, &QListWidget::currentItemChanged, this, &CustomerWindow::on_categoriesListWidget_currentItemChanged);

    // Настройка действия "Просмотреть корзину" из меню
    QAction *viewCartAction = new QAction("Просмотреть корзину", this);
    viewCartAction->setObjectName("actionViewCart");
    ui->viewCartButton->addAction(viewCartAction);
    connect(viewCartAction, &QAction::triggered, this, &CustomerWindow::on_actionViewCart_triggered);
}

CustomerWindow::~CustomerWindow()
{
    clearProductLayout();
    delete ui;
}

void CustomerWindow::setCurrentUser(int userId, const QString& userRole)
{
    m_currentUserId = userId;
    m_currentUserRole = userRole;
    qDebug() << "CustomerWindow: Current user ID:" << m_currentUserId << "Role:" << m_currentUserRole;

    if (m_currentUserId != -1) {
        qDebug() << "CustomerWindow: Fetching categories for user" << m_currentUserId;
        m_networkManager.fetchCategories();
    } else {
        qDebug() << "CustomerWindow: setCurrentUser called with invalid userId.";
    }
}

void CustomerWindow::handleCategoriesFetched(bool success, const QJsonArray& categories, const QString& errorString)
{
    qDebug() << "CustomerWindow: handleCategoriesFetched, success:" << success;
    if (success) {
        populateCategories(categories);
    } else {
        QMessageBox::critical(this, "Ошибка загрузки категорий", errorString);
        qDebug() << "CustomerWindow: Failed to fetch categories:" << errorString;
    }
}

void CustomerWindow::populateCategories(const QJsonArray& categories)
{
    ui->categoriesListWidget->clear();
    if (categories.isEmpty()) {
        qDebug() << "CustomerWindow: No categories to populate.";
        // Можно добавить QListWidgetItem с сообщением "Категорий нет"
        return;
    }
    for (const QJsonValue& value : categories) {
        QJsonObject categoryObj = value.toObject();
        QListWidgetItem *item = new QListWidgetItem(categoryObj["category_name"].toString());
        item->setData(Qt::UserRole, categoryObj["category_id"].toInt());
        ui->categoriesListWidget->addItem(item);
    }
    qDebug() << "CustomerWindow: Populated" << categories.count() << "categories.";
    // Выбор первого элемента для автоматической загрузки товаров (если список не пуст)
    if (ui->categoriesListWidget->count() > 0) {
        ui->categoriesListWidget->setCurrentRow(0);
    }
}

void CustomerWindow::on_categoriesListWidget_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    m_productsRequestFinished = false;
    m_cartRequestFinished = false;
    m_currentProducts.empty();
    m_cartProductIds.clear();

    qDebug() << "CustomerWindow: Category selection changed.";
    clearProductLayout();
    QLabel* loadingLabel = new QLabel("Загрузка товаров...", this);
    loadingLabel->setAlignment(Qt::AlignCenter);
    ui->verticalLayout->addWidget(loadingLabel);

    if (!current) {
        qDebug() << "CustomerWindow: No category selected.";
        return;
    }

    int categoryId = current->data(Qt::UserRole).toInt();
    qDebug() << "CustomerWindow: Fetching products for category" << categoryId << "and cart contents for user" << m_currentUserId;
    m_networkManager.fetchProducts(categoryId);
    m_networkManager.fetchCartContents(m_currentUserId);
}

void CustomerWindow::clearProductLayout()
{
    QLayout *layout = ui->verticalLayout;
    if (!layout) return;
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (QWidget* widget = item->widget()) {
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
}

void CustomerWindow::handleProductsFetched(bool success, const QJsonArray& products, const QString& errorString)
{
    qDebug() << "CustomerWindow: Products request finished, success:" << success;
    if (success) {
        m_currentProducts = products;
    } else {
        QMessageBox::critical(this, "Ошибка загрузки товаров", errorString);
        m_currentProducts = QJsonArray(); // Очищаем в случае ошибки
    }
    m_productsRequestFinished = true; // Отмечаем, что запрос завершен
    tryRenderFilteredProducts(); // Пытаемся отрисовать
}

void CustomerWindow::onProductCardAddToCartClicked(int productId)
{
    qDebug() << "CustomerWindow: Add to cart clicked from product card, ID:" << productId << "User ID:" << m_currentUserId;
    if (m_currentUserId != -1) {
        m_networkManager.addProductToCart(m_currentUserId, productId);
    } else {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, войдите в систему для добавления товаров в корзину.");
    }
}

void CustomerWindow::handleCartActionCompleted(bool success, const QString& message, const QString& errorString)
{
    refreshProducts();
    if (success) {
        QMessageBox::information(this, "Корзина", message.isEmpty() ? "Действие с корзиной выполнено." : message);
    } else {
        QMessageBox::critical(this, "Ошибка корзины", errorString);
    }
}

void CustomerWindow::on_actionViewCart_triggered()
{
    qDebug() << "CustomerWindow: 'View Cart' action triggered. User ID:" << m_currentUserId;
    // Создаем и показываем модальное окно корзины
    CartWindow *cartWin = new CartWindow(m_currentUserId, &m_networkManager, this);
    // Соединяем сигнал от корзины, чтобы обновить каталог после покупки
    connect(cartWin, &CartWindow::purchaseCompleted, this, &CustomerWindow::refreshProducts);
    cartWin->show();
}

void CustomerWindow::refreshProducts()
{
    qDebug() << "CustomerWindow: Refreshing products view...";
    // Просто вызываем тот же слот, что и при выборе категории, чтобы перезапустить процесс
    if (ui->categoriesListWidget->currentItem()) {
        on_categoriesListWidget_currentItemChanged(ui->categoriesListWidget->currentItem(), nullptr);
    }
}

void CustomerWindow::handleCartContentsFetched(bool success, const QJsonObject& cartData, const QString& errorString)
{
    qDebug() << "CustomerWindow: Cart request finished, success:" << success;
    m_cartProductIds.clear(); // Всегда очищаем перед заполнением
    if (success) {
        QJsonArray items = cartData.value("items").toArray();
        for (const QJsonValue& val : items) {
            m_cartProductIds.insert(val.toObject()["product_id"].toInt());
        }
    } else {
        QMessageBox::critical(this, "Ошибка загрузки корзины", errorString);
    }
    m_cartRequestFinished = true; // Отмечаем, что запрос завершен
    tryRenderFilteredProducts(); // Пытаемся отрисовать
}

void CustomerWindow::tryRenderFilteredProducts()
{
    // Проверяем, завершены ли ОБА запроса
    if (!m_productsRequestFinished || !m_cartRequestFinished) {
        qDebug() << "CustomerWindow: Waiting for all data to arrive before rendering.";
        return; // Если хотя бы один запрос еще не выполнен, выходим
    }
    qDebug() << "CustomerWindow: Both requests finished. Rendering filtered products.";

    clearProductLayout(); // Очищаем сообщение "Загрузка..."

    QLayout *layout = ui->verticalLayout;
    if (!layout) {
        qCritical() << "UI layout is null!";
        return;
    }

    int visibleProductsCount = 0;
    for (const QJsonValue& value : m_currentProducts) {
        QJsonObject productObj = value.toObject();
        int productId = productObj["product_id"].toInt();

        // --- ГЛАВНАЯ ЛОГИКА ФИЛЬТРАЦИИ ---
        if (m_cartProductIds.contains(productId)) {
            continue; // Если товар есть в корзине, пропускаем его
        }
        // ------------------------------------

        visibleProductsCount++;
        ProductCard *card = new ProductCard(
            productId,
            productObj["product_name"].toString(),
            productObj["product_description"].toString(),
            productObj["product_price"].toDouble(),
            productObj["product_image_path"].toString(),
            &m_networkManager,
            ui->productsVerticalLayout // Родитель
            );
        connect(card, &ProductCard::addToCartClicked, this, &CustomerWindow::onProductCardAddToCartClicked);
        layout->addWidget(card);
    }

    if (visibleProductsCount == 0) {
        QLabel* emptyLabel = new QLabel("В данной категории нет доступных для покупки товаров.", this);
        emptyLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(emptyLabel);
    }
}
