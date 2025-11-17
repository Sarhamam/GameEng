/*
    COMPLETE EXAMPLE: game.cpp with Gauge Theory Integration

    This is a complete working example showing how to integrate
    the SU(3)×SU(2)×U(1) gauge theory testbed into GameEng.

    To use:
    1. Copy this file to game.cpp (backup original first!)
    2. Build with: misc/build_with_gauge.bat
    3. Run: build/win32_main.exe

    Controls:
    - Left Shoulder:  Cycle visualization modes
    - Right Shoulder: Reset gauge field configuration
    - Action:         Add random noise
    - Up+Down:        Toggle between gauge mode and classic game
*/

#include "game.h"
#include "intrinsics.h"
#include "gauge_theory.h"

// Include gauge modules
#include "gauge_integration.cpp"
#include "gauge_render.cpp"
#include "gauge_bootstrap.cpp"

// ═══════════════════════════════════════════════════════════
// EXTENDED GAME STATE
// ═══════════════════════════════════════════════════════════

struct game_state_extended
{
    // Original game state
    world_position playerPos;

    // Gauge theory testbed
    gauge_theory_state* gaugeTheory;
    bool useGaugeTheory;
    bool runBootstrapExperiment;

    // Bootstrap analysis
    convergence_history* convergenceHistory;
    int frame_count;
};

// ═══════════════════════════════════════════════════════════
// HELPER FUNCTIONS (from original game.cpp)
// ═══════════════════════════════════════════════════════════

inline uint32_t colorHex(float red, float green, float blue)
{
    return roundFloatToUInt32(red * 255.0f) << 16 |
           roundFloatToUInt32(green * 255.0f) << 8 |
           roundFloatToUInt32(blue * 255.0f) << 0;
}

internal_fnc void DrawRectangle(game_offscreen_buffer* buffer,
                                 float fminX, float fminY, float fmaxX, float fmaxY,
                                 float red, float green, float blue)
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
    uint8_t* row = (uint8_t*)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;

    for (int y = minY; y < maxY; y++)
    {
        uint32_t* pixel = (uint32_t*)row;
        for (int x = minX; x < maxX; x++)
        {
            *pixel = color;
            pixel++;
        }
        row += buffer->pitch;
    }
}

// ═══════════════════════════════════════════════════════════
// DEBUG TEXT RENDERING (Simple)
// ═══════════════════════════════════════════════════════════

