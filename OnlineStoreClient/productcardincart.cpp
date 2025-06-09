#include "productcardincart.h"
#include "ui_productcardincart.h"
#include <QPixmap>
#include <QDebug>

ProductCardInCart::ProductCardInCart(int productId, const QString &name, double price,
                                     const QString &imageUrl, NetworkManager* networkManager,
                                     QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProductCardInCart),
    m_productId(productId)
{
    ui->setupUi(this);
    this->setStyleSheet("QFrame, QWidget { border: 1px solid #c0c0c0; border-radius: 5px; }");

    ui->productNameLabel->setText(name);
    ui->productPriceLabel->setText("Цена: " + QString::number(price, 'f', 2) + " руб.");

    if (networkManager && !imageUrl.isEmpty()) {
        QNetworkReply* reply = networkManager->fetchImage(imageUrl);
        connect(reply, &QNetworkReply::finished, this, [this, reply](){ onImageFetched(reply); });
    }
}

ProductCardInCart::~ProductCardInCart()
{
    delete ui;
}

void ProductCardInCart::onImageFetched(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QPixmap pixmap;
        pixmap.loadFromData(reply->readAll());
        ui->productImageLabel->setPixmap(pixmap.scaled(ui->productImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        ui->productImageLabel->setText("Ошибка фото");
    }
    reply->deleteLater();
}

void ProductCardInCart::on_btnRemoveProduct_clicked()
{
    emit removeClicked(m_productId);
}
