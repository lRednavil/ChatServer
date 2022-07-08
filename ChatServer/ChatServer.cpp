#include "stdafx.h"
#include "ChatServer.h"

#include "cpp_redis/cpp_redis"

CTLSMemoryPool<PLAYER> g_playerPool;
CTLSMemoryPool<JOB> g_jobPool;
CTLSMemoryPool<REDIS_JOB> g_redisJobPool;

ChatServer::ChatServer()
{
    updateEvent = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);
    monitorClient = new CMonitorClient;

    redisJobQ = new CLockFreeQueue<REDIS_JOB*>;
    redisTLS = TlsAlloc();
    redisEvent = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);
}

ChatServer::~ChatServer()
{
    isServerOn = false;
    CloseHandle(updateEvent);
    WaitForMultipleObjects(updateThreadCnt, hUpdateThreads, true, INFINITE);
    delete[] hUpdateThreads;
    delete monitorClient;

    //redis
    SetEvent(redisEvent);
    WaitForMultipleObjects(redisThreadCnt, hRedis, true, INFINITE);
    delete[] hRedis;
    delete redisJobQ;
    TlsFree(redisTLS);
    CloseHandle(redisEvent);
}


void ChatServer::Init()
{
    WCHAR IP[16] = L"0.0.0.0";
    DWORD PORT;
    DWORD createThreads;
    DWORD runningThreads;
    bool isNagle;
    DWORD maxConnect;

    int updateThreads;
    int redisThreads;

    GetPrivateProfileString(L"ChatServer", L"IP", L"0.0.0.0", IP, 16, L".//ServerSettings.ini");
    PORT = GetPrivateProfileInt(L"ChatServer", L"PORT", NULL, L".//ServerSettings.ini");
    createThreads = GetPrivateProfileInt(L"ChatServer", L"CreateThreads", NULL, L".//ServerSettings.ini");
    runningThreads = GetPrivateProfileInt(L"ChatServer", L"RunningThreads", NULL, L".//ServerSettings.ini");
    isNagle = GetPrivateProfileInt(L"ChatServer", L"isNagle", NULL, L".//ServerSettings.ini");
    maxConnect = GetPrivateProfileInt(L"ChatServer", L"MaxConnect", NULL, L".//ServerSettings.ini");

    updateThreads = GetPrivateProfileInt(L"ChatServer", L"UpdateThreads", NULL, L".//ServerSettings.ini");
    redisThreads = GetPrivateProfileInt(L"ChatServer", L"RedisThreads", NULL, L".//ServerSettings.ini");

    if ((PORT * createThreads * runningThreads * maxConnect * updateThreads) == 0) {
        _FILE_LOG(LOG_LEVEL_ERROR, L"INIT_LOG", L"INVALID ARGUMENTS or No ini FILE");
        CRASH();
    }

    Start(IP, PORT, createThreads, runningThreads, isNagle, maxConnect);
    ThreadInit(updateThreads, redisThreads);

    monitorClient->Init();
}

void ChatServer::OnStop()
{

}

bool ChatServer::OnConnectionRequest(WCHAR* IP, DWORD Port)
{
    return true;
}

bool ChatServer::OnClientJoin(DWORD64 sessionID)
{
    SetTimeOut(sessionID, 40000);
    //�ӽ÷� ������ ������
    return true;
}

bool ChatServer::OnClientLeave(DWORD64 sessionID)
{
    JOB* job = g_jobPool.Alloc();
    job->type = en_SERVER_DISCONNECT;
    job->sessionID = sessionID;
    job->packet = NULL;

    jobQ.Enqueue(job);
    SetEvent(updateEvent);

    return true;
}

