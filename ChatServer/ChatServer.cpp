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

#define PORT 11601
#define MAX_CONNECT 10000

#define TIME_OUT 40000

#define GetMapID(sessionID) (sessionID & (PLAYER_MAP_MAX - 1));

CDump dump;
ChatServer g_ChatServer;
WCHAR IP[16] = L"0.0.0.0";

CTLSMemoryPool<PLAYER> g_playerPool;

WORD deltaSectorX[9] = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };
WORD deltaSectorY[9] = { -1, -1, -1, 0, 0, 0, 1, 1, 1 };

ChatServer::ChatServer()
{
    int cntX;
    int cntY;

    for (cntY = 0; cntY < SECTOR_Y_MAX; ++cntY) {
        for (cntX = 0; cntX < SECTOR_X_MAX; ++cntX) {
            InitializeSRWLock(&sectorLock[cntY][cntX]);
        }
    }

    for (cntX = 0; cntX < PLAYER_MAP_MAX; ++cntX) {
        InitializeSRWLock(&playerMapLock[cntX]);
    }
}

ChatServer::~ChatServer()
{
    isServerOn = false;
}


bool ChatServer::OnConnectionRequest(WCHAR* IP, DWORD Port)
{
	return true;
}

bool ChatServer::OnClientJoin(DWORD64 sessionID)
{
    SetTimeOut(sessionID, TIME_OUT);
    //임시로 무조건 승인중
    return true;
}

bool ChatServer::OnClientLeave(DWORD64 sessionID)
{
    JOB job;
    job.type = en_SERVER_DISCONNECT;
    job.sessionID = sessionID;
    
    jobQ.Enqueue(job);

    return true;
}

