#include "Client.h"
#include "DebugClient.h"
#include <windows.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main()
{
    Client *client = new Client();
    client->start();
    AttachConsole(client->processId());
    return 0;
}
