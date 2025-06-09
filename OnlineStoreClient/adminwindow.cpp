#include "adminwindow.h"
#include "ui_adminwindow.h"
#include "productcardinadminpanel.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QBuffer>
#include <QListWidgetItem>
#include <QVBoxLayout>

AdminWindow::AdminWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AdminWindow)
{
    ui->setupUi(this);
    setWindowTitle("Панель администратора");

    if (!ui->scrollAreaWidgetContents->layout()) {
        ui->scrollAreaWidgetContents->setLayout(new QVBoxLayout());
    }

    connect(&m_networkManager, &NetworkManager::categoriesFetched, this, &AdminWindow::handleCategoriesFetched);
    connect(&m_networkManager, &NetworkManager::productsFetched, this, &AdminWindow::handleProductsFetched);
    connect(&m_networkManager, &NetworkManager::adminActionCompleted, this, &AdminWindow::handleAdminAction);
}

AdminWindow::~AdminWindow()
{
    delete ui;
}

void AdminWindow::setAdminId(int adminId)
{
    m_adminId = adminId;
    refreshCategories();
}

void AdminWindow::refreshCategories()
{
    m_networkManager.fetchCategories();
}

void AdminWindow::refreshProducts()
{
    if (m_selectedCategoryId != -1) {
        m_networkManager.fetchProducts(m_selectedCategoryId);
    } else {
        clearProductLayout();
    }
}

void AdminWindow::handleCategoriesFetched(bool success, const QJsonArray& categories, const QString& errorString)
{
    if (!success) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить категории: " + errorString);
        return;
    }
    m_currentCategories = categories; // Сохраняем для использования в других диалогах
    populateCategories(categories);
}

void AdminWindow::populateCategories(const QJsonArray& categories)
{
    ui->listWidgetCategories->clear();
    for (const QJsonValue& value : categories) {
        QJsonObject cat = value.toObject();
        QListWidgetItem* item = new QListWidgetItem(cat["category_name"].toString());
        item->setData(Qt::UserRole, cat["category_id"].toInt());
        ui->listWidgetCategories->addItem(item);
    }
    if (ui->listWidgetCategories->count() > 0) {
        ui->listWidgetCategories->setCurrentRow(0); // Автоматически выбираем первую категорию
    }
}

void AdminWindow::on_listWidgetCategories_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);
    if (!current) {
        m_selectedCategoryId = -1;
        ui->productsInCatelabel->setText("Товары в категории");
        clearProductLayout();
        return;
    }
    m_selectedCategoryId = current->data(Qt::UserRole).toInt();
    ui->productsInCatelabel->setText("Товары в категории: " + current->text());
    refreshProducts();
}

void AdminWindow::clearProductLayout()
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

void AdminWindow::handleProductsFetched(bool success, const QJsonArray& products, const QString& errorString)
{
    clearProductLayout();
    if (!success) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить товары: " + errorString);
        return;
    }
    populateProducts(products);
}

void AdminWindow::populateProducts(const QJsonArray& products)
{
    // Метод handleProductsFetched вызывает этот метод.
    // Сначала очищаем старые карточки.
    clearProductLayout();

    if (products.isEmpty()) {
        QLabel* emptyLabel = new QLabel("В этой категории нет товаров.", ui->scrollAreaWidgetContents);
        emptyLabel->setAlignment(Qt::AlignCenter);
        ui->verticalLayout->addWidget(emptyLabel);
        return;
    }

    for (const QJsonValue& val : products) {
        ProductCardInAdminPanel* card = new ProductCardInAdminPanel(
            val.toObject(),
            m_currentCategories,
            m_selectedCategoryId,
            &m_networkManager,
            ui->scrollAreaWidgetContents
            );

        connect(card, &ProductCardInAdminPanel::productDataChanged, this, &AdminWindow::refreshProducts);
        ui->verticalLayout->addWidget(card);
    }
}

void AdminWindow::on_btnCreateCategory_clicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "Создать категорию", "Название новой категории:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        m_networkManager.addCategory(name);
    }
}

void AdminWindow::on_btnDeleteCategory_clicked()
{
    QListWidgetItem* currentItem = ui->listWidgetCategories->currentItem();
    if (!currentItem) {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, выберите категорию для удаления.");
        return;
    }
    int categoryId = currentItem->data(Qt::UserRole).toInt();
    auto reply = QMessageBox::question(this, "Удаление категории",
                                       "Вы уверены? Удаление категории приведет к удалению всех товаров, которые находятся ТОЛЬКО в этой категории.",
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_networkManager.deleteCategory(categoryId);
    }
}

void AdminWindow::on_btnCreateProduct_clicked()
{
    if (m_selectedCategoryId == -1) {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, выберите категорию для нового товара.");
        return;
    }

    // Последовательные диалоги для сбора данных
    bool ok;
    QString name = QInputDialog::getText(this, "Новый товар", "Название:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    double price = QInputDialog::getDouble(this, "Новый товар", "Цена:", 0, 0.01, 10000000, 2, &ok);
    if (!ok) return;

    QString description = QInputDialog::getMultiLineText(this, "Новый товар", "Описание (можно оставить пустым):", "", &ok);
    if (!ok) return;

    // Диалог для выбора картинки
    QString imageFilePath = QFileDialog::getOpenFileName(this, "Выбрать изображение", "", "Images (*.png *.jpg *.jpeg)");

    // Собираем JSON для отправки
    QJsonObject productData;
    productData["product_name"] = name;
    productData["product_price"] = price;
    productData["product_description"] = description;
    productData["category_ids"] = QJsonArray({m_selectedCategoryId}); // Привязываем к текущей категории

    // Если картинка выбрана, сначала загружаем ее на сервер
    if (!imageFilePath.isEmpty()) {
        QFile file(imageFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            QNetworkReply* uploadReply = m_networkManager.uploadImage(file.readAll(), QFileInfo(file).fileName());
            // Когда картинка загрузится, мы получим путь и создадим товар
            connect(uploadReply, &QNetworkReply::finished, this, [this, uploadReply, productData]() mutable {
                bool success;
                QJsonDocument doc;
                // ... (логика парсинга ответа от загрузки, как в ProductCardInAdminPanel)
                if (uploadReply->error() == QNetworkReply::NoError) {
                    doc = QJsonDocument::fromJson(uploadReply->readAll());
                    if (doc.isObject() && doc.object().contains("image_path")) {
                        productData["product_image_path"] = doc.object()["image_path"].toString();
                        m_networkManager.addProduct(productData); // Отправляем запрос на создание товара
                    } else {
                        QMessageBox::critical(this, "Ошибка", "Сервер не вернул путь к изображению.");
                    }
                } else {
                    QMessageBox::critical(this, "Ошибка", "Не удалось загрузить изображение: " + uploadReply->errorString());
                }
                uploadReply->deleteLater();
            });
        }
    } else {
        // Если картинка не выбрана, создаем товар без нее
        productData["product_image_path"] = "";
        m_networkManager.addProduct(productData);
    }
}

void AdminWindow::handleAdminAction(bool success, const QString& action, const QString& message, const QString& errorString)
{
    if (success) {
        QMessageBox::information(this, "Успех", message);
        // Обновляем нужную часть интерфейса
        if (action == "add_category" || action == "delete_category") {
            refreshCategories();
        } else {
            refreshProducts();
        }
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось выполнить действие: " + errorString);
    }
}
