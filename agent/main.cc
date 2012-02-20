#include "Agent.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Q_ASSERT(argc == 5);
    Agent agent(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]));
    return a.exec();
}
