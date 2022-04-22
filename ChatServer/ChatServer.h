#pragma once

#include <list>
#include <unordered_map>

#define SECTOR_X_MAX 50
#define SECTOR_Y_MAX 50

//player�� job�� ���� ����� ��������..?
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

	void Monitor();

	void ThreadInit();
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

	void OnError(int error, const WCHAR* msg);

//chatserver ���� �Լ� ����
	static unsigned int __stdcall _UpdateThread(void* arg);
	static unsigned int __stdcall _TimerThread(void* arg);

	//��� Recv�Լ��� PacketFree�� ��
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
	DWORD64 totalAccept = 0;
	DWORD64 totalSend = 0;
	DWORD64 totalRecv = 0;

	CLockFreeQueue<JOB> jobQ;
	//sessionID���� Ž��
	std::list<DWORD64> sectorList[SECTOR_Y_MAX][SECTOR_X_MAX];
	//sessionID���� Ž��
	std::unordered_map<INT64, PLAYER*> playerMap;
	DWORD currentTime;

	//0 for update Thread, 1 for timer Thread
	HANDLE hThreads[2];
	bool isServerOn;
};

