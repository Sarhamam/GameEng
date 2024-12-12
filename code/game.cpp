#include "game.h"
#include "intrinsics.h"

inline uint32_t colorHex(float red, float green, float blue)
{
	return roundFloatToUInt32(red * 255.0f) << 16 | roundFloatToUInt32(green * 255.0f) << 8 | roundFloatToUInt32(blue * 255.0f) << 0;
}
internal_fnc void DrawRectangle(game_offscreen_buffer* buffer, float fminX, float fminY, float fmaxX, float fmaxY, float red, float green, float blue)
/*
Draws a rectangle with top left corner at (minX,minY) and bottom right at (maxX,maxY) non inclusive
*/
{
	int32_t minX = roundFloatToInt32(fminX);
	int32_t minY = roundFloatToInt32(fminY);
	int32_t maxX = roundFloatToInt32(fmaxX);
	int32_t maxY = roundFloatToInt32(fmaxY);

	minX = minX < 0 ? 0 : minX;
	minY = minY < 0 ? 0 : minY;
	maxY = maxY >= buffer->height ? buffer->height : maxY;
	maxX = maxX >= buffer->width ? buffer->width : maxX;
	uint32_t color = colorHex(red, green, blue);
	uint8_t* row = (uint8_t *)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;
	for (int y = minY; y < maxY; y++)
	{
		uint32_t* pixel = (uint32_t*) row;
		for (int x = minX; x < maxX; x++)
		{
			*pixel = color;
			pixel++;
		}

		row += buffer->pitch;

	}
}

inline uint32_t GetTileValue(world_map* world, tile_chunk* tileChunk, uint32_t tileX, uint32_t tileY)
{
	Assert(tileChunk);
	Assert((tileX < world->chunkDim && tileY < world->chunkDim));
	return tileChunk->tiles[tileY * world->chunkDim + tileX];
}

inline uint32_t GetTileValueDefault(world_map* world, tile_chunk* tileChunk, uint32_t tileX, uint32_t tileY)
{
	uint32_t result = 0;
	if (tileChunk) {
		result = GetTileValue(world, tileChunk, tileX, tileY);
	}
	return result;
}
inline tile_chunk* GetTileChunk(world_map* world, uint32_t chunkX, uint32_t chunkY)
{
	tile_chunk* tileChunk = 0;
	if ((chunkX < world->chunkCountX) &&
		((chunkY < world->chunkCountY)))
	{
		tileChunk = &world->tileChunks[chunkY * world->chunkCountX + chunkX];
	}
	return tileChunk;
}
internal_fnc bool32 isTileEmpty(world_map* world, tile_chunk* tileChunk, uint32_t tileX, uint32_t tileY)
{
	bool32 result = false;
	if (tileChunk)
	{
		if (GetTileValue(world, tileChunk, tileX, tileY) == 0)
		{
			result = true;
		}
	}
	
	return result;
}
inline tile_chunk_position GetChunkPosition(world_map* world, uint32_t tileX, uint32_t tileY)
{
	tile_chunk_position result = {};

	result.chunkX = tileX >> world->chunkShift;
	result.chunkY = tileY >> world->chunkShift;
	result.relativeX = tileX & world->chunkMask;
	result.relativeY = tileY & world->chunkMask;

	return result;
}
inline uint32_t GetWorldTileValue(world_map* world, uint32_t tileX, uint32_t tileY)
{
	uint32_t result;
	tile_chunk_position chunkPos = GetChunkPosition(world, tileX, tileY);
	tile_chunk* tileChunk = GetTileChunk(world, chunkPos.chunkX, chunkPos.chunkY);
	result = GetTileValueDefault(world, tileChunk, chunkPos.relativeX, chunkPos.relativeY);
	return result;
}
internal_fnc bool32 isWorldPointEmpty(world_map* world, world_position pos)
{
	bool32 result = false;
	result = GetWorldTileValue(world, pos.tileX, pos.tileY) == 0;
	return result;
}

