#pragma once
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <direct.h>

#include <MSWSock.h>


#include "LockFreeMemoryPool.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"
#include "TLSMemoryPool.h"
#include "SerializedBuffer.h"
#include "CommonProtocol.h"

#include "NetServer.h"
#include "ChatServer.h"

//¿¡·¯ Ã³¸®¿ë ´ýÇÁ¿Í ·Î±ë
#include "Dump.h"
#include "Logging.h"

//#define PROFILE_MODE
#include "TimeTracker.h"

#pragma comment (lib, "NetworkLibrary")
#pragma comment (lib, "Winmm")

