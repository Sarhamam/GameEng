#include "game.h"
#include "win32_main.h"


#define ALT_FLAG (1 << 29)
#define WAS_DOWN_FLAG (1 << 30)
#define IS_DOWN_FLAG (1 << 31)
#define SCHEDUELER_GRANULARITY_MS 1
#define refreshHz 60
#define gameUpdateHz (refreshHz / 2)
/* TODO: Refactor video/sound/input
	- Saved games 
	- get handle for our executable
	- asset loading
	- threading
	- Raw input
	- sleep / timing 
	- fullscreen support
	- clipcursor() for multiple screens
	- control cursor visibility
	- QueryCancelAutoplay (?)
	- WM_ACTIVATEAPP(when we're not active)
	- Improve blit speed
	- hawrdware acceleration (opengl d3d? )
	- GetKeyboardLayout (international WASD)
*/


// TODO: No globals
global bool globalRunning;
global win32_offscreen_buffer globalBackbuffer;
global LPDIRECTSOUNDBUFFER globalSecondaryBuffer;
global LARGE_INTEGER globalPerformanceFrequency;
global bool globalPause;

/*
DLL Hotloading
*/
inline FILETIME GetLastWriteTime(char* filename)
{
	FILETIME lastWriteTime = {};
	WIN32_FILE_ATTRIBUTE_DATA info = {};
	if (GetFileAttributesEx(filename, GetFileExInfoStandard, &info))
	{
		lastWriteTime = info.ftLastWriteTime;
	}
	return lastWriteTime;
}


internal_fnc win32_game_code Win32LoadGameCode(char* gameDllName, char* tmpDllName)
{
	win32_game_code gameCode = {};
	gameCode.lastWriteTime = GetLastWriteTime(gameDllName);
	CopyFile(gameDllName, tmpDllName, FALSE);
	gameCode.DLL = LoadLibraryA(tmpDllName);
	if (gameCode.DLL)
	{
		gameCode.GetSoundSamples = (game_get_sound_samples*)GetProcAddress(gameCode.DLL, "GameGetSoundSamples");
		gameCode.UpdateAndRender = (game_update_and_render*)GetProcAddress(gameCode.DLL, "GameUpdateAndRender");
		gameCode.isValid = gameCode.GetSoundSamples && gameCode.UpdateAndRender;
	}
	if (!gameCode.isValid)
	{
		gameCode.UpdateAndRender = GameUpdateAndRenderStub;
		gameCode.GetSoundSamples = GameGetSoundSamplesStub;
	}
	return gameCode;
}
internal_fnc void Win32UnloadGameCode(win32_game_code* gameCode)
{
	if (gameCode->DLL)
	{
		FreeLibrary(gameCode->DLL);
	}
	
	gameCode->isValid = false;
	gameCode->UpdateAndRender = GameUpdateAndRenderStub;
	gameCode->GetSoundSamples = GameGetSoundSamplesStub;
}


/*
Files (Internal)
*/

inline void GetDirectory(char* fullPath, int fullPathSize, char* basePath, int basePathSize)
{
	char* fileNameLocation = fullPath;
	for (char* c = fullPath; *c; c++) // Null terminated
	{
		if (*c == '\\')
		{
			fileNameLocation = c;
		}
	}
	for (int i = 0; i < fileNameLocation - fullPath && i < (basePathSize - 1); i++)
	{
		basePath[i] = fullPath[i];
	}
	basePath[fileNameLocation - fullPath] = 0;
}
internal_fnc void GetRunningDirectory(char* result, int resultSize)
{
	char exeFilename[WIN32_MAX_PATH];
	DWORD exeFilenameSize = GetModuleFileNameA(0, exeFilename, sizeof(exeFilename));
	GetDirectory(exeFilename, exeFilenameSize, result, resultSize);
}
 
