#ifndef PRODUCTCARDINADMINPANEL_H
#define PRODUCTCARDINADMINPANEL_H

#include <QWidget>
#include <QJsonObject>
#include <QNetworkReply>
#include <QJsonArray>
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ProductCardInAdminPanel; }
QT_END_NAMESPACE

class ProductCardInAdminPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProductCardInAdminPanel(const QJsonObject &productData,
                                     const QJsonArray &allCategories,
                                     int currentCategoryId,
                                     NetworkManager *networkManager,
                                     QWidget *parent = nullptr);
    ~ProductCardInAdminPanel();

    int getProductId() const;

signals:
    // Сигнал, который просит родительское окно (AdminWindow) обновиться
    void productDataChanged();

private slots:
    // Слоты для кнопок из UI
    void on_btnDeleteProduct_clicked();
    void on_btnChangeName_clicked();
    void on_btnChangeDescription_clicked();
    void on_btnChangePrice_clicked();
    void on_btnChangeImage_clicked();
    void on_comboBoxCategories_currentIndexChanged(int index);

    // Слоты для обработки ответов от NetworkManager
    void onImageFetched(QNetworkReply* reply);
    void onImageUploaded(QNetworkReply* reply);

private:
    Ui::ProductCardInAdminPanel *ui;
    NetworkManager* m_networkManager;
    QJsonObject m_productData;
    int m_productId;
    int m_currentCategoryId;

    void updateProductField(const QString& fieldName, const QVariant& value);
    void updateUi(); // Вспомогательный метод для обновления данных на карточке
    void populateCategories(const QJsonArray &allCategories, int currentCategoryId);
};

#endif // PRODUCTCARDINADMINPANEL_H
