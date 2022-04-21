#include <iostream>
#include <Windows.h>

#include "LockFreeMemoryPool.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "TLSMemoryPool.h"
#include "NetServer.h"
#include "ChatServer.h"
//에러 처리용 덤프와 로깅
#include "Dump.h"

#pragma comment (lib, "NetworkLibrary")
//LOG
#pragma region LOG

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_SYSTEM 1
#define LOG_LEVEL_ERROR 2

inline void Log(WCHAR* str, int logLevel) {
    wprintf(L"%s \n", str);
}

inline void ErrorLog(const WCHAR* str) {
    FILE* fp;
    WCHAR fileName[MAX_PATH];
    SYSTEMTIME nowTime;

    GetLocalTime(&nowTime);
    wsprintf(fileName, L"LOG_%d%02d%02d_%02d.%02d.%02d.txt",
        nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond);

    _wfopen_s(&fp, fileName, L"wt");
    if (fp == NULL) return;

    fwprintf_s(fp, str);

    fclose(fp);
}

#define _LOG(LogLevel, fmt, ...)    \
do{                                 \
    if(g_logLevel <= LogLevel){     \
        wsprintf(g_logBuf, fmt, ##__VA_ARGS__);  \
        Log(g_logBuf, LogLevel);                 \
    }                                            \
}while(0)                                    

int g_logLevel;
WCHAR g_logBuf[1024];

#pragma endregion

ChatServer g_ChatServer;
WCHAR IP[16] = L"127.0.0.1";

void ChatServer::Monitor()
{
    system("cls");
    wprintf_s(L"Total Accept : %llu \n", totalAccept);
    wprintf_s(L"Total Send : %llu \n", totalSend);
    wprintf_s(L"Total Recv : %llu \n", totalRecv);
}

int main()
{
	g_ChatServer.Start(IP, 12001, 4, 4, true, 5000);

    for (;;) {
        g_ChatServer.Monitor();
        Sleep(1000);
    }
}