internal_fnc void DrawDebugText(game_offscreen_buffer* buffer, const char* text,
                                 int x, int y, uint32_t color)
{
    // Simple text rendering - just shows colored boxes for now
    // In real implementation, use bitmap font
    for (int i = 0; text[i] != '\0' && i < 40; i++)
    {
        int px = x + i * 8;
        if (px >= buffer->width) break;

        // Draw a small box per character
        for (int dy = 0; dy < 8; dy++)
        {
            for (int dx = 0; dx < 6; dx++)
            {
                int screen_x = px + dx;
                int screen_y = y + dy;
                if (screen_x >= 0 && screen_x < buffer->width &&
                    screen_y >= 0 && screen_y < buffer->height)
                {
                    uint32_t* pixel = (uint32_t*)buffer->memory;
                    pixel[screen_y * buffer->width + screen_x] = color;
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════
// MAIN GAME UPDATE AND RENDER
// ═══════════════════════════════════════════════════════════

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state_extended) <= gameMemory->permanentSize);
    game_state_extended* state = (game_state_extended*)gameMemory->permanentMemory;

    if (!gameMemory->isInitialised)
    {
        gameMemory->isInitialised = true;

        // Initialize classic game state
        state->playerPos.tileX = 3;
        state->playerPos.tileY = 3;
        state->playerPos.xOffset = 5.0f;
        state->playerPos.yOffset = 5.0f;

        // Initialize gauge theory testbed
        state->useGaugeTheory = true;  // Start in gauge mode
        state->runBootstrapExperiment = false;
        state->frame_count = 0;

        // Allocate gauge theory state in transient memory
        state->gaugeTheory = (gauge_theory_state*)gameMemory->transientMemory;

        // Allocate lattice after gauge_theory_state
        void* latticeMemory = (uint8_t*)state->gaugeTheory + sizeof(gauge_theory_state);

        // Allocate convergence history after lattice
        state->convergenceHistory = (convergence_history*)
            ((uint8_t*)latticeMemory + sizeof(gauge_field_lattice));

        // Initialize gauge testbed
        InitGaugeTheoryTestbed(state->gaugeTheory, latticeMemory);

        // Initialize convergence tracking
        InitConvergenceHistory(state->convergenceHistory);

        printf("\n╔══════════════════════════════════════════════════════╗\n");
        printf("║   GAUGE THEORY TESTBED INITIALIZED                   ║\n");
        printf("╚══════════════════════════════════════════════════════╝\n\n");
        printf("Controls:\n");
        printf("  Left Shoulder:  Cycle visualization modes\n");
        printf("  Right Shoulder: Reset gauge fields\n");
        printf("  Action:         Add noise\n");
        printf("  Up+Down:        Toggle gauge/classic mode\n");
        printf("\n");
    }

    game_controller_input* controller = &playerInput->controllers[0];

    // Toggle between gauge mode and classic game mode
    static bool toggle_was_pressed = false;
    bool toggle_pressed = controller->up.endedDown && controller->down.endedDown;

    if (toggle_pressed && !toggle_was_pressed)
    {
        state->useGaugeTheory = !state->useGaugeTheory;
        printf("\n%s MODE\n\n", state->useGaugeTheory ? "GAUGE THEORY" : "CLASSIC GAME");
    }
    toggle_was_pressed = toggle_pressed;

    // ═══════════════════════════════════════════════════════════
    // GAUGE THEORY MODE
    // ═══════════════════════════════════════════════════════════

    if (state->useGaugeTheory)
    {
        // Update gauge theory physics
        UpdateGaugeTheoryTestbed(state->gaugeTheory, playerInput, playerInput->dtdf);

        // Render gauge fields
        RenderGaugeFields(state->gaugeTheory, screenBuffer);

        // Compute and track bootstrap metrics every 10 frames
        if (state->frame_count % 10 == 0)
        {
            bootstrap_metrics metrics = ComputeBootstrapMetrics(state->gaugeTheory);
            UpdateConvergenceHistory(state->convergenceHistory, metrics);

            // Print metrics every 100 frames
            if (state->frame_count % 100 == 0)
            {
                printf("Frame %d: β=%.4f, κζ_gauge=%.4f, κζ_metric=%.4f, Δκζ=%.6f %s\n",
                       state->frame_count,
                       metrics.beta,
                       metrics.kappa_zeta_gauge,
                       metrics.kappa_zeta_metric,
                       metrics.kappa_zeta_difference,
                       metrics.is_fixed_point ? "✓" : " ");
            }

            #if DEV_BUILD
            // Log to file
            if (state->frame_count % 50 == 0)
            {
                LogBootstrapMetrics(metrics, "bootstrap_realtime.csv");
            }
            #endif
        }

        // Draw overlay info
        char info[128];
        const char* mode_names[] = {
            "GLUONS (SU3)", "WEAK (SU2)", "EM (U1)",
            "CURVATURE", "METRIC", "PARTICLES"
        };

        sprintf(info, "Mode: %s", mode_names[state->gaugeTheory->viz_mode]);
        DrawDebugText(screenBuffer, info, 10, 10, 0xFFFFFF);

        sprintf(info, "Frame: %d", state->frame_count);
        DrawDebugText(screenBuffer, info, 10, 20, 0xCCCCCC);

        state->frame_count++;
    }

    // ═══════════════════════════════════════════════════════════
    // CLASSIC GAME MODE
    // ═══════════════════════════════════════════════════════════

    else
    {
        // Original game rendering (simplified placeholder)
        DrawRectangle(screenBuffer, 0, 0,
                      (float)screenBuffer->width, (float)screenBuffer->height,
                      0.1f, 0.1f, 0.2f);

        // Draw player
        float playerX = 400.0f;
        float playerY = 300.0f;

        // Handle input
        if (controller->up.endedDown) playerY -= 5.0f;
        if (controller->down.endedDown) playerY += 5.0f;
        if (controller->left.endedDown) playerX -= 5.0f;
        if (controller->right.endedDown) playerX += 5.0f;

        // Draw player rectangle
        DrawRectangle(screenBuffer,
                      playerX - 20.0f, playerY - 20.0f,
                      playerX + 20.0f, playerY + 20.0f,
                      0.3f, 0.6f, 0.9f);

        DrawDebugText(screenBuffer, "CLASSIC GAME MODE", 10, 10, 0xFFFF00);
    }
}

// ═══════════════════════════════════════════════════════════
// SOUND (Placeholder)
// ═══════════════════════════════════════════════════════════

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    // Silence
    for (int i = 0; i < soundBuffer->samplesCount * 2; i++)
    {
        soundBuffer->samplesOutput[i] = 0;
    }
}
