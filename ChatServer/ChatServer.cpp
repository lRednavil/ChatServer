#include <cpp_redis/cpp_redis>

#include <string>
#include <vector>

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
#include "Logging.h"

#pragma comment (lib, "cpp_redis.lib")
#pragma comment (lib, "tacopie.lib")

#pragma comment (lib, "NetworkLibrary")
#pragma comment (lib, "Winmm")

#define PORT 11701
#define MAX_CONNECT 10000

#define TIME_OUT 40000

CDump dump;
ChatServer g_ChatServer;
WCHAR IP[16] = L"0.0.0.0";

char redisIP[16] = "10.0.2.2";

CTLSMemoryPool<PLAYER> g_playerPool;
CTLSMemoryPool<REDIS_JOB> redisJobPool; 

ChatServer::ChatServer()
{
    redisTLS = TlsAlloc();
    redisEvent[0] = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);
    redisEvent[1] = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);

    redisJobQ = new CLockFreeQueue<REDIS_JOB*>;

    for (int cnt = 0; cnt < REDIS_THREAD; cnt++) {
        hRedis[cnt] = (HANDLE)_beginthreadex(NULL, 0, RedisWork, this, 0, NULL);
    }
}

ChatServer::~ChatServer()
{
    isServerOn = false;

    SetEvent(redisEvent[1]);
    WaitForMultipleObjects(REDIS_THREAD, hRedis, true, INFINITE);

    delete redisJobQ;

    TlsFree(redisTLS);

    WaitForSingleObject(hThreads, INFINITE);
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
    job.packet = NULL;
    
    jobQ.Enqueue(job);

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
        job.type = en_PACKET_CS_CHAT_REQ_SECTOR_MOVE;
        job.sessionID = sessionID;
        job.packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_MESSAGE:
    {
        job.type = en_PACKET_CS_CHAT_REQ_MESSAGE;
        job.sessionID = sessionID;
        job.packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
    {
        //job.type = en_PACKET_CS_CHAT_REQ_HEARTBEAT;
        //job.sessionID = sessionID;
        //job.packet = packet;

        //jobQ.Enqueue(job);
    }
        break;

    default:
        //_FILE_LOG(LOG_LEVEL_ERROR, L"ContentsLog", L"Packet Type Error");
        PacketFree(packet);
        Disconnect(sessionID);
    }
}

void ChatServer::OnTimeOut(DWORD64 sessionID)
{
    JOB job;

    job.type = en_SERVER_DISCONNECT;
    job.sessionID = sessionID;
    job.packet = NULL;
    
    jobQ.Enqueue(job);
    OnError(timeOutCnt++, L"Time Out!!");
}

void ChatServer::OnError(int error, const WCHAR* msg)
{
    _LOG(LOG_LEVEL_ERROR, msg);
}

unsigned int __stdcall ChatServer::_UpdateThread(void* arg)
{
    ChatServer* server = (ChatServer*)arg;
    JOB job;

    while (server->isServerOn) {
        //쉬게 할 방법 추가 고민
        if (server->jobQ.Dequeue(&job) == false) {
            Sleep(0);
            continue;
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
            //지금 당장은 함수 Call의 이유가 없음
            //server->Recv_HeartBeat(job.sessionID, job.packet);
        }
        break;
        case en_SERVER_DISCONNECT:
        {
            server->DisconnectProc(job.sessionID);
        }
        break;
        default:
            server->Disconnect(job.sessionID);
        }

        if (job.packet != NULL) {
            server->PacketFree(job.packet);
        }
    }

    return 0;
}

void ChatServer::ThreadInit()
{
    isServerOn = true;

    hThreads = (HANDLE)_beginthreadex(NULL, 0, _UpdateThread, this, NULL, 0);
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

    //플레이어 생성 성공(추가 가능한 필터링 -> 아이디나 닉네임 규정 위반)
    if (playerMap.find(sessionID) == playerMap.end()) {
        //sector정보 초기화목적
        player->sectorX = SECTOR_X_MAX;
        player->sectorY = SECTOR_Y_MAX;
        //player sector map에 삽입
        playerMap.insert({ sessionID, player });

        PutRedisJob(sessionID, player);
    }
    else {
        Res_Login(player->accountNo, sessionID, 0);
        g_playerPool.Free(player);
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
    INT64 accountNo;
    WORD newSectorX;
    WORD newSectorY;

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        Disconnect(sessionID);
        return;
    }

    //accountNO처리 한번더 고민
    *packet >> accountNo >> newSectorX >> newSectorY;
    
    //contents방어
    if (player->accountNo != accountNo) {
        Disconnect(sessionID);
        return;
    }

    //oldSector 제거
    std::list<DWORD64>::iterator itr;
    //monitor용
    int listSize;
    if (player->sectorY == SECTOR_Y_MAX || player->sectorX == SECTOR_X_MAX) {}
    else {
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
    }
    //newSector 삽입
    //contents 방어
    if (newSectorX >= SECTOR_X_MAX || newSectorY >= SECTOR_Y_MAX)
    {
        Disconnect(sessionID);
        return;
    }
    
    //mointor용
    listSize = sectorList[newSectorY][newSectorX].size();
    --sectorCnt[listSize];
    ++sectorCnt[listSize + 1];

    sectorList[newSectorY][newSectorX].push_back(sessionID);

    player->sectorX = newSectorX;
    player->sectorY = newSectorY;

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
    PLAYER* player;
    INT64 accountNo;
    WORD msgLen;
    WCHAR msg[512];

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        Disconnect(sessionID);
        return;
    }


    *packet >> accountNo >> msgLen;

    //contents방어
    if (player->accountNo != accountNo) {
        Disconnect(sessionID);
        return;
    }
    if (player->sectorX == SECTOR_X_MAX || player->sectorY == SECTOR_Y_MAX) {
        Disconnect(sessionID);
        return;
    }
    if (packet->GetDataSize() < msgLen) {
        Disconnect(sessionID);
        return;
    }
    

    packet->GetData((char*)msg, msgLen);

    //컨텐츠 모니터링용
    {
        ++chatCnt;
        if (msg[0] == L'=') {
            ++logOutRecv;
        }
    }   

    Res_Message(sessionID, msg, msgLen);
}

