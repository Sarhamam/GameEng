#pragma once
#include <stdint.h>
#include <stdio.h>
#define global static
#define local static
#define internal_fnc static
#define bool32 uint32_t
#define PI32 3.141592653589793f
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


#if SLOW_BUILD
#define Assert(Exp) if(!(Exp)) {*(int *)0 = 0;}
#else
#define Assert(Exp)
#endif

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

inline uint32_t SafeTruncateUInt64(uint64_t value)
{
	Assert(value < 0xFFFFFFFF);
	return (uint32_t)value;
}

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

struct tile_chunk_position
{
	uint32_t chunkX;
	uint32_t chunkY;
	uint32_t relativeX;
	uint32_t relativeY;
};
struct world_position
{
	uint32_t tileX;
	uint32_t tileY;
	// Tile relative X,Y
	float xOffset;
	float yOffset;
};
struct game_state
{
	world_position playerPos;

};

struct tile_chunk
{
	uint32_t* tiles;
};
struct world_map
{
	float tileSizeMeters;
	float metersToPixels;
	int32_t tileSizePixels; // In pixels

	uint32_t chunkMask;
	uint32_t chunkShift;
	uint32_t chunkDim;

	uint32_t chunkCountX;
	uint32_t chunkCountY;
	tile_chunk* tileChunks;
};

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context* thread, game_memory* gameMemory, game_offscreen_buffer* screenBuffer, game_input* playerInput)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context* thread, game_memory* gameMemory, game_sound_output* soundBuffer) // Must be a fast function for audio sync.
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}