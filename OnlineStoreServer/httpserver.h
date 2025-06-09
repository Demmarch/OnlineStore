#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QHttpServer>
#include <QDir>
#include <QFile>
#include <QUuid>
#include <QMimeDatabase>
#include <QMap>

#include "databasehandler.h"

class HttpServer : public QObject
{
    Q_OBJECT
public:
    explicit HttpServer(DatabaseHandler* dbHandler, QObject *parent = nullptr);
    ~HttpServer() override;
    bool startServer(quint16 port);

private:
    void setupRoutes();

    // === Общие обработчики ===
    QHttpServerResponse handleLogin(const QHttpServerRequest &request);
    QHttpServerResponse handleGetCategories(const QHttpServerRequest &request);
    QHttpServerResponse handleGetProducts(const QHttpServerRequest &request);
    QHttpServerResponse handleServeStaticFile(const QString &fileName);

    // === Обработчики корзины ===
    QHttpServerResponse handlePostCart(const QHttpServerRequest &request);
    QHttpServerResponse handlePostOrder(const QHttpServerRequest &request);
    QHttpServerResponse handleGetCart(const QHttpServerRequest &request);
    QHttpServerResponse handleRemoveFromCart(const QHttpServerRequest &request);

    // === Обработчики админ панели ===
    QHttpServerResponse handlePostCategory(const QHttpServerRequest &request);
    QHttpServerResponse handleDeleteCategory(int categoryId);
    QHttpServerResponse handlePostProducts(const QHttpServerRequest &request);
    QHttpServerResponse handleDeleteProduct(int productId);
    QHttpServerResponse handleUpdateProduct(int productId, const QHttpServerRequest &request);;
    QHttpServerResponse handleImageUpload(const QHttpServerRequest &request);
    QHttpServerResponse handleChangeProductCategory(int productId, const QHttpServerRequest &request);

    QHttpServer m_httpServer;
    QTcpServer  m_tcpServer;
    DatabaseHandler* m_dbHandler;
};

#endif // HTTPSERVER_H
