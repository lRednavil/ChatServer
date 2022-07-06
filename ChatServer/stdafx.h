#pragma once
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <direct.h>

#include <MSWSock.h>


#include "ProcessMonitor.h"
#include "ProcessorMonitor.h"

#include "LockFreeMemoryPool.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "TLSMemoryPool.h"
#include "SerializedBuffer.h"
#include "CommonProtocol.h"

#include <cpp_redis/cpp_redis>

#include "LanClient.h"
#include "NetServer.h"
#include "ChatServer.h"
#include "MonitorClient.h"

//���� ó���� ������ �α�
#include "Dump.h"
#include "Logging.h"

//#define PROFILE_MODE
#include "TimeTracker.h"

#pragma comment (lib, "cpp_redis")
#pragma comment (lib, "tacopie")
#pragma comment (lib, "NetworkLibrary")
#pragma comment (lib, "Winmm")

