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
        //console화면 지워지니까 contentsMontior는 그냥 로그만 추가하세요
        g_ChatServer.Monitor();
        g_ChatServer.ContentsMonitor();
        logCnt++;
        if (logCnt & 0x40) {
            logCnt ^= 0x40;
            PROFILE_WRITE();
            PROFILE_RESET();
        }

        Sleep(1000);
    }
}


