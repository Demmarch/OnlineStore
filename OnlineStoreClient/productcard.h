#ifndef PRODUCTCARD_H
#define PRODUCTCARD_H

#include <QWidget>
#include <QNetworkReply>
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ProductCard; }
QT_END_NAMESPACE

class ProductCard : public QWidget
{
    Q_OBJECT

public:
    explicit ProductCard(int productId, const QString &name, const QString &description,
                         double price, const QString &imageUrl,
                         NetworkManager *networkManager, // Передаем NetworkManager
                         QWidget *parent = nullptr);
    ~ProductCard();

    int getProductId() const;

signals:
    void addToCartClicked(int productId); // Сигнал при нажатии кнопки "В корзину"

private slots:
    void on_addToCartButton_clicked();
    void onImageFetched(QNetworkReply* reply);

private:
    Ui::ProductCard *ui;
    int m_productId;
    QString m_imageUrl;
    NetworkManager* m_networkManager; // Для загрузки изображения
};

#endif // PRODUCTCARD_H