//	Playbacks
//
internal_fnc void Win32StartRecording(win32_state* win32State, int recordIndex)
{
	char filepath[WIN32_MAX_PATH];
	GetRunningDirectory(filepath, WIN32_MAX_PATH);
	wsprintf(filepath, "%s\\loop_%d.input", filepath, recordIndex);
	win32State->recordIndex = recordIndex;
	win32State->recordHandle = CreateFileA(filepath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	Assert(win32State->memorySize < 0xFFFFFFFF);
	DWORD bytesWritten;
	WriteFile(win32State->recordHandle, win32State->gameMemory, (DWORD)win32State->memorySize, &bytesWritten, 0);

}
internal_fnc void Win32EndRecording(win32_state* win32State)
{
	CloseHandle(win32State->recordHandle);
	win32State->recordHandle = 0;
	//win32State->recordIndex = 0;
}
internal_fnc void Win32StartPlayback(win32_state* win32State, int playingIndex)
{
	char filepath[WIN32_MAX_PATH];
	GetRunningDirectory(filepath, WIN32_MAX_PATH);
	wsprintf(filepath, "%s\\loop_%d.input", filepath, playingIndex);
	win32State->playingIndex = playingIndex;
	win32State->playingHandle = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	DWORD bytesRead;
	Assert(win32State->memorySize < 0xFFFFFFFF);
	ReadFile(win32State->playingHandle, win32State->gameMemory, (DWORD)win32State->memorySize, &bytesRead, 0);
}
internal_fnc void Win32EndPlayback(win32_state* win32State)
{
	CloseHandle(win32State->playingHandle);
	win32State->playingHandle = 0;
	//win32State->playingIndex = 0;

}
internal_fnc void Win32RecordInput(win32_state* win32State, game_input* input)
{
	DWORD bytesWritten;
	WriteFile(win32State->recordHandle, input, sizeof(*input), &bytesWritten, 0);
}
internal_fnc void Win32PlayInput(win32_state* win32State, game_input* input)
{
	DWORD bytesRead;
	if (ReadFile(win32State->playingHandle, input, sizeof(*input), &bytesRead, 0))
	{
		if (bytesRead == 0)
		{
			int playingIndex = win32State->playingIndex;
			Win32EndPlayback(win32State);
			Win32StartPlayback(win32State, playingIndex);
			ReadFile(win32State->playingHandle, input, sizeof(*input), &bytesRead, 0);
		}
	}
}

/*
Files (Externally used by game layer)
*/

#if DEV_BUILD
DEBUG_PLATFORM_FREE_FILE(DEBUGPlatformFreeFile)
{
	if (fileMemory)
	{
		VirtualFree(fileMemory, 0, MEM_RELEASE);
	}
}
DEBUG_PLATFORM_READ_FILE(DEBUGPlatformReadFile)
{
	debug_read_result result = {};
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(fileHandle, &fileSize))
	{
		// TODO: Log
		return result;
	} 
	Assert(fileSize.QuadPart < 0xFFFFFFFF);
	uint32_t fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
	void* fileMemory = VirtualAlloc(0, fileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!fileMemory)
	{
		// TODO log
		return result;
	}
	DWORD bytesRead = 0;
	if (!ReadFile(fileHandle, fileMemory, fileSize32, &bytesRead, 0) || bytesRead < fileSize32)
	{
		// TODO : Log
		DEBUGPlatformFreeFile(thread, fileMemory);
		fileMemory=0;
	}
	CloseHandle(fileHandle);
	result.content = fileMemory;
	result.size = fileSize32;
	return result;
}
DEBUG_PLATFORM_WRITE_FILE(DEBUGPlatformWriteFile)
{
	bool result = true;
	HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	DWORD bytesWritten = 0;
	if (!WriteFile(fileHandle, fileMemory, fileSize, &bytesWritten, 0) || bytesWritten < fileSize)
	{
		// TODO : Log
		result = false;
	}
	CloseHandle(fileHandle);
	return result;
}
#endif
/*
Video
*/

internal_fnc void Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int width, int height)
{
	// TODO : Dont free first, for robust. try allocating first if we run out of memory to still have a bm

	if (Buffer->memory)
	{
		VirtualFree(Buffer->memory, 0, MEM_RELEASE);

	}
	int BytesPerPixel = 4; // RGB + 1 Pad for alignment

	Buffer->width = width;
	Buffer->height = height;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->height; // Minus for top-down orientation (starts with top left)
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;
	// TODO: Can we allocate memory and create ourself?

	int BitmapMemorySize = BytesPerPixel * (Buffer->width*Buffer->height);
	Buffer->memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Buffer->pitch =Buffer->width *BytesPerPixel;
	Buffer->bytesPerPixel = BytesPerPixel;
}
internal_fnc void Win32DisplayBufferToWindow(HDC DeviceContext, win32_offscreen_buffer* Buffer, int WindowWidth, int WindowHeight)
{
	// TODO: Aspect ratio correct
	StretchDIBits(DeviceContext,
		// Disable resizing for development 0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer->width, Buffer->height,
		0, 0, Buffer->width, Buffer->height,
		Buffer->memory,
		&Buffer->Info,
		DIB_RGB_COLORS,
		SRCCOPY);
}
internal_fnc win32_window_dimensions Win32GetWindowDimension(HWND Window)
{
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	win32_window_dimensions WindowDimensions = { ClientRect.right - ClientRect.left, ClientRect.bottom - ClientRect.top };
	return WindowDimensions;
}
/*
Audio
*/