void ChatServer::OnRecv(DWORD64 sessionID, CPacket* packet)
{
    WORD type;

    *packet >> type;
   
    switch (type) {
    case en_PACKET_CS_CHAT_REQ_LOGIN:
    {
        Recv_Login(sessionID, packet);
    }
        break;

    case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE: 
    {
        Recv_SectorMove(sessionID, packet);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_MESSAGE:
    {
        Recv_Message(sessionID, packet);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
    {
        //지금 당장은 함수 Call의 이유가 없음
        //server->Recv_HeartBeat(job.sessionID, job.packet);
        PacketFree(packet);
    }
        break;

    default:
        Disconnect(sessionID);
    }
}

void ChatServer::OnTimeOut(DWORD64 sessionID)
{
    JOB job;

    job.type = en_SERVER_DISCONNECT;
    job.sessionID = sessionID;
    
    jobQ.Enqueue(job);
    OnError(timeOutCnt++, L"Time Out!!");
}

void ChatServer::OnError(int error, const WCHAR* msg)
{
    if (error != -1) {
        ErrorLog(error, msg);
    }
    else ErrorLog(msg);
}

void ChatServer::ThreadInit()
{
    isServerOn = true;
}

void ChatServer::ContentsMonitor()
{
    wprintf_s(L"========= CONTENTS ========== \n");
    wprintf_s(L"TimeOut : %llu || LogOut : %llu || Chat Recvd : %llu \n", timeOutCnt, logOutRecv, chatCnt);

    //sector용
    int cnt;
    int lim;
    int idx = 0;

    wprintf_s(L"Max Sectors : ");
    lim = 10;
    for (idx = 49; idx >= 0; --idx) {
        for (cnt = 0; cnt < min(sectorCnt[idx], lim); ++cnt) {
            wprintf_s(L"%d  ", idx);
        }
        lim -= sectorCnt[idx];
        if (lim <= 0) break;
    }
    wprintf_s(L"\n");

    wprintf_s(L"Min Sectors : ");
    lim = 10;
    for (idx = 0; idx < 50; ++idx) {
        for (cnt = 0; cnt < min(sectorCnt[idx], lim); ++cnt) {
            wprintf_s(L"%d  ", idx);
        }
        lim -= sectorCnt[idx];
        if (lim <= 0) break;
    }
    wprintf_s(L"\n");


}

void ChatServer::Recv_Login(DWORD64 sessionID, CPacket* packet)
{
    //packet 추출
    PLAYER* player = g_playerPool.Alloc();
    *packet >> player->accountNo;
    packet->GetData((char*)player->ID, 40);
    packet->GetData((char*)player->Nickname, 40);
    packet->GetData(player->sessionKey, 64);

    int mapID = GetMapID(sessionID);

    PacketFree(packet);
    //플레이어 생성 성공(추가 가능한 필터링 -> 아이디나 닉네임 규정 위반)
    if (playerMap[mapID].find(sessionID) == playerMap[mapID].end()) {
        //sector정보 초기화목적
        player->sectorX = SECTOR_X_MAX;
        player->sectorY = SECTOR_Y_MAX;
        Res_Login(player->accountNo, sessionID, 1);
        //player sector map에 삽입
        AcquireSRWLockExclusive(&playerMapLock[mapID]);
        playerMap[mapID].insert({ sessionID, player });
        ReleaseSRWLockExclusive(&playerMapLock[mapID]);
    }
    else {
        Res_Login(player->accountNo, sessionID, 0);
        Disconnect(sessionID);
        return;
    }
}

void ChatServer::Res_Login(INT64 accountNo, DWORD64 sessionID, BYTE isSuccess)
{
    CPacket* packet = PacketAlloc();
    
    *packet << (WORD)en_PACKET_SC_CHAT_RES_LOGIN << isSuccess << accountNo;

    SendPacket(sessionID, packet);
}

void ChatServer::Recv_SectorMove(DWORD64 sessionID, CPacket* packet)
{
    PLAYER* player;
    WORD oldSectorX;
    WORD oldSectorY;
    
    int mapID = GetMapID(sessionID);

    //find player        
    AcquireSRWLockShared(&playerMapLock[mapID]);
    if (playerMap[mapID].find(sessionID) != playerMap[mapID].end()) {
        player = playerMap[mapID][sessionID];
        ReleaseSRWLockShared(&playerMapLock[mapID]);
    }
    else {
        ReleaseSRWLockShared(&playerMapLock[mapID]);
        PacketFree(packet);
        Disconnect(sessionID);
        return;
    }

    oldSectorX = player->sectorX;
    oldSectorY = player->sectorY;

    //accountNO처리 한번더 고민
    *packet >> player->accountNo >> player->sectorX >> player->sectorY;

    PacketFree(packet);

    //oldSector 제거
    std::list<DWORD64>::iterator itr;
    //monitor용
    int listSize;
    
    for (;;) {
        if (TryAcquireSRWLockExclusive(&sectorLock[oldSectorY][oldSectorX]) == true) {
            if (TryAcquireSRWLockExclusive(&sectorLock[player->sectorY][player->sectorX]) == false) {
                ReleaseSRWLockExclusive(&sectorLock[oldSectorY][oldSectorX]);
            }
            else {
                break;
            }
        }
    }

    if (oldSectorY == SECTOR_Y_MAX || oldSectorX == SECTOR_X_MAX) {}
    else {
        for (itr = sectorList[oldSectorY][oldSectorX].begin(); itr != sectorList[oldSectorY][oldSectorX].end(); ++itr) {
            if (*itr == sessionID) {
                //mointor용
                listSize = sectorList[oldSectorY][oldSectorX].size();
                --sectorCnt[listSize];
                ++sectorCnt[listSize - 1];

                sectorList[oldSectorY][oldSectorX].erase(itr);
                break;
            }
        }
    }
    //newSector 삽입
    //mointor용
    listSize = sectorList[player->sectorY][player->sectorX].size();
    --sectorCnt[listSize];
    ++sectorCnt[listSize + 1];

    sectorList[player->sectorY][player->sectorX].push_back(sessionID);

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
    INT64 accountNo;
    WORD msgLen;
    WCHAR* msg;

    *packet >> accountNo >> msgLen;

    msg = new WCHAR[msgLen / 2];

    packet->GetData((char*)msg, msgLen);
    
    PacketFree(packet);

    //컨텐츠 모니터링용
    {
        InterlockedIncrement64((__int64*)&chatCnt);
        if (msg[0] == L'=') {
            InterlockedIncrement64((__int64*)&logOutRecv);
        }
    }

    Res_Message(sessionID, msg, msgLen);
}

void ChatServer::Res_Message(DWORD64 sessionID, WCHAR* msg, WORD len)
{
    PLAYER* player;
    CPacket* packet;

    int mapID = GetMapID(sessionID);

    //find player
    AcquireSRWLockShared(&playerMapLock[mapID]);
    if (playerMap[mapID].find(sessionID) != playerMap[mapID].end()) {
        player = playerMap[mapID][sessionID];
        ReleaseSRWLockShared(&playerMapLock[mapID]);
    }
    else {
        ReleaseSRWLockShared(&playerMapLock[mapID]);
        Disconnect(sessionID);
        return;
    }

    packet = PacketAlloc();

    *packet << (WORD)en_PACKET_SC_CHAT_RES_MESSAGE << player->accountNo;

    packet->PutData((char*)player->ID, 40);
    packet->PutData((char*)player->Nickname, 40);

    *packet << len;
    packet->PutData((char*)msg, len);

    delete msg;

    SendSectorAround(sessionID, packet);
}

void ChatServer::Recv_HeartBeat(DWORD64 sessionID, CPacket* packet)
{
    PLAYER* player;

    PacketFree(packet);

    int mapID = GetMapID(sessionID);

    //find player
    AcquireSRWLockShared(&playerMapLock[mapID]);
    if (playerMap[mapID].find(sessionID) != playerMap[mapID].end()) {
        player = playerMap[mapID][sessionID];
        ReleaseSRWLockShared(&playerMapLock[mapID]);
    }
    else {
        ReleaseSRWLockShared(&playerMapLock[mapID]);
        Disconnect(sessionID);
        return;
    }

}

void ChatServer::SendSectorAround(DWORD64 sessionID, CPacket* packet)
{
    PLAYER* player;
    WORD sectorY;
    WORD sectorX;
    char cntY;
    char cntX;
    char cnt;
    char inCnt;
    //디버그용
    PLAYER* temp;
    std::list<DWORD64>::iterator itr;

    int mapID = GetMapID(sessionID);

    //find player
    AcquireSRWLockShared(&playerMapLock[mapID]);
    if (playerMap[mapID].find(sessionID) != playerMap[mapID].end()) {
        player = playerMap[mapID][sessionID];
        ReleaseSRWLockShared(&playerMapLock[mapID]);
    }
    else {
        ReleaseSRWLockShared(&playerMapLock[mapID]);
        PacketFree(packet);
        Disconnect(sessionID);
        return;
    }

    //acquireLock구간
    for (cnt = 0; cnt < 9; ++cnt) {
        sectorX = player->sectorX + deltaSectorX[cnt];
        sectorY = player->sectorY + deltaSectorY[cnt];

        if (sectorX >= SECTOR_X_MAX)
            continue;
        if (sectorY >= SECTOR_Y_MAX)
            continue;

        if (TryAcquireSRWLockShared(&sectorLock[sectorY][sectorX]) == false) 
        {
            for (inCnt = 0; inCnt < cnt; ++inCnt) {
                sectorX = player->sectorX + deltaSectorX[inCnt];
                sectorY = player->sectorY + deltaSectorY[inCnt];

                if (sectorX >= SECTOR_X_MAX)
                    continue;
                if (sectorY >= SECTOR_Y_MAX)
                    continue;
                
                ReleaseSRWLockShared(&sectorLock[sectorY][sectorX]);
            }
            cnt = -1;
        }
    }


    //SendPacket 구간
    for (cnt = 0; cnt < 9; ++cnt) {
        sectorX = player->sectorX + deltaSectorX[cnt];
        sectorY = player->sectorY + deltaSectorY[cnt];

        if (sectorX >= SECTOR_X_MAX)
            continue;
        if (sectorY >= SECTOR_Y_MAX)
            continue;

        //packet addref처리
        packet->AddRef(sectorList[sectorY][sectorX].size());
        //각 session에 sendpacket
        for (itr = sectorList[sectorY][sectorX].begin(); itr != sectorList[sectorY][sectorX].end(); ++itr) {
            SendPacket(*itr, packet);
        }
    }


    //release Lock 구간
    for (cnt = 0; cnt < 9; ++cnt) {
        sectorX = player->sectorX + deltaSectorX[cnt];
        sectorY = player->sectorY + deltaSectorY[cnt];

        if (sectorX >= SECTOR_X_MAX)
            continue;
        if (sectorY >= SECTOR_Y_MAX)
            continue;

        ReleaseSRWLockShared(&sectorLock[sectorY][sectorX]);
    }

    //본인 포함 send로 packetRef 1 초과
    PacketFree(packet);
}

void ChatServer::DisconnectProc(DWORD64 sessionID)
{
    PLAYER* player;
    std::list<DWORD64>::iterator itr;

    int mapID = GetMapID(sessionID);

    //find player
    AcquireSRWLockShared(&playerMapLock[mapID]);
    if (playerMap[mapID].find(sessionID) != playerMap[mapID].end()) {
        player = playerMap[mapID][sessionID];
        ReleaseSRWLockShared(&playerMapLock[mapID]);
    }
    else {
        ReleaseSRWLockShared(&playerMapLock[mapID]);
        return;
    }
    //sectorList에서 제거

    //mointor용
    int listSize;
    if (player->sectorY == SECTOR_Y_MAX || player->sectorX == SECTOR_X_MAX) {}
    else {
        AcquireSRWLockExclusive(&sectorLock[player->sectorY][player->sectorX]);
        for (itr = sectorList[player->sectorY][player->sectorX].begin(); itr != sectorList[player->sectorY][player->sectorX].end(); ++itr) {
            if (*itr == sessionID) {
                //mointor용
                listSize = sectorList[player->sectorY][player->sectorX].size();
                --sectorCnt[listSize];
                ++sectorCnt[listSize - 1];

                sectorList[player->sectorY][player->sectorX].erase(itr);
                break;
            }
        }
        ReleaseSRWLockExclusive(&sectorLock[player->sectorY][player->sectorX]);
    }

    //playerMap[mapID]에서 제거
    AcquireSRWLockExclusive(&playerMapLock[mapID]);
    playerMap[mapID].erase(sessionID);
    ReleaseSRWLockExclusive(&playerMapLock[mapID]);

    g_playerPool.Free(player);
}

int main()
{
    LogInit();
	g_ChatServer.Start(IP, PORT, 4, 4, true, MAX_CONNECT);
    g_ChatServer.ThreadInit();

    timeBeginPeriod(1);

    for (;;) {
        //console화면 지워지니까 contentsMontior는 그냥 로그만 추가하세요
        g_ChatServer.Monitor();
        g_ChatServer.ContentsMonitor();
        Sleep(1000);
    }
}