void ChatServer::OnRecv(DWORD64 sessionID, CPacket* packet)
{
    WORD type;
    JOB* job = g_jobPool.Alloc();

    *packet >> type;

    switch (type) {
    case en_PACKET_CS_CHAT_REQ_LOGIN:
    {
        job->type = en_PACKET_CS_CHAT_REQ_LOGIN;
        job->sessionID = sessionID;
        job->packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
    {
        job->type = en_PACKET_CS_CHAT_REQ_SECTOR_MOVE;
        job->sessionID = sessionID;
        job->packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_MESSAGE:
    {
        job->type = en_PACKET_CS_CHAT_REQ_MESSAGE;
        job->sessionID = sessionID;
        job->packet = packet;

        jobQ.Enqueue(job);
    }
    break;

    case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
    {
        g_jobPool.Free(job);
    }
    break;

    default:
        _FILE_LOG(LOG_LEVEL_ERROR, L"ContentsLog", L"Packet Type Error");
        PacketFree(packet);
        g_jobPool.Free(job);
        Disconnect(sessionID);
    }

    SetEvent(updateEvent);
}

void ChatServer::OnTimeOut(DWORD64 sessionID)
{
    JOB* job = g_jobPool.Alloc();

    job->type = en_SERVER_DISCONNECT;
    job->sessionID = sessionID;
    job->packet = NULL;

    jobQ.Enqueue(job);
    OnError(timeOutCnt++, L"Time Out!!");
}

void ChatServer::OnError(int error, const WCHAR* msg)
{
    //_LOG(LOG_LEVEL_ERROR, msg);
}

unsigned int __stdcall ChatServer::UpdateThread(void* arg)
{
    ChatServer* server = (ChatServer*)arg;
    server->_UpdateThread();

    return 0;
}

void ChatServer::_UpdateThread()
{
    JOB* job;

    while (isServerOn) {
        {
            WaitForSingleObject(updateEvent, INFINITE);

            //���� �� ��� �߰� ���
            if (jobQ.Dequeue(&job) == false) {
                ResetEvent(updateEvent);
                continue;
            }

            updateCnt++;

            switch (job->type) {
            case en_PACKET_CS_CHAT_REQ_LOGIN:
            {
                Recv_Login(job->sessionID, job->packet);
            }
            break;

            case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
            {
                Recv_SectorMove(job->sessionID, job->packet);
            }
            break;

            case en_PACKET_CS_CHAT_REQ_MESSAGE:
            {
                Recv_Message(job->sessionID, job->packet);
            }
            break;

            case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
            {
                //���� ������ �Լ� Call�� ������ ����
            }
            break;
            case en_SERVER_DISCONNECT:
            {
                DisconnectProc(job->sessionID);
            }
            break;
            default:
                Disconnect(job->sessionID);
            }

            if (job->packet != NULL) {
                PacketFree(job->packet);
            }

            g_jobPool.Free(job);
        }
    }

}

unsigned int __stdcall ChatServer::RedisThread(void* arg)
{
    ChatServer* server = (ChatServer*)arg;
    server->_RedisThread();

    return 0;
}

void ChatServer::_RedisThread()
{
    REDIS_JOB* job;

    bool loginRes;

    WORD version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);

    cpp_redis::client* redis = new cpp_redis::client;
    TlsSetValue(redisTLS, redis);

    redis->connect("10.0.2.2", 6379U, nullptr, 0U, 0, 0U);

    std::string chatKey;
    std::string value;

    while (isServerOn) {
        WaitForSingleObject(redisEvent, INFINITE);

        if (redisJobQ->Dequeue(&job) == false) {
            ResetEvent(redisEvent);
            continue;
        }

        chatKey = std::to_string(job->player->accountNo) + ".Chat";

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

        if (loginRes) {
            Res_Login(job->player->accountNo, job->sessionID, loginRes);
        }
        else {
            Res_Login(job->player->accountNo, job->sessionID, loginRes);
            Disconnect(job->sessionID);
        }

        g_redisJobPool.Free(job);
    }

    redis->flushall();
}


void ChatServer::PutRedisJob(DWORD64 sessionID, PLAYER* player)
{
    REDIS_JOB* job = g_redisJobPool.Alloc();

    job->sessionID = sessionID;
    job->player = player;

    redisJobQ->Enqueue(job);
    SetEvent(redisEvent);
}

void ChatServer::ThreadInit(int updateCnt, int redisCnt)
{
    int idx;

    isServerOn = true;

    updateThreadCnt = updateCnt;
    redisThreadCnt = redisCnt;

    hUpdateThreads = new HANDLE[updateCnt];
    hRedis = new HANDLE[redisCnt];

    for (idx = 0; idx < updateCnt; idx++) {
        hUpdateThreads[idx] = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, this, NULL, 0);
    }

    for (idx = 0; idx < redisCnt; idx++) {
        hRedis[idx] = (HANDLE)_beginthreadex(NULL, 0, RedisThread, this, NULL, 0);
    }
}

