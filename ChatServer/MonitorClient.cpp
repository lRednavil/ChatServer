#include "stdafx.h"
#include "MonitorClient.h"
#define WritePrivateProfileInt(section, key, value, buffer, fileName) \
_itow_s(value, buffer, 10); \
WritePrivateProfileString(section, key, buffer, fileName)

void CMonitorClient::Init()
{
	WCHAR IP[16];
	DWORD PORT;
	bool isNagle;

	WCHAR temp[22];

	GetPrivateProfileString(L"MonitorClient", L"IP", L"0.0.0.0", IP, 16, L".//ServerSettings.ini");
	PORT = GetPrivateProfileInt(L"MonitorClient", L"PORT", NULL, L".//ServerSettings.ini");
	isNagle = GetPrivateProfileInt(L"MonitorClient", L"isNagle", NULL, L".//ServerSettings.ini");

	WritePrivateProfileString(L"MonitorClient", L"IP", IP, L".//ServerSettings.ini");
	WritePrivateProfileInt(L"MonitorClient", L"PORT", PORT, temp, L".//ServerSettings.ini");
	WritePrivateProfileInt(L"MonitorClient", L"isNagle", isNagle, temp, L".//ServerSettings.ini");

	if (PORT == 0) {
		_FILE_LOG(LOG_LEVEL_ERROR, L"INIT_LOG", L"INVALID ARGUMENTS or No ini FILE");
		CRASH();
	}

	Start(isNagle);
	Connect(L"MonitorClient", IP, PORT);
}

bool CMonitorClient::OnConnect()
{
	_LOG(LOG_LEVEL_DEBUG, L"Connected to Monitoring Server");
	Req_Login();
	return true;
}

void CMonitorClient::OnRecv(CPacket* packet)
{
}

void CMonitorClient::OnExpt()
{
	ReConnect();
}

void CMonitorClient::OnDisconnect()
{
	_LOG(LOG_LEVEL_SYSTEM, L"Client Disconnected");
	ReConnect();
}

void CMonitorClient::OnError(int error, const WCHAR* msg)
{
	_LOG(LOG_LEVEL_DEBUG, L"code : %d | msg : %s", error, msg);
	ReConnect();
}

void CMonitorClient::OnStop()
{
}

void CMonitorClient::UpdateMonitorInfo(BYTE dataType, int value, int timeVal)
{
	CPacket* packet = PacketAlloc();
	constexpr WORD type = en_PACKET_SS_MONITOR_DATA_UPDATE;
	*packet << type << dataType << value << timeVal;

	SendPacket(packet);
}

void CMonitorClient::Req_Login()
{
	CPacket* packet = PacketAlloc();
	constexpr WORD type = en_PACKET_SS_MONITOR_LOGIN;
	constexpr int serverNo = 2;
	
	*packet << type << serverNo;

	SendPacket(packet);
}
