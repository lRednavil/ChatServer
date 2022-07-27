#include "stdafx.h"

CDump dump;
ChatServer g_ChatServer;

int main()
{
    LogInit();
    g_ChatServer.Init();
	
    timeBeginPeriod(1);

    int logCnt = 0;

    for (;;) {
        logCnt++;
        g_ChatServer.ContentsMonitor();
        Sleep(1000);
    }
}


