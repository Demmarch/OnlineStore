#include "cartwindow.h"
#include "ui_cartwindow.h"
#include "productcardincart.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QVBoxLayout>

CartWindow::CartWindow(int userId, NetworkManager* networkManager, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CartWindow),
    m_networkManager(networkManager),
    m_currentUserId(userId)
{
    ui->setupUi(this);
    setWindowTitle("Корзина");
    setAttribute(Qt::WA_DeleteOnClose); // Окно будет удалено после закрытия

    // Убедимся, что у scrollAreaWidgetContents есть layout
    if (!ui->scrollAreaWidgetContents->layout()) {
        ui->scrollAreaWidgetContents->setLayout(new QVBoxLayout());
        ui->scrollAreaWidgetContents->layout()->setContentsMargins(5,5,5,5);
    }
    // В вашем .ui файле у scrollAreaWidgetContents уже есть QVBoxLayout 'verticalLayout', будем использовать его.

    // Соединение сигналов
    connect(m_networkManager, &NetworkManager::cartContentsFetched, this, &CartWindow::handleCartContentsFetched);
    connect(m_networkManager, &NetworkManager::productRemovedFromCart, this, &CartWindow::handleProductRemoved);
    connect(m_networkManager, &NetworkManager::orderPlaced, this, &CartWindow::handleOrderPlaced);

    loadCartContents();
}

CartWindow::~CartWindow()
{
    delete ui;
}

void CartWindow::loadCartContents()
{
    if (m_networkManager && m_currentUserId > 0) {
        ui->productsInCartLabel->setText("Загрузка...");
        m_networkManager->fetchCartContents(m_currentUserId);
    }
}

void CartWindow::clearCartLayout()
{
    QLayout *layout = ui->verticalLayout; // Используем имя компоновщика из .ui
    if (!layout) return;
    QLayoutItem* item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (QWidget* widget = item->widget()) {
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
}

void CartWindow::handleCartContentsFetched(bool success, const QJsonObject& cartData, const QString& errorString)
{
    clearCartLayout();
    if (!success) {
        QMessageBox::critical(this, "Ошибка", "Не удалось загрузить корзину: " + errorString);
        return;
    }

    QJsonArray items = cartData["items"].toArray();
    m_totalPrice = cartData["total_price"].toDouble();

    ui->productsInCartLabel->setText(QString("Товары в корзине: %1").arg(items.count()));
    ui->totalPriceLabel->setText(QString("Всего: %1 руб.").arg(QString::number(m_totalPrice, 'f', 2)));

    if (items.isEmpty()) {
        QLabel* emptyLabel = new QLabel("Ваша корзина пуста.", this);
        emptyLabel->setAlignment(Qt::AlignCenter);
        ui->verticalLayout->addWidget(emptyLabel);
        ui->btnBuy->setEnabled(false);
    } else {
        ui->btnBuy->setEnabled(true);
        for (const QJsonValue& val : items) {
            QJsonObject itemObj = val.toObject();
            ProductCardInCart* card = new ProductCardInCart(
                itemObj["product_id"].toInt(),
                itemObj["product_name"].toString(),
                itemObj["product_price"].toDouble(),
                itemObj["product_image_path"].toString(),
                m_networkManager,
                this
                );
            connect(card, &ProductCardInCart::removeClicked, this, &CartWindow::onRemoveFromCartClicked);
            ui->verticalLayout->addWidget(card);
        }
    }
}

void CartWindow::onRemoveFromCartClicked(int productId)
{
    qDebug() << "CartWindow: Requesting to remove product" << productId;
    ui->centralwidget->setEnabled(false); // Блокируем UI на время запроса
    m_networkManager->removeFromCart(m_currentUserId, productId);
}

void CartWindow::handleProductRemoved(bool success, const QString& message, const QString& errorString)
{
    ui->centralwidget->setEnabled(true); // Разблокируем UI
    if (success) {
        //QMessageBox::information(this, "Удаление", message);
        loadCartContents(); // Обновляем содержимое корзины
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось удалить товар: " + errorString);
    }
}

void CartWindow::on_btnBack_clicked()
{
    this->close();
}

void CartWindow::on_btnBuy_clicked()
{
    bool ok;
    QString promptText = QString("Общая сумма вашего заказа: %1 руб.\nПожалуйста, введите сумму для подтверждения покупки:")
                             .arg(QString::number(m_totalPrice, 'f', 2));

    double enteredSum = QInputDialog::getDouble(this, "Подтверждение покупки", promptText, 0, 0, 10000000, 2, &ok);

    if (ok) { // Пользователь нажал OK
        if (qAbs(enteredSum - m_totalPrice) < 0.001) { // Сравниваем double с погрешностью
            qDebug() << "CartWindow: Correct sum entered. Placing order.";
            ui->centralwidget->setEnabled(false);
            m_networkManager->placeOrder(m_currentUserId);
        } else {
            QMessageBox::warning(this, "Ошибка суммы", "Введенная сумма не совпадает с итоговой суммой заказа.");
        }
    }
    // Если пользователь нажал Cancel, ничего не делаем.
}

void CartWindow::handleOrderPlaced(bool success, const QString& message, const QString& errorString)
{
    ui->centralwidget->setEnabled(true);
    if (success) {
        QMessageBox::information(this, "Покупка совершена!", "Ваш заказ успешно оформлен. Товары куплены.");
        emit purchaseCompleted(); // Сигнал для обновления основного окна
        this->close();
    } else {
        QMessageBox::critical(this, "Ошибка покупки", "Не удалось оформить заказ: " + errorString);
    }
}
