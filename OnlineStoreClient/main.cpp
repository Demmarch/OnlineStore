#include "loginwindow.h"
#include "customerwindow.h"
#include "adminwindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    LoginWindow loginWindow;

    CustomerWindow *customerWindow = new CustomerWindow();
    AdminWindow *adminWindow = new AdminWindow(); // Создаем окно админа

    QObject::connect(&loginWindow, &LoginWindow::loginSuccessful,
                     [&](int userId, const QString& userRole) {
                         if (userRole == "user") {
                             customerWindow->setCurrentUser(userId);
                             customerWindow->show();
                             loginWindow.close();
                         } else if (userRole == "admin") {
                             adminWindow->setAdminId(userId); // Передаем ID админа
                             adminWindow->show();
                             loginWindow.close();
                         } else {
                             QMessageBox::warning(&loginWindow, "Ошибка роли", "Неизвестная роль пользователя: " + userRole);
                         }
                     });

    loginWindow.show();
    int result = a.exec();
    delete customerWindow;
    delete adminWindow; // Не забываем удалить
    return result;
}
