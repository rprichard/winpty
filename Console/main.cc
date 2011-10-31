#include <QApplication>
#include "ConsoleWindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ConsoleWindow w;
    w.show();
    return a.exec();
}
