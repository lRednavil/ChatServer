#pragma once

#include <list>
#include <unordered_map>
#include <set>

#define SECTOR_X_MAX 50
#define SECTOR_Y_MAX 50

//player랑 job은 별도 헤더로 빠질수도..?
struct PLAYER {
	INT64 accountNo;
	WCHAR ID[20];
	WCHAR Nickname[20];
	char sessionKey[64];

	WORD sectorX;
	WORD sectorY;

	DWORD lastTime;
};

struct JOB {
	BYTE type;
	DWORD64 sessionID;
	CPacket* packet;
};

class ChatServer : public CNetServer
{
public:
	ChatServer();
	~ChatServer();

	void ThreadInit();
	
	void ContentsMonitor();

//virtual함수 영역
private:
	//accept 직후, IP filterinig 등의 목적
	bool OnConnectionRequest(WCHAR* IP, DWORD Port);
	//return false; 시 클라이언트 거부.
	//return true; 시 접속 허용
	bool OnClientJoin(DWORD64 sessionID);
	bool OnClientLeave(DWORD64 sessionID);

	//message 분석 역할
	void OnRecv(DWORD64 sessionID, CPacket* packet);
	
	void OnTimeOut(DWORD64 sessionID);

	void OnError(int error, const WCHAR* msg);

	//모든 Recv함수는 PacketFree할 것
	//playerMap 삽입
	void Recv_Login(DWORD64 sessionID, CPacket* packet);
	void Res_Login(INT64 accountNo, DWORD64 sessionID, BYTE isSuccess);

	void Recv_SectorMove(DWORD64 sessionID, CPacket* packet);
	void Res_SectorMove(PLAYER* player, DWORD64 sessionID);

	void Recv_Message(DWORD64 sessionID, CPacket* packet);
	void Res_Message(DWORD64 sessionID, WCHAR* msg, WORD len);

	void Recv_HeartBeat(DWORD64 sessionID, CPacket* packet);

	void SendSectorAround(DWORD64 sessionID, CPacket* packet);
	void DisconnectProc(DWORD64 sessionID);


private:
	CLockFreeQueue<JOB> jobQ;
	//sessionID기준 탐색
	std::list<DWORD64> sectorList[SECTOR_Y_MAX][SECTOR_X_MAX];
	//sessionID기준 탐색
	std::unordered_map<INT64, PLAYER*> playerMap;

	DWORD64 timeOutCnt = 0;
	//메세지에서 L'='수신 카운트
	DWORD64 logOutRecv = 0;
	DWORD64 chatCnt = 0;

	int sectorCnt[50] = { 2500, };

	bool isServerOn;
};

