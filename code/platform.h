#pragma once
#include <stdint.h>
#include <stdio.h>
#define bool32 uint32_t
/*
	DEV_BUILD - Build for development
	SLOW_BUILD - Slow code allowed
*/
/*
	Services the OS provides the game
*/
struct thread_context
{
	int placeholder;
};
#if DEV_BUILD
struct debug_read_result
{
	uint32_t size;
	void* content;
};
// DEBUG ONLY - blocking and write doesnt protect against lost data
#define DEBUG_PLATFORM_READ_FILE(name) debug_read_result name(thread_context* thread, char* fileName)
typedef DEBUG_PLATFORM_READ_FILE(debug_platform_read_file);
#define DEBUG_PLATFORM_FREE_FILE(name) void name(thread_context* thread, void* fileMemory)
typedef DEBUG_PLATFORM_FREE_FILE(debug_platform_free_file);
#define DEBUG_PLATFORM_WRITE_FILE(name) bool name(thread_context* thread, char* fileName, void* fileMemory, uint32_t fileSize)
typedef DEBUG_PLATFORM_WRITE_FILE(debug_platform_write_file);
#endif
/*
	Services the game provides to the OS-Layer.
*/

struct game_offscreen_buffer
{// 32 bits long BB GG RR xx
	void* memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
};
struct game_sound_output
{
	int samplesPerSecond;
	int samplesCount;
	int16_t* samplesOutput;
};

struct game_button_state
{
	int transitions;
	bool endedDown;
	uint8_t pad;
	uint16_t padding;
};

struct game_controller_input
{
	// Here we define the available buttons
	union
	{
		game_button_state Buttons[7];
		struct
		{
			game_button_state up;
			game_button_state down;
			game_button_state left;
			game_button_state right;
			game_button_state leftShoulder;
			game_button_state rightShoulder;
			game_button_state action;
		};
	};
};
struct game_mouse_input
{
	int32_t x;
	int32_t y;
	int32_t z; // Scroller - not supported atm
	union
	{
		game_button_state Buttons[5];
		struct
		{
			game_button_state left;
			game_button_state right;
			game_button_state middle;
			game_button_state M1;
			game_button_state M2;
		};
	};
};
struct game_input
{
	float dtdf; // seconds per frame
	game_mouse_input mouse[1];
	game_controller_input controllers[1]; // Support single player with single device for now
};
struct game_memory
{
	uint64_t permanentSize;
	uint64_t transientSize;
	uint64_t totalSize;
	void* permanentMemory; // Must be cleared to 0
	void* transientMemory; // Must be cleared to 0
	bool isInitialised;
#if DEV_BUILD
	debug_platform_read_file* DEBUGPlatformReadFile;
	debug_platform_free_file* DEBUGPlatformFreeFile;
	debug_platform_write_file* DEBUGPlatformWriteFile;
#endif
};

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context* thread, game_memory* gameMemory, game_offscreen_buffer* screenBuffer, game_input* playerInput)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context* thread, game_memory* gameMemory, game_sound_output* soundBuffer) // Must be a fast function for audio sync.
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
