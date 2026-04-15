#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QLockFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QStyleFactory>

#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("HTodo");
    QApplication::setOrganizationName("hyp");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setWindowIcon(QIcon(":/icons/htodo.png"));

    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString lockDir = runtimeDir.isEmpty() ? QDir::tempPath() : runtimeDir;
    QDir().mkpath(lockDir);
    const QString instanceServerName = QStringLiteral("HTodo.SingleInstance");
    QLockFile instanceLock(QDir(lockDir).filePath("HTodo.lock"));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock()) {
        QLocalSocket socket;
        socket.connectToServer(instanceServerName);
        if (socket.waitForConnected(500)) {
            socket.write("activate");
            socket.flush();
            socket.waitForBytesWritten(500);
            socket.disconnectFromServer();
        }
        return 0;
    }

    QLocalServer::removeServer(instanceServerName);
    auto *instanceServer = new QLocalServer(&app);
    if (!instanceServer->listen(instanceServerName)) {
        delete instanceServer;
        instanceServer = nullptr;
    }

    MainWindow window;
    window.setWindowIcon(QIcon(":/icons/htodo.png"));
    window.attachSingleInstanceServer(instanceServer);
    window.show();

    return app.exec();
}
