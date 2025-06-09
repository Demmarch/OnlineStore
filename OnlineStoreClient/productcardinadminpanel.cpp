#include "productcardinadminpanel.h"
#include "ui_productcardinadminpanel.h"
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>
#include <QDebug>
#include <QPixmap>

ProductCardInAdminPanel::ProductCardInAdminPanel(const QJsonObject &productData,
                                                 const QJsonArray &allCategories,
                                                 int currentCategoryId, // Принимаем ID текущей категории
                                                 NetworkManager *networkManager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProductCardInAdminPanel),
    m_networkManager(networkManager),
    m_productData(productData),
    m_currentCategoryId(currentCategoryId)
{
    ui->setupUi(this);
    this->setStyleSheet("QWidget#ProductCardInAdminPanel { border: 1px solid #c0c0c0; border-radius: 5px; }");
    m_productId = m_productData["product_id"].toInt();

    updateUi();
    populateCategories(allCategories, m_currentCategoryId);
}

ProductCardInAdminPanel::~ProductCardInAdminPanel()
{
    delete ui;
}

void ProductCardInAdminPanel::updateUi()
{
    ui->productIdLabel->setText("ID: " + QString::number(m_productId));
    ui->productNamelabel->setText(m_productData["product_name"].toString());
    ui->productDescriptionLabel->setText(m_productData["product_description"].toString());
    ui->productPriceLabel->setText("Цена: " + QString::number(m_productData["product_price"].toDouble(), 'f', 2) + " руб.");

    // Загрузка изображения
    QString imageUrl = m_productData["product_image_path"].toString();
    if (m_networkManager && !imageUrl.isEmpty()) {
        QNetworkReply* reply = m_networkManager->fetchImage(imageUrl);
        if (reply) {
            connect(reply, &QNetworkReply::finished, this, [this, reply](){ onImageFetched(reply); });
        }
    } else {
        ui->productImagelabel->setText("Нет изображения");
    }
}

void ProductCardInAdminPanel::populateCategories(const QJsonArray &allCategories, int currentCategoryId)
{
    // Блокируем сигналы, чтобы избежать срабатывания слота on_comboBoxCategories_currentIndexChanged во время заполнения
    ui->comboBoxCategories->blockSignals(true);

    ui->comboBoxCategories->clear();
    int currentCategoryIndex = -1; // Индекс текущей категории товара

    for (int i = 0; i < allCategories.size(); ++i) {
        QJsonObject cat = allCategories[i].toObject();
        int categoryId = cat["category_id"].toInt();
        QString categoryName = cat["category_name"].toString();

        // Добавляем элемент, сохраняя ID категории в UserRole
        ui->comboBoxCategories->addItem(categoryName, categoryId);

        // Если ID этой категории совпадает с ID текущей категории товара, запоминаем ее индекс
        if (categoryId == currentCategoryId) {
            currentCategoryIndex = i;
        }
    }

    // Устанавливаем текущую категорию
    if (currentCategoryIndex != -1) {
        ui->comboBoxCategories->setCurrentIndex(currentCategoryIndex);
    }

    // Разблокируем сигналы
    ui->comboBoxCategories->blockSignals(false);
}

void ProductCardInAdminPanel::on_comboBoxCategories_currentIndexChanged(int index)
{
    if (index < 0) return; // Элемент не выбран

    // Получаем ID новой выбранной категории
    int newCategoryId = ui->comboBoxCategories->itemData(index).toInt();

    // Если выбрана та же категория, что и была, ничего не делаем
    if (newCategoryId == m_currentCategoryId) {
        return;
    }

    qDebug() << "ProductCard: User requested category change for product" << m_productId
             << "from" << m_currentCategoryId << "to" << newCategoryId;

    // Вызываем метод NetworkManager для отправки запроса на сервер
    m_networkManager->changeProductCategory(m_productId, m_currentCategoryId, newCategoryId);
}

int ProductCardInAdminPanel::getProductId() const
{
    return m_productId;
}

void ProductCardInAdminPanel::onImageFetched(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        if (!pixmap.isNull()) {
            ui->productImagelabel->setPixmap(pixmap.scaled(ui->productImagelabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } else {
        ui->productImagelabel->setText("Ошибка фото");
    }
    reply->deleteLater();
}

void ProductCardInAdminPanel::on_btnDeleteProduct_clicked()
{
    auto reply = QMessageBox::question(this, "Удаление товара",
                                       "Вы уверены, что хотите удалить товар '" + m_productData["product_name"].toString() + "'?",
                                       QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_networkManager->deleteProduct(m_productId);
    }
}

void ProductCardInAdminPanel::on_btnChangeName_clicked()
{
    bool ok;
    QString newName = QInputDialog::getText(this, "Изменить название", "Новое название:", QLineEdit::Normal, m_productData["product_name"].toString(), &ok);
    if (ok && !newName.isEmpty()) {
        updateProductField("product_name", newName);
    }
}

void ProductCardInAdminPanel::on_btnChangeDescription_clicked()
{
    bool ok;
    QString newDesc = QInputDialog::getMultiLineText(this, "Изменить описание", "Новое описание:", m_productData["product_description"].toString(), &ok);
    if (ok) { // Описание может быть пустым
        updateProductField("product_description", newDesc);
    }
}

void ProductCardInAdminPanel::on_btnChangePrice_clicked()
{
    bool ok;
    double newPrice = QInputDialog::getDouble(this, "Изменить цену", "Новая цена:", m_productData["product_price"].toDouble(), 0.01, 10000000, 2, &ok);
    if (ok) {
        updateProductField("product_price", newPrice);
    }
}

void ProductCardInAdminPanel::on_btnChangeImage_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Выбрать новое изображение", "", "Images (*.jpg)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл изображения.");
        return;
    }
    QByteArray imageData = file.readAll();
    QFileInfo fileInfo(filePath);

    QNetworkReply* reply = m_networkManager->uploadImage(imageData, fileInfo.fileName());
    connect(reply, &QNetworkReply::finished, this, [this, reply](){ onImageUploaded(reply); });
}

void ProductCardInAdminPanel::onImageUploaded(QNetworkReply* reply)
{
    bool success;
    QJsonDocument doc;
    QString errorStr;
    // Используем лямбду для обработки JSON
    auto callback = [&](bool s, const QJsonDocument& d, const QString& e) {
        success = s;
        doc = d;
        errorStr = e;
    };
    // Это вспомогательный обработчик, который нужно будет создать в NetworkManager или здесь
    // Пока что реализуем логику прямо тут:
    if (reply->error() == QNetworkReply::NoError) {
        QJsonParseError parseError;
        doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        success = (parseError.error == QJsonParseError::NoError);
        if (!success) errorStr = "JSON Parse Error: " + parseError.errorString();
    } else {
        success = false;
        errorStr = reply->errorString();
    }


    if (success && doc.isObject()) {
        QString newPath = doc.object()["image_path"].toString();
        if (!newPath.isEmpty()) {
            updateProductField("product_image_path", newPath);
        } else {
            QMessageBox::critical(this, "Ошибка", "Сервер не вернул путь к изображению.");
        }
    } else {
        QMessageBox::critical(this, "Ошибка загрузки изображения", errorStr);
    }

    reply->deleteLater();
}

void ProductCardInAdminPanel::updateProductField(const QString& fieldName, const QVariant& value)
{
    m_networkManager->updateProductField(m_productId, fieldName, value);
}
