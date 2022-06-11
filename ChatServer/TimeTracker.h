#pragma once

#include <Windows.h>


class CTimeTracker {
public:
	CTimeTracker(const WCHAR* name);

	~CTimeTracker() {
		EndTimeTrack();
	}

	void StartTimeTrack();
	static void MakeFileName();
	void EndTimeTrack();
	static void WriteTimeTrack();

	static void ResetTimeTrack();
};

#define PROFILE_MODE

#ifdef PROFILE_MODE
#define PROFILE_START(X) CTimeTracker(X)
#define PROFILE_RESET() ResetTimeTrack()
#define PROFILE_WRITE() WriteTimeTrack()
#else
#define PROFILE_START(X)
#define PROFILE_RESET()
#define PROFILE_WRITE()
#endif 
