#include <QCoreApplication>
#include "databasehandler.h"
#include "httpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString dbHost = "localhost";
    int dbPort = 5432;
    QString dbName = "OnlineStore";
    QString dbUser = "postgres";
    QString dbPassword = "demmarc";

    DatabaseHandler dbHandler;
    if (!dbHandler.connectToDatabase(dbHost, dbPort, dbName, dbUser, dbPassword)) {
        qCritical() << "Failed to connect to the database. Exiting.";
        return -1;
    }

    // Создаем и запускаем HTTP сервер
    HttpServer server(&dbHandler);
    quint16 serverPort = 8080; // Порт для сервера

    if (!server.startServer(serverPort)) {
        qCritical() << "Failed to start the HTTP server. Exiting.";
        return -1;
    }

    qInfo() << QString("Online Store Server is running on http://localhost:%1").arg(serverPort);

    return a.exec();
}