internal_fnc void Win32InitDSound(HWND Window, uint32_t samplesPerSecond, uint32_t bufferSize)
{
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if (!DSoundLibrary)
	{
		//TODO: Log
		return;
	}
	#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
	typedef DIRECT_SOUND_CREATE(direct_sound_create);
	direct_sound_create* DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
	LPDIRECTSOUND directSound;
	if (!(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0))))
	{
		//TODO : Log
		OutputDebugString("DSError");
		return;
	}
	if (!SUCCEEDED(directSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
		{
			OutputDebugString("DSError");
			return;
			//TODO: Log
		}
	
	// Define wave format
	WAVEFORMATEX waveFormat = {};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = 2;
	waveFormat.nSamplesPerSec = samplesPerSecond;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	// Create primary buffer
	
	LPDIRECTSOUNDBUFFER primaryBuffer;
	DSBUFFERDESC bufferDescription = {};
	// global focus? (play when window not active)
	bufferDescription.dwSize = sizeof(bufferDescription);
	bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0)))
	{
		// TODO: Log
		OutputDebugString("DSError");
		return;
	}
	if (!SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
	{
		//TODO : Log
		OutputDebugString("DSError");
		return;
	}

	// Create secondary buffer
	DSBUFFERDESC secondaryBufferDescription = {};
	// global focus? (play when window not active)
	secondaryBufferDescription.dwSize = sizeof(secondaryBufferDescription);
	secondaryBufferDescription.dwBufferBytes = bufferSize;
	secondaryBufferDescription.lpwfxFormat = &waveFormat; // This time around we can pass a wave format directly
	if (!SUCCEEDED(directSound->CreateSoundBuffer(&secondaryBufferDescription, &globalSecondaryBuffer, 0)))
	{
		OutputDebugString("DSError");
		// TODO: Log
		return;
	}
	OutputDebugString("DS- Okay\n");

}
internal_fnc void Win32FillSoundBuffer(win32_sound_output* soundOutput, game_sound_output* source, DWORD byteToLock, DWORD bytesToWrite)
{
	VOID* writeRegion;
	VOID* writeOverhead;
	DWORD regionSize;
	DWORD overheadSize;
	if (!SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &writeRegion, &regionSize, &writeOverhead, &overheadSize, 0)))
	{
		return;
	}
	int16_t* dest = (int16_t *)writeRegion;
	int16_t* src = source->samplesOutput;
	DWORD sampleCount = regionSize / soundOutput->bytesPerSample;
	for (DWORD sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
	{
		*dest++ = *src++;
		*dest++ = *src++;
		soundOutput->runningSampleIndex = 1 + soundOutput->runningSampleIndex % soundOutput->bufferSize;
	}

	sampleCount = overheadSize / soundOutput->bytesPerSample;
	dest = (int16_t *)writeOverhead;
	for (DWORD sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++)
	{
		*dest++ = *src++;
		*dest++ = *src++;
		soundOutput->runningSampleIndex = 1 + soundOutput->runningSampleIndex % soundOutput->bufferSize;

	}

	if (!SUCCEEDED(globalSecondaryBuffer->Unlock(writeRegion, regionSize, writeOverhead, overheadSize)))
	{
		// TODO : Log
		return;
	}
}
internal_fnc void Win32ClearSoundBuffer(win32_sound_output* soundOutput)
{
	VOID* writeRegion;
	VOID* writeOverhead;
	DWORD regionSize;
	DWORD overheadSize;
	if (!SUCCEEDED(globalSecondaryBuffer->Lock(0, soundOutput->bufferSize, &writeRegion, &regionSize, &writeOverhead, &overheadSize, 0)))
	{
		return;
	}
	uint8_t* dest = (uint8_t*)writeRegion;
	for (DWORD byteIndex = 0; byteIndex < regionSize; byteIndex++)
	{
		*dest++ = 0;
	}
	dest = (uint8_t*)writeOverhead;
	for (DWORD byteIndex = 0; byteIndex < overheadSize; byteIndex++)
	{
		*dest++ = 0;
	}
	if (!SUCCEEDED(globalSecondaryBuffer->Unlock(writeRegion, regionSize, writeOverhead, overheadSize)))
	{
		// TODO : Log
		return;
	}
}

