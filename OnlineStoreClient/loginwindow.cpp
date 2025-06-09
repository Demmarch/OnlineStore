#include "loginwindow.h"
#include "ui_loginwindow.h" // Подключаем заголовок для UI формы
#include <QMessageBox>
#include <QDebug>

LoginWindow::LoginWindow(QWidget *parent) :
    QMainWindow(parent), // Вызываем конструктор QMainWindow
    ui(new Ui::LoginWindow) // Создаем экземпляр UI
{
    ui->setupUi(this); // Настраиваем UI для этого окна

    // Соединяем сигнал от NetworkManager со слотом этого окна
    connect(&m_networkManager, &NetworkManager::loginCompleted, this, &LoginWindow::handleLoginResponse);

    // Поля lineLogin и linePasswor доступны через ui->lineLogin и ui->linePasswor
    // Кнопка pushButtonAuth доступна через ui->pushButtonAuth
    // Их сигналы (например, clicked для кнопки) могут быть соединены здесь или автоматически по имени
    // (если слот называется on_<objectName>_<signalName>())
}

LoginWindow::~LoginWindow()
{
    delete ui; // Освобождаем память от UI
}

void LoginWindow::on_pushButtonAuth_clicked() // Имя слота соответствует objectName кнопки
{
    QString username = ui->lineLogin->text(); // Используем objectName из .ui
    QString password = ui->linePasswor->text(); // Используем objectName из .ui (linePasswor)

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Ошибка входа", "Пожалуйста, введите логин и пароль.");
        return;
    }

    ui->pushButtonAuth->setEnabled(false); // Блокируем кнопку на время запроса
    m_networkManager.login(username, password);
}

void LoginWindow::handleLoginResponse(bool success, const QJsonObject& userData, const QString& errorString)
{
    ui->pushButtonAuth->setEnabled(true); // Разблокируем кнопку

    if (success) {
        int userId = userData.value("user_id").toInt(-1);
        QString userRole = userData.value("user_role").toString();

        if (userId != -1 && !userRole.isEmpty()) {
            qDebug() << "LoginWindow: Login successful. User ID:" << userId << "Role:" << userRole;
            // QMessageBox::information(this, "Успех", "Вход выполнен успешно!"); // Можно убрать, если сразу переход
            emit loginSuccessful(userId, userRole);
            // this->close(); // main.cpp закроет это окно
        } else {
            QMessageBox::warning(this, "Ошибка входа", "Неверный формат данных от сервера.");
            qDebug() << "LoginWindow: Invalid data from server after login:" << userData;
        }
    } else {
        QMessageBox::warning(this, "Ошибка входа", "Не удалось войти: " + errorString);
        qDebug() << "LoginWindow: Login failed. Error:" << errorString;
    }
}
