#include "stdafx.h"

CTLSMemoryPool<PLAYER> g_playerPool;
CTLSMemoryPool<JOB> g_jobPool;

ChatServer::ChatServer()
{
    updateEvent = (HANDLE)CreateEvent(NULL, TRUE, FALSE, NULL);
    monitorClient = new CMonitorClient;
}

ChatServer::~ChatServer()
{
    isServerOn = false;
    CloseHandle(updateEvent);
    WaitForSingleObject(hThreads, INFINITE);
    delete monitorClient;
}


void ChatServer::Init()
{
    WCHAR IP[16] = L"0.0.0.0";
    DWORD PORT;
    DWORD createThreads;
    DWORD runningThreads;
    bool isNagle;
    DWORD maxConnect;
    DWORD snap;

    GetPrivateProfileString(L"ChatServer", L"IP", L"0.0.0.0", IP, 16, L".//ServerSettings.ini");
    PORT = GetPrivateProfileInt(L"ChatServer", L"PORT", NULL, L".//ServerSettings.ini");
    createThreads = GetPrivateProfileInt(L"ChatServer", L"CreateThreads", NULL, L".//ServerSettings.ini");
    runningThreads = GetPrivateProfileInt(L"ChatServer", L"RunningThreads", NULL, L".//ServerSettings.ini");
    isNagle = GetPrivateProfileInt(L"ChatServer", L"isNagle", NULL, L".//ServerSettings.ini");
    maxConnect = GetPrivateProfileInt(L"ChatServer", L"MaxConnect", NULL, L".//ServerSettings.ini");
    snap = GetPrivateProfileInt(L"ChatServer", L"SnapLatency", 4, L".//ServerSettings.ini");

    if ((PORT * createThreads * runningThreads * maxConnect * snap) == 0) {
        _FILE_LOG(LOG_LEVEL_ERROR, L"INIT_LOG", L"INVALID ARGUMENTS or No ini FILE");
        CRASH();
    }

    Start(IP, PORT, createThreads, runningThreads, isNagle, maxConnect, snap);
    ThreadInit();

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
    //임시로 무조건 승인중
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
        //job->type = en_PACKET_CS_CHAT_REQ_HEARTBEAT;
        //job->sessionID = sessionID;
        //job->packet = packet;

        //jobQ.Enqueue(job);
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

void ChatServer::OnTimeOut(DWORD64 sessionID, int reason)
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
            PROFILE_START(_Update);

            //쉬게 할 방법 추가 고민
            if (jobQ.Dequeue(&job) == false) {
                ResetEvent(updateEvent);
                continue;
            }

            updateCnt++;

            switch (job->type) {
            case en_PACKET_CS_CHAT_REQ_LOGIN:
            {
                PROFILE_START(Login);
                Recv_Login(job->sessionID, job->packet);
            }
            break;

            case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
            {
                PROFILE_START(SectorMove);
                Recv_SectorMove(job->sessionID, job->packet);
            }
            break;

            case en_PACKET_CS_CHAT_REQ_MESSAGE:
            {
                PROFILE_START(RecvMsg);
                Recv_Message(job->sessionID, job->packet);
            }
            break;

            case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
            {
                //지금 당장은 함수 Call의 이유가 없음
                //Recv_HeartBeat(job->sessionID, job->packet);
            }
            break;
            case en_SERVER_DISCONNECT:
            {
                PROFILE_START(DisconnectProc);
                DisconnectProc(job->sessionID);
            }
            break;
            default:
                Disconnect(job->sessionID);
            }

            if (job->packet != NULL) {
                PROFILE_START(PacketFree);
                PacketFree(job->packet);
            }

            g_jobPool.Free(job);
        }
    }

}

void ChatServer::ThreadInit()
{
    isServerOn = true;

    hThreads = (HANDLE)_beginthreadex(NULL, 0, UpdateThread, this, NULL, 0);
}

void ChatServer::ContentsMonitor()
{
    if (isServerOn == false) return;
    int tv = time(NULL);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, 1, tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, processMonitor.ProcessTotal(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, processMonitor.ProcessPrivateBytes() / 1024 / 1024, tv); // Mbyte단위로
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_SESSION, GetSessionCount(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_PLAYER, playerMap.size(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, updateCnt - lastUpdateCnt, tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, GetPacketPoolUse(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, jobQ.GetSize(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_CHAT_ACCEPT_TPS, GetAcceptTPS(), tv);

    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_CPU_TOTAL, processorMonitor.ProcessorTotal(), tv);
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_NONPAGED_MEMORY, processorMonitor.NonPagedMemory() / 1024 / 1024, tv); // Mbytes단위로
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_RECV, processorMonitor.EthernetRecvTPS() / 1024, tv); //Kbytes단위로
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_NETWORK_SEND, processorMonitor.EthernetSendTPS() / 1024, tv); //Kbytes단위로
    monitorClient->UpdateMonitorInfo(dfMONITOR_DATA_TYPE_MONITOR_AVAILABLE_MEMORY, processorMonitor.AvailableMemory(), tv); 


    processMonitor.UpdateProcessTime();
    processorMonitor.UpdateHardwareTime();

    wprintf_s(L"========= CONTENTS ========== \n");
    wprintf_s(L"Update TPS : %llu || Left Update Message : %llu \n", updateCnt - lastUpdateCnt, jobQ.GetSize());
    wprintf_s(L"TimeOut : %llu || LogOut : %llu || Chat Recvd : %llu \n", timeOutCnt, logOutRecv, chatCnt);

    lastUpdateCnt = updateCnt;

    //sector용
    int cnt;
    int lim;
    int idx = 0;

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
        Res_Login(player->accountNo, sessionID, 1);
        //player sector map에 삽입
        playerMap.insert({ sessionID, player });
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
    PROFILE_START(SendAround);
    PLAYER* player = playerMap[sessionID];
    WORD sectorY;
    WORD sectorX;
    char cntY;
    char cntX;

    std::list<DWORD64>::iterator itr;

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

                sectorList[player->sectorY][player->sectorX].erase(itr);
                break;
            }
        }
    }

    //playerMap에서 제거
    playerMap.erase(sessionID);

    g_playerPool.Free(player);
}