inline void CalibrateCoordinates(world_map* world,uint32_t* tileC, float* offset)
{
	// TODO: Fix floating point rounding errors (too many multiplications and divisions)
	// Note: World is a torus
	int32_t overflow = FloorFloatToInt32( *offset / world->tileSizeMeters);
	*tileC += overflow;
	*offset -= overflow * world->tileSizeMeters;
	Assert(*offset < world->tileSizeMeters && *offset >= 0);
}
internal_fnc world_position CalibrateWorldPosition(world_map* world, world_position pos)
{
	world_position result = pos;
	CalibrateCoordinates(world, &result.tileX, &result.xOffset);
	CalibrateCoordinates(world, &result.tileY, &result.yOffset);
	return result;
}
internal_fnc void DEBUGGameUpdateSound(game_sound_output* soundOutput, int toneHz)
{
	local float tSin = 0;
	int volume = 0;
	float period = (float)soundOutput->samplesPerSecond / (float)toneHz;
	int16_t* sample = (int16_t *)soundOutput->samplesOutput;
	for (int sampleIndex = 0; sampleIndex < soundOutput->samplesCount; sampleIndex++)
	{
		float sinValue = Sin(tSin);
		int16_t value = (int16_t)(sinValue * volume);
		*sample++ = 0;
		*sample++ = 0;
		tSin += (float) (2.0f*PI32*1.0f / period);
		if (tSin > 2.0f*PI32)
		{
			tSin -= 2.0f*PI32;
		}
	}
}
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert(sizeof(game_state) <= gameMemory->permanentSize);
	game_state* state = (game_state* ) gameMemory->permanentMemory;
	if (!gameMemory->isInitialised)
	{
		gameMemory->isInitialised = true;
		state->playerPos.tileX = 3;
		state->playerPos.tileY = 3;
		state->playerPos.xOffset = 5.0f;
		state-> playerPos.yOffset = 5.0f;

	}
		game_controller_input controller = playerInput->controllers[0];
		game_mouse_input mouse = playerInput->mouse[0];
		// Digital input
		float dx = 0;
		float dy = 0;
		if (controller.up.endedDown)
		{
			dy = 1;
		}
		if (controller.down.endedDown)
		{
			dy = -1;
		}
		if (controller.right.endedDown)
		{
			dx = 1;
		}
		if (controller.left.endedDown)
		{
			dx = -1;
		}
	DrawRectangle(screenBuffer, 0, 0, (float)screenBuffer->width, (float)screenBuffer->height, 0.0f, 0.0f, 0.0f); // clear screen
#define TILEMAP_HEIGHT 256
#define TILEMAP_WIDTH 256
	uint32_t tempTiles[TILEMAP_HEIGHT][TILEMAP_WIDTH] =
	{
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	};
	tile_chunk tileChunk;
	tileChunk.tiles = (uint32_t*)tempTiles;

	world_map world;
	world.tileSizeMeters = 1.5f;
	world.tileSizePixels = 60;

	world.chunkCountX = 1;
	world.chunkCountY = 1;
	world.chunkShift = 8; //// 256x256 tile maps
	world.chunkMask = (1 << world.chunkShift) - 1; 
	world.chunkDim = 256;
	world.tileChunks = &tileChunk;
	world.metersToPixels = world.tileSizePixels / world.tileSizeMeters;
	
	// move player
	float playerHeigth = 1.4f; //meters
	float playerWidth = playerHeigth / 2.0f;
	float speed = 4; // meters per sec
	float newPlayerX = state->playerPos.xOffset + playerInput->dtdf * dx * speed;
	float newPlayerY = state->playerPos.yOffset + playerInput->dtdf * dy * speed;
	world_position newPos = state->playerPos;
	newPos.xOffset = newPlayerX;
	newPos.yOffset = newPlayerY;
	newPos = CalibrateWorldPosition(&world, newPos);

	bool32 validMovement = isWorldPointEmpty(&world, newPos);

	if (validMovement)
	{
		state->playerPos = newPos;
	}

	// Draw world
	float originX = screenBuffer->width * 0.5f;
	float originY = screenBuffer->height * 0.5f;

	Assert(tileChunk.tiles);

	for (int relY = -10; relY < 10; relY++)
	{
		for (int relX = -20; relX < 20; relX++)
		{
			uint32_t x = state->playerPos.tileX + relX;
			uint32_t y = state->playerPos.tileY + relY;
			float red = 0.5f;
			float green = 0.5f;
			float blue = 0.5f;
			if (GetWorldTileValue(&world, x, y))
			{
				red = 1.0f;
				green = 1.0f;
				blue = 1.0f;
			}
			if (state->playerPos.tileX == x && state->playerPos.tileY == y)
			{
				red = 0.0f;
				green = 0.0f;
				blue = 0.0f;
			}
			float minX = originX + ((float)relX)*world.tileSizePixels;
			float minY = originY - ((float)relY)*world.tileSizePixels;
			float maxX = minX + world.tileSizePixels;
			float maxY = minY - world.tileSizePixels;
			DrawRectangle(screenBuffer, minX, maxY, maxX, minY, red, green, blue); // draw map


		}
	}

	// Draw player
	float playerR = 0.5f;
	float playerG = 0.5f;
	float playerB = 1.0f;
	float playerHeigthInPixels = playerHeigth * world.metersToPixels;
	float playerWidthInPixels = playerWidth * world.metersToPixels;
	float playerLeft = originX + world.metersToPixels * state->playerPos.xOffset - playerWidthInPixels / 2;
	float playerTop = originY - world.metersToPixels * state->playerPos.yOffset - playerHeigthInPixels;
	DrawRectangle(screenBuffer, playerLeft, playerTop, playerLeft + playerWidthInPixels, playerTop + playerHeigthInPixels, playerR, playerG, playerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state* state = (game_state*)gameMemory->permanentMemory;
	DEBUGGameUpdateSound(soundBuffer, 400);
}