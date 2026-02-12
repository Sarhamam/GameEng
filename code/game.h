#pragma once
#include "platform.h"
#define global static
#define local static
#define internal_fnc static
#define PI32 3.141592653589793f

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

GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}

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
