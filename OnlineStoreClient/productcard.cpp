#include "productcard.h"
#include "ui_productcard.h"
#include <QPixmap>
#include <QDebug>

ProductCard::ProductCard(int productId, const QString &name, const QString &description,
                         double price, const QString &imageUrl,
                         NetworkManager *networkManager, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProductCard),
    m_productId(productId),
    m_imageUrl(imageUrl),
    m_networkManager(networkManager)
{
    ui->setupUi(this);

    ui->nameLabel->setText(name);
    ui->descriptionLabel->setText(description.left(100) + (description.length() > 100 ? "..." : "")); // Ограничиваем описание
    ui->priceLabel->setText(QString::number(price, 'f', 2) + " руб.");

    // Загрузка изображения
    if (m_networkManager && !m_imageUrl.isEmpty()) {
        QNetworkReply* reply = m_networkManager->fetchImage(m_imageUrl);
        if (reply) { // fetchImage теперь возвращает QNetworkReply*
            connect(reply, &QNetworkReply::finished, this, [this, reply](){
                onImageFetched(reply);
            });
        } else {
            ui->imageLabel->setText("Ошибка URL фото");
        }
    } else {
        ui->imageLabel->setText("Нет фото");
    }
}

ProductCard::~ProductCard()
{
    delete ui;
}

int ProductCard::getProductId() const
{
    return m_productId;
}

void ProductCard::on_addToCartButton_clicked() // Автоматически соединяется по имени
{
    emit addToCartClicked(m_productId);
}

void ProductCard::onImageFetched(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        if (!pixmap.isNull()) {
            ui->imageLabel->setPixmap(pixmap.scaled(ui->imageLabel->size(),
                                                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            ui->imageLabel->setText("Ошибка фото");
            qDebug() << "ProductCard: Could not load pixmap from data for" << m_imageUrl;
        }
    } else {
        ui->imageLabel->setText("Ошибка фото");
        qDebug() << "ProductCard: Failed to fetch image" << m_imageUrl << ":" << reply->errorString();
    }
    reply->deleteLater();
}