/*
User Input
*/
internal_fnc void Win32KeyboardHandler(game_button_state* button, bool isDown)
{
	if (button->endedDown != isDown)
	{
		button->transitions++;
		button->endedDown = isDown;
	}
}
internal_fnc void Win32ProcessInputMessages(win32_state* win32State, game_controller_input* keyboardInput)
{
	MSG Message;
	while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch (Message.message)
		{
		case WM_QUIT:
		{
			globalRunning = false;
			break;
		}
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32_t VKCode = (uint32_t)Message.wParam;
			bool wasDown = ((Message.lParam & WAS_DOWN_FLAG) != 0);
			bool isDown = ((Message.lParam & IS_DOWN_FLAG) == 0);
			bool repeated = wasDown == isDown;
			if (!repeated)
			{
				switch (VKCode)
				{
				case('W'):
				{
					Win32KeyboardHandler(&keyboardInput->up, isDown);
					break;
				}
				case('A'):
				{
					Win32KeyboardHandler(&keyboardInput->left, isDown);
					break;
				}
				case('S'):
				{
					Win32KeyboardHandler(&keyboardInput->down, isDown);
					break;
				}
				case('D'):
				{
					Win32KeyboardHandler(&keyboardInput->right, isDown);
					break;
				}
				case('Q'):
				{
					break;
				}
				case('E'):
				{
					break;
				}
				case('L'):
				{
					if (isDown)
					{
						if (win32State->recordIndex == 0)
						{
							Win32StartRecording(win32State, 1);
						}
						else
						{
							Win32EndRecording(win32State);
							Win32StartPlayback(win32State, 1);
						}
					}
					if (Message.lParam & ALT_FLAG)
					{
						Win32EndPlayback(win32State);
					}
				}
				case(VK_UP):
				{
					break;
				}
				case(VK_LEFT):
				{
					break;
				}
				case(VK_DOWN):
				{
					break;
				}
				case(VK_RIGHT):
				{
					break;
				}
				case(VK_SPACE):
				{
					Win32KeyboardHandler(&keyboardInput->action, isDown);
					break;
				}
				case(VK_ESCAPE):
				{
					globalRunning = false;
					break;
				}
				case(VK_F4):
					if (Message.lParam & ALT_FLAG)
					{
						globalRunning = false;
					}
#if DEV_BUILD
				case('P'):
				{
					if(!wasDown)
						globalPause = !globalPause;
					break;
				}
#endif
				}
			}
		}
		break;
		default:
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		} break;
		}
	}
}