void ChatServer::ContentsMonitor()
{
    if (isServerOn == false) return;
    int tv = time(NULL);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, 1, tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, processMonitor.ProcessTotal(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, processMonitor.ProcessPrivateBytes() / 1024 / 1024, tv); // Mbyte������
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SESSION, GetSessionCount(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_PLAYER, playerMap.size(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, updateCnt - lastUpdateCnt, tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, GetPacketPoolUse(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, jobQ.GetSize(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_ACCEPT_TPS, GetAcceptTPS(), tv);

    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, processorMonitor.ProcessorTotal(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, processorMonitor.NonPagedMemory() / 1024 / 1024, tv); // Mbytes������
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, processorMonitor.EthernetRecvTPS() / 1024, tv); //Kbytes������
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, processorMonitor.EthernetSendTPS() / 1024, tv); //Kbytes������
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, processorMonitor.AvailableMemory(), tv); 


    processMonitor.UpdateProcessTime();
    processorMonitor.UpdateHardwareTime();

    wprintf_s(L"========= SERVER INFO ========== \n");
    wprintf_s(L"Total Accept : %llu \n", GetTotalAccept());

    wprintf_s(L"========= CONTENTS ========== \n");
    wprintf_s(L"Update TPS : %llu || Left Update Message : %llu \n", updateCnt - lastUpdateCnt, jobQ.GetSize());
    wprintf_s(L"TimeOut : %llu || LogOut : %llu || Chat Recvd : %llu \n", timeOutCnt, logOutRecv, chatCnt);

    lastUpdateCnt = updateCnt;

    //sector��
    int cnt;
    int lim;
    int idx = 0;

}

void ChatServer::Recv_Login(DWORD64 sessionID, CPacket* packet)
{
    //packet ����
    PLAYER* player = g_playerPool.Alloc();
    *packet >> player->accountNo;
    packet->GetData((char*)player->ID, 40);
    packet->GetData((char*)player->Nickname, 40);
    packet->GetData(player->sessionKey, 64);

    //�÷��̾� ���� ����(�߰� ������ ���͸� -> ���̵� �г��� ���� ����)
    if (playerMap.find(sessionID) == playerMap.end()) {
        //sector���� �ʱ�ȭ����
        player->sectorX = SECTOR_X_MAX;
        player->sectorY = SECTOR_Y_MAX;
        //player sector map�� ����
        playerMap.insert({ sessionID, player });
        
        if (redisThreadCnt == 0)
            Res_Login(player->accountNo, sessionID, 1);
        else
            PutRedisJob(sessionID, player);
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

    //accountNOó�� �ѹ��� ���
    *packet >> accountNo >> newSectorX >> newSectorY;

    //contents���
    if (player->accountNo != accountNo) {
        Disconnect(sessionID);
        return;
    }

    //oldSector ����
    std::list<DWORD64>::iterator itr;
    //monitor��
    int listSize;
    if (player->sectorY == SECTOR_Y_MAX || player->sectorX == SECTOR_X_MAX) {}
    else {
        for (itr = sectorList[player->sectorY][player->sectorX].begin(); itr != sectorList[player->sectorY][player->sectorX].end(); ++itr) {
            if (*itr == sessionID) {
                sectorList[player->sectorY][player->sectorX].erase(itr);
                break;
            }
        }
    }
    //newSector ����
    //contents ���
    if (newSectorX >= SECTOR_X_MAX || newSectorY >= SECTOR_Y_MAX)
    {
        Disconnect(sessionID);
        return;
    }

    sectorList[newSectorY][newSectorX].emplace_back(sessionID);

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

    //contents���
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

    //������ ����͸���
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
    PROFILE_START(SendAround);
    PLAYER* player = playerMap[sessionID];
    WORD sectorY;
    WORD sectorX;
    char cntY;
    char cntX;

    std::list<DWORD64>::iterator itr;

    for (cntY = -1; cntY <= 1; ++cntY) {
        sectorY = player->sectorY + cntY;
        //WORD�̹Ƿ� -1 => 65535
        if (sectorY >= SECTOR_Y_MAX)
            continue;

        for (cntX = -1; cntX <= 1; ++cntX) {
            sectorX = player->sectorX + cntX;
            if (sectorX >= SECTOR_X_MAX)
                continue;

            //packet addrefó��
            packet->AddRef(sectorList[sectorY][sectorX].size());
            //�� session�� sendpacket
            for (itr = sectorList[sectorY][sectorX].begin(); itr != sectorList[sectorY][sectorX].end(); ++itr) {
                SendPacket(*itr, packet);
            }
        }
    }

    //���� ���� send�� packetRef 1 �ʰ�
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
    //sectorList���� ����

    //mointor��
    int listSize;
    if (player->sectorY >= SECTOR_Y_MAX || player->sectorX >= SECTOR_X_MAX) {}
    else {
        for (itr = sectorList[player->sectorY][player->sectorX].begin(); itr != sectorList[player->sectorY][player->sectorX].end(); ++itr) {
            if (*itr == sessionID) {
                //mointor��
                listSize = sectorList[player->sectorY][player->sectorX].size();

                sectorList[player->sectorY][player->sectorX].erase(itr);
                break;
            }
        }
    }

    //playerMap���� ����
    playerMap.erase(sessionID);

    g_playerPool.Free(player);
}
