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

	WORD SectorX;
	WORD SectorY;

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

//virtual�Լ� ����
private:
	//accept ����, IP filterinig ���� ����
	bool OnConnectionRequest(WCHAR* IP, DWORD Port) {
		return true;
	};
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

	void Recv_Login(DWORD64 sessionID, CPacket* packet);
	void Res_Login(DWORD64 sessionID, CPacket* packet);

	void Recv_SectorMove(DWORD64 sessionID, CPacket* packet);
	void Res_SectorMove(DWORD64 sessionID, CPacket* packet);

	void Recv_Message(DWORD64 sessionID, CPacket* packet);
	void Res_Message(DWORD64 sessionID, CPacket* packet);

	void Recv_HeartBeat(DWORD64 sessionID, CPacket* packet);

private:
	DWORD64 totalAccept = 0;
	DWORD64 totalSend = 0;
	DWORD64 totalRecv = 0;

	CLockFreeQueue<JOB> jobQ;
	std::list<PLAYER> sectorList[SECTOR_Y_MAX][SECTOR_X_MAX];
	//player�� ���� player*���� ����� �սô�
	std::unordered_map<INT64, PLAYER> playerMap;

	//0 for update Thread, 1 for timer Thread
	HANDLE hThreads[2];
	bool isServerOn;
};

