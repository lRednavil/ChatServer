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
	//accept 직후, IP filterinig 등의 목적
	bool OnConnectionRequest(WCHAR* IP, DWORD Port) {
		return true;
	};
	//return false; 시 클라이언트 거부.
	//return true; 시 접속 허용
	bool OnClientJoin(DWORD64 sessionID) {
		++totalAccept;
		return true;
	};
	bool OnClientLeave(DWORD64 sessionID) {
		return true;
	}

	//message 분석 역할
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

