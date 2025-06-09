#include "loginwindow.h"
#include "ui_loginwindow.h" // Подключаем заголовок для UI формы
#include <QMessageBox>
#include <QDebug>

LoginWindow::LoginWindow(QWidget *parent) :
    QMainWindow(parent), // Вызываем конструктор QMainWindow
    ui(new Ui::LoginWindow) // Создаем экземпляр UI
{
    ui->setupUi(this); // Настраиваем UI для этого окна

    connect(&m_networkManager, &NetworkManager::loginCompleted, this, &LoginWindow::handleLoginResponse);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::on_pushButtonAuth_clicked()
{
    QString username = ui->lineLogin->text();
    QString password = ui->linePasswor->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Ошибка входа", "Пожалуйста, введите логин и пароль.");
        return;
    }

    ui->pushButtonAuth->setEnabled(false);
    m_networkManager.login(username, password);
}

void LoginWindow::handleLoginResponse(bool success, const QJsonObject& userData, const QString& errorString)
{
    ui->pushButtonAuth->setEnabled(true);
    if (success) {
        int userId = userData.value("user_id").toInt(-1);
        QString userRole = userData.value("user_role").toString();

        if (userId != -1 && !userRole.isEmpty()) {
            qDebug() << "LoginWindow: Login successful. User ID:" << userId << "Role:" << userRole;
            emit loginSuccessful(userId, userRole);
        } else {
            QMessageBox::warning(this, "Ошибка входа", "Неверный формат данных от сервера.");
            qDebug() << "LoginWindow: Invalid data from server after login:" << userData;
        }
    } else {
        QMessageBox::warning(this, "Ошибка входа", "Не удалось войти: " + errorString);
        qDebug() << "LoginWindow: Login failed. Error:" << errorString;
    }
}
