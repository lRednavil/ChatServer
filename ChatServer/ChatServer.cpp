#include <iostream>
#include <Windows.h>
#include <direct.h>

#include "LockFreeMemoryPool.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "TLSMemoryPool.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "SerializedBuffer.h"

#include "CommonProtocol.h"

//에러 처리용 덤프와 로깅
#include "Dump.h"

#pragma comment (lib, "NetworkLibrary")
#pragma comment (lib, "Winmm")
//LOG
#pragma region LOG

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_SYSTEM 1
#define LOG_LEVEL_ERROR 2

int g_logLevel;
WCHAR g_logBuf[1024];
WCHAR g_logFileName[MAX_PATH];

inline void Log(WCHAR* str, int logLevel) {
    wprintf(L"%s \n", str);
}

inline void LogInit() {
    SYSTEMTIME nowTime;

    GetLocalTime(&nowTime);
    wsprintf(g_logFileName, L"LOG\\LOG_%d%02d%02d_%02d.%02d.%02d",
        nowTime.wYear, nowTime.wMonth, nowTime.wDay, nowTime.wHour, nowTime.wMinute, nowTime.wSecond);

    _wmkdir(L"LOG");
}

inline void ErrorLog(const WCHAR* str) {
    FILE* fp;
    WCHAR fileName[MAX_PATH];
    DWORD threadID = GetCurrentThreadId();

    swprintf_s(fileName, L"%s_%u.txt",g_logFileName, threadID);
       
    _wfopen_s(&fp, fileName, L"at");
    if (fp == NULL) return;

    fwprintf_s(fp, str);
    fwprintf_s(fp, L"\n");

    fclose(fp);
}

inline void ErrorLog(const int err, const WCHAR* str) {
    FILE* fp;
    WCHAR fileName[MAX_PATH];
    DWORD threadID = GetCurrentThreadId();

    swprintf_s(fileName, L"%s_%u.txt", g_logFileName, threadID);

    _wfopen_s(&fp, fileName, L"at");
    if (fp == NULL) return;

    fwprintf_s(fp, L"%d ", err);
    fwprintf_s(fp, str);
    fwprintf_s(fp, L"\n");

    fclose(fp);
}