/*
Audio Debug Draws
*/
internal_fnc void Win32DebugDrawVLine(win32_offscreen_buffer* backbuffer, int x, int top, int bottom, uint32_t color)
{
	uint8_t* Pixel = (uint8_t *)backbuffer->memory + x * backbuffer->bytesPerPixel + top * backbuffer->pitch;
	for (int Y = top; Y < bottom; Y++)
	{
		*(uint32_t *)Pixel = color;
		Pixel += backbuffer->pitch;
	}
}
inline void Win32DebugDrawMarker(win32_offscreen_buffer* backbuffer, win32_sound_output* soundBuffer, DWORD cursor, float mapped_width, int padding, int top, int bottom, int color)
{
		int x = padding + (int)(mapped_width * (float)cursor);
		Win32DebugDrawVLine(backbuffer, x, top, bottom, color);
}
internal_fnc void Win32DebugAudioDraw(win32_offscreen_buffer* backbuffer, win32_sound_output* soundBuffer, win32_debug_sound_markers* soundMarkers, int markerCount, int currentMarkerIndex, float secondsPerFrame)
{
	int padding = 16;
	int lineHeight = 64;
	Assert(soundMarkers->flipPlayCursor < soundBuffer->bufferSize);
	Assert(soundMarkers->flipWriteCursor < soundBuffer->bufferSize);
	Assert(soundMarkers->outputPlayCursor < soundBuffer->bufferSize);
	Assert(soundMarkers->outputWriteCursor < soundBuffer->bufferSize);
	Assert(soundMarkers->outputLocation < soundBuffer->bufferSize);
	Assert(soundMarkers->outputBytes < soundBuffer->bufferSize);

	float mapped_width = (float) (backbuffer->width-2 * padding) / (float)soundBuffer->bufferSize;
	{
		int playColor = 0xFFFFFFFF; //White
		int writeColor = 0x00FF0000; //Red
		int windowColor = 0x00FFFF00; // Yellow
		int expectedColor = 0x000000FF; // Blue
		int locationColor = 0x00FF00FF; // Purple.
		
		for (int cursorIndex = 0; cursorIndex < markerCount; cursorIndex++)
		{
			int top = padding;
			int bottom = lineHeight + padding;
			if (cursorIndex == currentMarkerIndex)
			{
				top += padding + lineHeight;
				bottom += padding + lineHeight;

				Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].outputPlayCursor, mapped_width, padding, top, bottom, playColor);
				Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].outputWriteCursor, mapped_width, padding, top, bottom, writeColor);

				top += padding + lineHeight;
				bottom += padding + lineHeight;

				Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].outputLocation, mapped_width, padding, top, bottom, locationColor);
				Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].outputLocation + soundMarkers[cursorIndex].outputBytes, mapped_width, padding, top, bottom, locationColor);
	
				top += padding + lineHeight;
				bottom += padding + lineHeight;
			}
			Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].flipPlayCursor, mapped_width, padding, top, bottom, playColor);
			Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].flipWriteCursor, mapped_width, padding, top, bottom, writeColor);
			Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].expectedFlipPlayCursor, mapped_width, padding, top, bottom, expectedColor);
			Win32DebugDrawMarker(backbuffer, soundBuffer, soundMarkers[cursorIndex].expectedFlipPlayCursor + 480 * soundBuffer->bytesPerSample, mapped_width, padding, top, bottom, windowColor);



		}
	}
}


/*
Timing
*/
inline LARGE_INTEGER Win32GetTiming()
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return Result;
}
inline float Win32SecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	float result = (float)(end.QuadPart - start.QuadPart) / (float)globalPerformanceFrequency.QuadPart;
	return result;
}

/*
WinMain and WinCallback
*/

LRESULT CALLBACK Win32WindowCallback(HWND Window, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	switch (Message)
	{
	/* Inactive for the moment. all rendering at 720p
	case WM_SIZE:
	{
		win32_window_dimensions WindowDimension = Win32GetWindowDimension(Window);
		Win32ResizeDIBSection(&globalBackbuffer, WindowDimension.width, WindowDimension.height);
	} break; */
	case WM_DESTROY:
	{
		//  TODO : recreate window?
		globalRunning = false;
		OutputDebugStringA("DESTROY\n");

	} break;
	case WM_CLOSE:
	{
		// TODO : Message user?
		globalRunning = false;
		OutputDebugStringA("CLOSE\n");

	} break;
	case WM_ACTIVATEAPP:
	{
		if (wParam == TRUE)
		{
			SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);

		}
		else
		{
			SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
		}
		OutputDebugStringA("ACTIVEAPP\n");

	} break;
	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		win32_window_dimensions WindowDimension = Win32GetWindowDimension(Window);
		Win32DisplayBufferToWindow(DeviceContext, &globalBackbuffer, WindowDimension.width, WindowDimension.height);

		EndPaint(Window, &Paint);
		break;
	}
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		Assert(!"KEYBOARD OUTPUT BAD FLOW");
		break;
	}
	default:
	{
		//OutputDebugStringA("DEFAULT\n")
		result = DefWindowProc(Window, Message, wParam, lParam);
	}
	break;
	}
	return result;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{

	/* 
	Path resolving 
	*/
	char exeDirectory[WIN32_MAX_PATH];
	GetRunningDirectory(exeDirectory, WIN32_MAX_PATH);
	char gameDllPath[WIN32_MAX_PATH];
	char gameTempDllPath[WIN32_MAX_PATH];
	wsprintf(gameDllPath,"%s\\game.dll",exeDirectory);
	wsprintf(gameTempDllPath,"%s\\game_tmp.dll",exeDirectory);

	/*
		Input Recording
	*/
	// This is currently used for debugging. Can we use this for a "replay" feature?
	win32_state win32State = {};

	WNDCLASSA WindowClass = {};
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32WindowCallback;
	WindowClass.hInstance = hInstance;
	//	WindowClass.hIcon;
	WindowClass.lpszClassName = "DIYWindowClass";

	if (!RegisterClass(&WindowClass))
	{
		// TODO: Log	
	}
	//WS_EX_TOPMOST  might come in handy
	HWND Window = CreateWindowEx(WS_EX_LAYERED, WindowClass.lpszClassName, "Game", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);

	Win32ResizeDIBSection(&globalBackbuffer, 1240, 720);
	
	if (!Window)
	{
		// TODO: Log
	}
	globalRunning = true;
	globalPause = false;
#if 0
	// Tests for audio granularity (write/play cursors frequency.)
	// 480 samples in this pc
	while (globalRunning)
	{
		DWORD playCursor;
		DWORD writeCursor;
		globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);
		char textBuffer[256];
		sprintf_s(textBuffer, "PC: %u, WC: %u,\n", playCursor, writeCursor);
		OutputDebugStringA(textBuffer);
	}
