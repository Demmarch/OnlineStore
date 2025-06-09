#ifndef CARTWINDOW_H
#define CARTWINDOW_H

#include <QMainWindow>
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class CartWindow; }
QT_END_NAMESPACE

class CartWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Передаем ID пользователя и менеджер сети из CustomerWindow
    explicit CartWindow(int userId, NetworkManager* networkManager, QWidget *parent = nullptr);
    ~CartWindow();

signals:
    void purchaseCompleted(); // Сигнал для CustomerWindow, чтобы обновить каталог

private slots:
    void on_btnBack_clicked();
    void on_btnBuy_clicked();

    void handleCartContentsFetched(bool success, const QJsonObject& cartData, const QString& errorString);
    void handleProductRemoved(bool success, const QString& message, const QString& errorString);
    void handleOrderPlaced(bool success, const QString& message, const QString& errorString);

    void onRemoveFromCartClicked(int productId);

private:
    Ui::CartWindow *ui;
    NetworkManager* m_networkManager;
    int m_currentUserId;
    double m_totalPrice = 0.0;

    void loadCartContents();
    void clearCartLayout();
};

#endif // CARTWINDOW_H
