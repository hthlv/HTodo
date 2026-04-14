#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("HTodo");
    QApplication::setOrganizationName("hyp");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setWindowIcon(QIcon(":/icons/htodo.png"));

    MainWindow window;
    window.setWindowIcon(QIcon(":/icons/htodo.png"));
    window.show();

    return app.exec();
}
