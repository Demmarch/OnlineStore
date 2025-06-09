#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QMainWindow>
#include "networkmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoginWindow; } // Используем имя класса из .ui файла
QT_END_NAMESPACE

class LoginWindow : public QMainWindow // Наследуемся от QMainWindow
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow();

signals:
    void loginSuccessful(int userId, const QString& userRole);

private slots:
    void on_pushButtonAuth_clicked();
    void handleLoginResponse(bool success, const QJsonObject& userData, const QString& errorString);

private:
    Ui::LoginWindow *ui;
    NetworkManager m_networkManager;
};

#endif // LOGINWINDOW_H