void ChatServer::Res_Message(DWORD64 sessionID, WCHAR* msg, WORD len)
{
    PLAYER* player;
    CPacket* packet;

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        Disconnect(sessionID);
        return;
    }

    packet = PacketAlloc();

    *packet << (WORD)en_PACKET_SC_CHAT_RES_MESSAGE << player->accountNo;

    packet->PutData((char*)player->ID, 40);
    packet->PutData((char*)player->Nickname, 40);

    *packet << len;
    packet->PutData((char*)msg, len);

    SendSectorAround(sessionID, packet);
}

void ChatServer::Recv_HeartBeat(DWORD64 sessionID, CPacket* packet)
{
    PLAYER* player;

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
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
    //디버그용
    PLAYER* temp;
    std::list<DWORD64>::iterator itr;

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        Disconnect(sessionID);
        return;
    }

    for (cntY = -1; cntY <= 1; ++cntY) {
        sectorY = player->sectorY + cntY;
        //WORD이므로 -1 => 65535
        if (sectorY >= SECTOR_Y_MAX)
            continue;

        for (cntX = -1; cntX <= 1; ++cntX) {
            sectorX = player->sectorX + cntX;
            if (sectorX >= SECTOR_X_MAX)
                continue;

            //packet addref처리
            packet->AddRef(sectorList[sectorY][sectorX].size());
            //각 session에 sendpacket
            for (itr = sectorList[sectorY][sectorX].begin(); itr != sectorList[sectorY][sectorX].end(); ++itr) {
                SendPacket(*itr, packet);
            }
        }
    }

    //본인 포함 send로 packetRef 1 초과
    PacketFree(packet);
}

void ChatServer::DisconnectProc(DWORD64 sessionID)
{
    PLAYER* player;
    std::list<DWORD64>::iterator itr;

    //find player
    if (playerMap.find(sessionID) != playerMap.end()) {
        player = playerMap[sessionID];
    }
    else {
        return;
    }
    //sectorList에서 제거

    //mointor용
    int listSize;
    if (player->sectorY >= SECTOR_Y_MAX || player->sectorX >= SECTOR_X_MAX) {}
    else {
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
    }

    //playerMap에서 제거
    playerMap.erase(sessionID);
    
    g_playerPool.Free(player);
}

unsigned int __stdcall ChatServer::RedisWork(void* arg)
{
    ChatServer* server = (ChatServer*)arg;
    REDIS_JOB* job;

    bool loginRes;

    DWORD redisTime;
    DWORD deltaTime;

    WORD version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);

    cpp_redis::client* redis = new cpp_redis::client;
    TlsSetValue(server->redisTLS, redis);

    redis->connect(redisIP, 6379U, nullptr, 0U, 0, 0U);

    std::string chatKey;
    std::string value;

    for (;;) {
        //1인 경우 exit
        if (WaitForMultipleObjects(2, server->redisEvent, false, INFINITE) - WAIT_OBJECT_0 == 1) {
            break;
        }

        if (server->redisJobQ->Dequeue(&job) == false) {
            ResetEvent(server->redisEvent[0]);
            continue;
        }

        chatKey = std::to_string(job->player->accountNo) + ".Chat";

        redisTime = timeGetTime();
        std::future<cpp_redis::reply> redisFuture;
        redisFuture = redis->get(chatKey);
        redis->sync_commit(std::chrono::milliseconds(50));

        cpp_redis::reply rep = redisFuture.get();
        if (rep.is_string()) {
            loginRes = memcmp(rep.as_string().c_str(), job->player->sessionKey, 64) == 0;
        }
        else {
            loginRes = false;
        }

        if (loginRes) {
            redis->del({ chatKey });
            redis->sync_commit(std::chrono::milliseconds(50));
        }

        deltaTime = timeGetTime() - redisTime;

        if (loginRes) {
            server->Res_Login(job->player->accountNo, job->sessionID, loginRes);
        }
        else {
            server->Res_Login(job->player->accountNo, job->sessionID, loginRes);
            server->Disconnect(job->sessionID);
        }

        redisJobPool.Free(job);
    }

    redis->flushall();
    WSACleanup();

    return 0;
}

void ChatServer::PutRedisJob(DWORD64 sessionID, PLAYER* player)
{
    REDIS_JOB* job = redisJobPool.Alloc();

    job->sessionID = sessionID;
    job->player = player;

    redisJobQ->Enqueue(job);
    SetEvent(redisEvent[0]);
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