#endif
	// Memory Init;
	LPVOID baseAddress = 0;
	#if DEV_BUILD
	// Get static address space for dev builds
	// Absurdly large address so there's no way it was allocated already
	baseAddress = (LPVOID) Terabytes(2);
	#endif
	thread_context thread = {};
	game_memory gameMemory = {};
	gameMemory.permanentSize =Megabytes(64);
	gameMemory.transientSize = Gigabytes(4);
	gameMemory.totalSize = gameMemory.permanentSize + gameMemory.transientSize;
	gameMemory.permanentMemory = VirtualAlloc(baseAddress, gameMemory.totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	gameMemory.transientMemory = ((uint8_t* )gameMemory.permanentMemory + gameMemory.permanentSize);
	// Save to state for debug loops
	win32State.memorySize = gameMemory.permanentSize;
	win32State.gameMemory = gameMemory.permanentMemory;
	/*
	EXPORTS TO GAME
	*/
	gameMemory.DEBUGPlatformReadFile = DEBUGPlatformReadFile;
	gameMemory.DEBUGPlatformFreeFile = DEBUGPlatformFreeFile;
	gameMemory.DEBUGPlatformWriteFile = DEBUGPlatformWriteFile;
	if (!(gameMemory.permanentMemory && gameMemory.transientMemory))
	{
		//TODO: Log
		return -1;
	}


	// Sound timing init
	win32_debug_sound_markers soundMarkers[gameUpdateHz / 2] = {};
	int DEBUGCursorIndex = 0;
	int audioLatencyBytes = 0;
	float audioLatencySeconds = 0;

	// Sound init
	win32_sound_output soundOutput = {};
	soundOutput.samplesPerSecond = 48000;
	soundOutput.bytesPerSample = sizeof(uint16_t) * 2;
	soundOutput.bufferSize = soundOutput.samplesPerSecond*soundOutput.bytesPerSample;
	soundOutput.safetyBytes = (soundOutput.samplesPerSecond *soundOutput.bytesPerSample) / gameUpdateHz / 3;
	Win32InitDSound(Window, soundOutput.samplesPerSecond, soundOutput.bufferSize);
	Win32ClearSoundBuffer(&soundOutput);
	if (!SUCCEEDED(globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING)))
	{
		// TODO : Log
	}
	int16_t* samples = (int16_t*)VirtualAlloc(0, soundOutput.bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	bool soundIsValid = false;

	// Input init
	game_input playerInput = {};
	game_controller_input* keyboardInput = &playerInput.controllers[0];
	// game_mouse_input* mouseInput = &playerInput.mouse[0]; Disabled for debugging. We dont want to record mouse movements.
	// Video init
	game_offscreen_buffer screenBuffer;
	screenBuffer.memory = globalBackbuffer.memory;
	screenBuffer.width = globalBackbuffer.width;
	screenBuffer.height = globalBackbuffer.height;
	screenBuffer.pitch = globalBackbuffer.pitch;
	screenBuffer.bytesPerPixel = globalBackbuffer.bytesPerPixel;

	// Timing init
	bool sleepIsGranular = (timeBeginPeriod(SCHEDUELER_GRANULARITY_MS) == TIMERR_NOERROR); // Sets schedueler granularity to 1ms for smallest sleeps possible.
	QueryPerformanceFrequency(&globalPerformanceFrequency);
	float targetSecondsPerFrame = 1.0f / (float)gameUpdateHz;
	playerInput.dtdf = targetSecondsPerFrame;


	win32_game_code Game = Win32LoadGameCode(gameDllPath, gameTempDllPath);
	LARGE_INTEGER flipCounter = Win32GetTiming();
	LARGE_INTEGER audioCounter = Win32GetTiming();
	float fromFlipToAudioSeconds = Win32SecondsElapsed(flipCounter, audioCounter);
	LARGE_INTEGER lastCounter = Win32GetTiming();
	uint64_t lastCycleCount = __rdtsc();
	while (globalRunning)
	{
		// TODO: Better mouse input.
#if DEV_BUILD
		POINT mousePosition;
		GetCursorPos(&mousePosition);
		ScreenToClient(Window, &mousePosition);
		playerInput.mouse->x = mousePosition.x;
		playerInput.mouse->y = mousePosition.y;
		playerInput.mouse->z = 0; // TODO: Support mouse wheel
		Win32KeyboardHandler(&playerInput.mouse->left, GetKeyState(VK_LBUTTON) & (1 << 15));
		Win32KeyboardHandler(&playerInput.mouse->right, GetKeyState(VK_RBUTTON) & (1 << 15));
		Win32KeyboardHandler(&playerInput.mouse->middle, GetKeyState(VK_MBUTTON) & (1 << 15));
		Win32KeyboardHandler(&playerInput.mouse->M1, GetKeyState(VK_XBUTTON1) & (1 << 15));
		Win32KeyboardHandler(&playerInput.mouse->M2, GetKeyState(VK_XBUTTON2) & (1 << 15));
#endif
		FILETIME updatedWriteTime = GetLastWriteTime(gameDllPath);
		if(CompareFileTime(&updatedWriteTime, &Game.lastWriteTime))
		{
			Win32UnloadGameCode(&Game);
			Game = Win32LoadGameCode(gameDllPath, gameTempDllPath);
		}
		for (int buttonIndex = 0; buttonIndex < ArrayCount(keyboardInput->Buttons); buttonIndex++)
		{
			keyboardInput->Buttons[buttonIndex].transitions = 0;
		}
		Win32ProcessInputMessages(&win32State, keyboardInput);

		if (!globalPause)
		{
			//Assert(!(win32State.recordIndex && win32State.playingIndex))
			if (win32State.recordIndex)
			{
				Win32RecordInput(&win32State, &playerInput);
			}
			if (win32State.playingIndex)
			{
				Win32PlayInput(&win32State, &playerInput);
			}
			Game.UpdateAndRender(&thread, &gameMemory, &screenBuffer, &playerInput);

			// Audio test
			DWORD bytesToWrite = 0;
			DWORD byteToLock = 0;
			DWORD targetCursor = 0;
			/*  AUDIO SYNC.
				Safety value - 1ms

				Low latency : if writecursor is a safety value behind frame boundary - fill the current frame and next one. (Syncronized audio)
				High latency : cant sync. always write one frame audio + safety value.
			*/
			DWORD playCursor;
			DWORD writeCursor;
			if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
			{
				if (!soundIsValid)
				{
					soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
					soundIsValid = true;
				}
			}
			else
			{
				soundIsValid = false;
			}
			if (soundIsValid)
			{

				byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.bufferSize;

				DWORD expectedSoundBytesPerFrame = soundOutput.samplesPerSecond * soundOutput.bytesPerSample / gameUpdateHz;
				float secondsLeftUntilFlip = targetSecondsPerFrame - fromFlipToAudioSeconds;
				DWORD expectedBytesUntilFlip = (DWORD) (secondsLeftUntilFlip / targetSecondsPerFrame * expectedSoundBytesPerFrame);
				DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;

				DWORD normalWriteCursor = writeCursor;
				if (normalWriteCursor < playCursor)
				{
					normalWriteCursor += soundOutput.bufferSize;
				}
				Assert(normalWriteCursor >= playCursor);

				normalWriteCursor += soundOutput.safetyBytes;
				bool lowLatencyAudio = (normalWriteCursor) < expectedFrameBoundaryByte;
				if (lowLatencyAudio)
				{
					targetCursor = (expectedFrameBoundaryByte + expectedSoundBytesPerFrame) % soundOutput.bufferSize;
				}
				else
				{
					// high latency audio card
					targetCursor = (writeCursor + expectedSoundBytesPerFrame + soundOutput.safetyBytes) % soundOutput.bufferSize;

				}
				bytesToWrite = 0;
				if (byteToLock > targetCursor)
				{
					bytesToWrite = (soundOutput.bufferSize - byteToLock);
					bytesToWrite += targetCursor;
				}
				else
				{
					bytesToWrite = targetCursor - byteToLock;
				}

				game_sound_output soundBuffer = {};
				soundBuffer.samplesOutput = samples;
				soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
				soundBuffer.samplesCount = bytesToWrite / soundOutput.bytesPerSample;

				Game.GetSoundSamples(&thread, &gameMemory, &soundBuffer);

				Win32FillSoundBuffer(&soundOutput, &soundBuffer, byteToLock, bytesToWrite);
				audioCounter = Win32GetTiming();
#if DEV_BUILD
				soundMarkers[DEBUGCursorIndex].expectedFlipPlayCursor = expectedFrameBoundaryByte;
				soundMarkers[DEBUGCursorIndex].outputPlayCursor = playCursor;
				soundMarkers[DEBUGCursorIndex].outputWriteCursor = writeCursor;
				soundMarkers[DEBUGCursorIndex].outputLocation = byteToLock;
				soundMarkers[DEBUGCursorIndex].outputBytes = bytesToWrite;
				audioLatencyBytes = writeCursor - playCursor;
				audioLatencySeconds = ((float)(audioLatencyBytes) / (float)soundOutput.bytesPerSample) / (float)soundOutput.samplesPerSecond;
				char textBuffer[256];
				sprintf_s(textBuffer, "TC: %u, BTW: %u, BTL: %u \n PC:%u WC:%u\n", targetCursor, bytesToWrite, byteToLock, playCursor, writeCursor);
				//OutputDebugStringA(textBuffer);
#endif
			}


			win32_window_dimensions WindowDimension = Win32GetWindowDimension(Window);

			LARGE_INTEGER workCounter = Win32GetTiming();

			float secondsElapsed = Win32SecondsElapsed(lastCounter, workCounter);
			if (secondsElapsed < targetSecondsPerFrame)
			{
				if (sleepIsGranular)
				{
					DWORD sleepMS = (DWORD)(1000.0f * (targetSecondsPerFrame - secondsElapsed));

					if (sleepMS > 0)
					{
						Sleep(sleepMS);
					}
				}
				float TestSecondsElapsed = Win32SecondsElapsed(workCounter, Win32GetTiming());
				// Assert(TestSecondsElapsed < targetSecondsPerFrame);
				while (secondsElapsed < targetSecondsPerFrame)
				{
					secondsElapsed = Win32SecondsElapsed(lastCounter, Win32GetTiming());
				}
			}
			else
			{

				// Missed a frame.
				// TODO: Log
			}
			LARGE_INTEGER endCounter = Win32GetTiming();
			int64_t endCycleCount = __rdtsc();
#if DEV_BUILD
			// Display previous markers 
			// Win32DebugAudioDraw(&globalBackbuffer, &soundOutput, soundMarkers, gameUpdateHz / 2, DEBUGCursorIndex - 1, targetSecondsPerFrame);
#endif
			HDC DeviceContext = GetDC(Window);
			Win32DisplayBufferToWindow(DeviceContext, &globalBackbuffer, WindowDimension.width, WindowDimension.height);
			ReleaseDC(Window, DeviceContext);

			flipCounter = Win32GetTiming();
#if DEV_BUILD
			{ // Update debug markers
				if (globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
				{
					soundMarkers[DEBUGCursorIndex].flipPlayCursor = playCursor;
					soundMarkers[DEBUGCursorIndex].flipWriteCursor = writeCursor;
				}
			}
#endif


			float milisecondsPerFrame = 1000.0f * Win32SecondsElapsed(lastCounter, endCounter);
			float fps = 1.0f / (float)Win32SecondsElapsed(lastCounter, flipCounter);
			uint64_t cyclesElapsed = endCycleCount - lastCycleCount;
			float megaCyclesPerFrame = (cyclesElapsed / (1000.0f * 1000.0f));
			char fpsBuffer[256];
			sprintf_s(fpsBuffer, "Diagnostics: %0.2fms/f, %0.2fmc/s %0.2ff/s\n", milisecondsPerFrame, megaCyclesPerFrame, fps);
			//if(milisecondsPerFrame >= 34.0f)
				OutputDebugStringA(fpsBuffer);

			lastCounter = endCounter;
			lastCycleCount = endCycleCount;
#if DEV_BUILD
			DEBUGCursorIndex = ++DEBUGCursorIndex % (gameUpdateHz / 2);
#endif
		}
	}

	VirtualFree(gameMemory.transientMemory, gameMemory.transientSize, MEM_DECOMMIT);
	return 0;
}