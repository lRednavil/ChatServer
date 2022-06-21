#pragma once
class CMonitorClient : public CLanClient
{
public:
	//시동함수 작성용
	virtual void Init();
	//connect직후 호출
	virtual bool OnConnect();
	//message 분석 역할
	virtual void OnRecv(CPacket* packet);
	//예외일 경우 선택(아마 높은확률로 disconnect)
	virtual void OnExpt();

	virtual void OnDisconnect();

	virtual void OnError(int error, const WCHAR* msg);

	//종료함수 작성용
	virtual void OnStop();

public:
	void UpdateMonitorInfo(BYTE dataType, int value, int timeVal);

private:
	void Req_Login();
};

