#pragma once

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <set>

#define SECTOR_X_MAX 50
#define SECTOR_Y_MAX 50

class CMonitorClient;

//player�� job�� ���� ����� ��������..?
struct PLAYER {
	INT64 accountNo;
	WCHAR ID[20];
	WCHAR Nickname[20];
	char sessionKey[64];

	WORD sectorX;
	WORD sectorY;
};

struct JOB {
	BYTE type;
	DWORD64 sessionID;
	CPacket* packet;
};

struct REDIS_JOB {
	DWORD64 sessionID;
	PLAYER* player;
};

class ChatServer : public CNetServer
{
public:
	ChatServer();
	~ChatServer();

	void Init();
	void ThreadInit(int updateCnt, int redisCnt);
	
	void ContentsMonitor();

//virtual�Լ� ����
private:
	//accept ����, IP filterinig ���� ����
	bool OnConnectionRequest(WCHAR* IP, DWORD Port);
	//return false; �� Ŭ���̾�Ʈ �ź�.
	//return true; �� ���� ���
	bool OnClientJoin(DWORD64 sessionID);
	bool OnClientLeave(DWORD64 sessionID);

	//message �м� ����
	void OnRecv(DWORD64 sessionID, CPacket* packet);
	
	void OnTimeOut(DWORD64 sessionID);

	void OnError(int error, const WCHAR* msg);

	void OnStop();
//chatserver ���� �Լ� ����
	static unsigned int __stdcall UpdateThread(void* arg);
	void _UpdateThread();

	//Redis ����
	static unsigned int __stdcall RedisThread(void* arg);
	void _RedisThread();
	
	void PutRedisJob(DWORD64 sessionID, PLAYER* player);

	//playerMap ����
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
	CLockFreeQueue<JOB*> jobQ;
	//sessionID���� Ž��
	std::list<DWORD64> sectorList[SECTOR_Y_MAX][SECTOR_X_MAX];
	//sessionID���� Ž��
	std::unordered_map<INT64, PLAYER*> playerMap;

	CProcessMonitor processMonitor;
	CProcessorMonitor processorMonitor;
	CMonitorClient* monitorClient;

	DWORD64 timeOutCnt = 0;
	//�޼������� L'='���� ī��Ʈ
	DWORD64 logOutRecv = 0;
	DWORD64 chatCnt = 0;

	DWORD64 updateCnt = 0;
	DWORD64 lastUpdateCnt = 0;

	DWORD redisTLS;
	HANDLE* hRedis;
	int redisThreadCnt;
	HANDLE redisEvent;
	CLockFreeQueue<REDIS_JOB*>* redisJobQ;

	HANDLE updateEvent;

	HANDLE* hUpdateThreads;
	int updateThreadCnt;
	bool isServerOn;
};

