#include "Agent.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Q_ASSERT(argc == 2);
    Agent agent(argv[1]);
    return a.exec();
}