#define _LOG(LogLevel, fmt, ...)    \
do{                                 \
    if(g_logLevel <= LogLevel){     \
        wsprintf(g_logBuf, fmt, ##__VA_ARGS__);  \
        Log(g_logBuf, LogLevel);                 \
    }                                            \
}while(0)                                    



#pragma endregion

#define PORT 12001
#define MAX_CONNECT 5000

#define TIME_OUT 40000

ChatServer g_ChatServer;
WCHAR IP[16] = L"127.0.0.1";

ChatServer::ChatServer()
{
    hThreads[0] = (HANDLE)_beginthreadex(NULL, 0, _UpdateThread, this, NULL, 0);
    hThreads[1] = (HANDLE)_beginthreadex(NULL, 0, _TimerThread, this, NULL, 0);

    isServerOn = true;
}

ChatServer::~ChatServer()
{
    isServerOn = false;

    WaitForMultipleObjects(2, hThreads, true, INFINITE);
}

void ChatServer::Monitor()
{
    system("cls");
    wprintf_s(L"Total Accept : %llu \n", totalAccept);
    wprintf_s(L"Total Send : %llu \n", totalSend);
    wprintf_s(L"Total Recv : %llu \n", totalRecv);
}

bool ChatServer::OnClientJoin(DWORD64 sessionID)
{
    //임시로 무조건 승인중
    return true;
}

bool ChatServer::OnClientLeave(DWORD64 sessionID)
{
    return true;
}

void ChatServer::OnRecv(DWORD64 sessionID, CPacket* packet)
{
    WORD type;
    JOB job;

    *packet >> type;
   

    switch (type) {
    case en_PACKET_CS_CHAT_REQ_LOGIN:
    {
        job.type = en_PACKET_CS_CHAT_REQ_LOGIN;
        job.sessionID = sessionID;
        job.packet = packet;

        jobQ.Enqueue(job);
    }
        break;

    case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE: 
    {
        job.type = en_PACKET_CS_CHAT_REQ_LOGIN;
        job.sessionID = sessionID;
        job.packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_MESSAGE:
    {
        job.type = en_PACKET_CS_CHAT_REQ_LOGIN;
        job.sessionID = sessionID;
        job.packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
    {
        job.type = en_PACKET_CS_CHAT_REQ_LOGIN;
        job.sessionID = sessionID;
        job.packet = packet;

        jobQ.Enqueue(job);
    }
        break;

    default:
        Disconnect(sessionID);
    }
}

void ChatServer::OnError(int error, const WCHAR* msg)
{
    if (error != -1) ErrorLog(error, msg);
    else ErrorLog(msg);
}

unsigned int __stdcall ChatServer::_UpdateThread(void* arg)
{
    ChatServer* server = (ChatServer*)arg;
    JOB job;

    while (server->isServerOn) {
        //쉬게 할 방법 추가 고민
        if (server->jobQ.Dequeue(&job) == false) {
            Sleep(0);
        }

        switch (job.type) {
        case en_PACKET_CS_CHAT_REQ_LOGIN:
        {
            server->Recv_Login(job.sessionID, job.packet);
        }
        break;

        case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
        {
            server->Recv_SectorMove(job.sessionID, job.packet);
        }
        break;

        case en_PACKET_CS_CHAT_REQ_MESSAGE:
        {
            server->Recv_Message(job.sessionID, job.packet);
        }
        break;

        case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
        {
            server->Recv_HeartBeat(job.sessionID, job.packet);
        }
        break;
        default:
            server->Disconnect(job.sessionID);
        }
    }

    return 0;
}

unsigned int __stdcall ChatServer::_TimerThread(void* arg)
{
    ChatServer* server = (ChatServer*)arg;
    
    while (server->isServerOn) {
        server->currentTime = timeGetTime();
        for (auto itr = server->playerMap.begin(); itr != server->playerMap.end(); ++itr) {
            if (itr->second->lastTime - server->currentTime >= TIME_OUT) {
                server->Disconnect(itr->first);
            }
        }

        Sleep(1000);
    }

    return 0;

}

void ChatServer::Recv_Login(DWORD64 sessionID, CPacket* packet)
{
    //packet 추출
    PLAYER* player = new PLAYER;
    *packet >> player->accountNo;
    packet->GetData((char*)player->ID, 40);
    packet->GetData((char*)player->Nickname, 40);
    packet->GetData(player->sessionKey, 64);

    PacketFree(packet);
    //플레이어 생성 성공(추가 가능한 필터링 -> 아이디나 닉네임 규정 위반)
    if (playerMap.find(sessionID) != playerMap.end()) {
        playerMap.insert({ sessionID, player });
        Res_Login(player->accountNo, sessionID, true);
        player->lastTime = currentTime;
    }
    else {
        Res_Login(player->accountNo, sessionID, false);
        Disconnect(sessionID);
        return;
    }
}

void ChatServer::Res_Login(INT64 accountNo, DWORD64 sessionID, bool isSuccess)
{
    CPacket* packet = PacketAlloc();
    
    *packet << (WORD)en_PACKET_SC_CHAT_RES_LOGIN << isSuccess << accountNo;

    SendPacket(sessionID, packet);
}

void ChatServer::Recv_SectorMove(DWORD64 sessionID, CPacket* packet)
{
    PLAYER* player;
    
    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        PacketFree(packet);
        Disconnect(sessionID);
        return;
    }

    //accountNO처리 한번더 고민
    *packet >> player->accountNo >> player->sectorX >> player->sectorY;

    PacketFree(packet);

    player->lastTime = currentTime;

    Res_SectorMove(player, sessionID);
}

void ChatServer::Res_SectorMove(PLAYER* player, DWORD64 sessionID)
{
    CPacket* packet = PacketAlloc();

    *packet << (WORD)en_PACKET_SC_CHAT_RES_SECTOR_MOVE << player->accountNo << player->sectorX << player->sectorY;

    SendPacket(sessionID, packet);
}

void ChatServer::Recv_Message(DWORD64 sessionID, CPacket* packet)
{

}

void ChatServer::Res_Message(DWORD64 sessionID, CPacket* packet)
{

}

void ChatServer::Recv_HeartBeat(DWORD64 sessionID, CPacket* packet)
{
    PLAYER* player;

    PacketFree(packet);

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        Disconnect(sessionID);
        return;
    }

    player->lastTime = currentTime;
}

int main()
{
    LogInit();
	g_ChatServer.Start(IP, PORT, 4, 4, true, MAX_CONNECT);

    timeBeginPeriod(1);

    for (;;) {
        g_ChatServer.Monitor();
        Sleep(1000);
    }
}


