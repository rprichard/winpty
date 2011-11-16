#include "Agent.h"
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Q_ASSERT(argc == 4);
    Agent agent(argv[1], atoi(argv[2]), atoi(argv[3]));
    return a.exec();
}
