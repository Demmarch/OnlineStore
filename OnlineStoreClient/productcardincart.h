#ifndef PRODUCTCARDINCART_H
#define PRODUCTCARDINCART_H

#include <QWidget>
#include <QNetworkReply>
#include "networkmanager.h" // Для загрузки картинки

QT_BEGIN_NAMESPACE
namespace Ui { class ProductCardInCart; }
QT_END_NAMESPACE

class ProductCardInCart : public QWidget
{
    Q_OBJECT

public:
    explicit ProductCardInCart(int productId, const QString &name, double price,
                               const QString &imageUrl, NetworkManager* networkManager,
                               QWidget *parent = nullptr);
    ~ProductCardInCart();

signals:
    void removeClicked(int productId); // Сигнал для удаления

private slots:
    void on_btnRemoveProduct_clicked();
    void onImageFetched(QNetworkReply* reply);

private:
    Ui::ProductCardInCart *ui;
    int m_productId;
};

#endif // PRODUCTCARDINCART_H
