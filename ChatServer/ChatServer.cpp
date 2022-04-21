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

int main()
{
	g_ChatServer.Start(IP, 12001, 4, 4, true, 5000);

	scanf_s("%s");
}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
