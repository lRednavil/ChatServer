#pragma once

#include <list>
#include <unordered_map>

#define SECTOR_X_MAX 50
#define SECTOR_Y_MAX 50

struct PLAYER {
	INT64 accountNo;
	WCHAR ID[20];
	WCHAR Nickname[20];

	WORD SectorX;
	WORD SectorY;
};

class ChatServer : public CNetServer
{
public:
	void Monitor();

private:
	//accept ����, IP filterinig ���� ����
	bool OnConnectionRequest(WCHAR* IP, DWORD Port) {
		return true;
	};
	//return false; �� Ŭ���̾�Ʈ �ź�.
	//return true; �� ���� ���
	bool OnClientJoin(DWORD64 sessionID) {
		++totalAccept;
		return true;
	};
	bool OnClientLeave(DWORD64 sessionID) {
		return true;
	}

	//message �м� ����
	void OnRecv(DWORD64 sessionID, CPacket* packet) {

	};

	void OnError(int error, const WCHAR* msg) {
	};

private:
	DWORD64 totalAccept;
	DWORD64 totalSend;
	DWORD64 totalRecv;

	std::list<PLAYER> sectorList[SECTOR_Y_MAX][SECTOR_X_MAX];



};

