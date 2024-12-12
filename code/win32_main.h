#pragma once
#include <windows.h>
#include <winuser.h>
#include <dsound.h>
#define WIN32_MAX_PATH MAX_PATH
struct win32_game_code
{
	HMODULE DLL;
	FILETIME lastWriteTime;
	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;
	bool isValid;
};
struct win32_sound_output
{
	int samplesPerSecond;
	int bytesPerSample;
	uint32_t bufferSize;
	uint32_t runningSampleIndex;
	float tSin;
	int safetyBytes;
};
struct win32_offscreen_buffer
{// 32 bits long BB GG RR xx
	BITMAPINFO Info;
	void* memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
};
struct win32_window_dimensions
{
	int width;
	int height;
};

struct win32_state
{
	uint64_t memorySize;
	void* gameMemory;

	HANDLE recordHandle;
	HANDLE playingHandle;
	int recordIndex;
	int playingIndex;
};

#if DEV_BUILD
struct win32_debug_sound_markers
{
	DWORD outputPlayCursor;
	DWORD outputWriteCursor;
	DWORD flipPlayCursor;
	DWORD flipWriteCursor;
	DWORD expectedFlipPlayCursor;
	DWORD outputLocation;
	DWORD outputBytes;
};
#endif