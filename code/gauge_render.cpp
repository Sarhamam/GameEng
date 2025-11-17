#include "gauge_theory.h"
#include "game.h"
#include <math.h>

// ═══════════════════════════════════════════════════════════
// RENDERING UTILITIES
// ═══════════════════════════════════════════════════════════

// Color mapping: value → RGB
internal_fnc uint32_t ValueToColor(float value, float min_val, float max_val)
{
    // Map value in [min_val, max_val] to color gradient
    float normalized = (value - min_val) / (max_val - min_val + 1e-8f);
    normalized = (normalized < 0.0f) ? 0.0f : (normalized > 1.0f) ? 1.0f : normalized;

    // Color gradient: blue → cyan → green → yellow → red
    uint8_t r, g, b;

    if (normalized < 0.25f)
    {
        // Blue → Cyan
        float t = normalized / 0.25f;
        r = 0;
        g = (uint8_t)(t * 255);
        b = 255;
    }
    else if (normalized < 0.5f)
    {
        // Cyan → Green
        float t = (normalized - 0.25f) / 0.25f;
        r = 0;
        g = 255;
        b = (uint8_t)((1.0f - t) * 255);
    }
    else if (normalized < 0.75f)
    {
        // Green → Yellow
        float t = (normalized - 0.5f) / 0.25f;
        r = (uint8_t)(t * 255);
        g = 255;
        b = 0;
    }
    else
    {
        // Yellow → Red
        float t = (normalized - 0.75f) / 0.25f;
        r = 255;
        g = (uint8_t)((1.0f - t) * 255);
        b = 0;
    }

    return (r << 16) | (g << 8) | b;
}

// Phase to color (for quantum numbers)
internal_fnc uint32_t PhaseToColor(float phase)
{
    // Map phase ∈ [0, 2π] to hue wheel
    float hue = phase / (2.0f * PI32); // ∈ [0, 1]
    hue = hue - floorf(hue); // Wrap to [0, 1]

    // HSV to RGB conversion (S=1, V=1)
    float r, g, b;
    float h6 = hue * 6.0f;
    float f = h6 - floorf(h6);

    switch ((int)h6)
    {
        case 0: r = 1.0f; g = f;    b = 0.0f; break;
        case 1: r = 1.0f-f; g = 1.0f; b = 0.0f; break;
        case 2: r = 0.0f; g = 1.0f; b = f;    break;
        case 3: r = 0.0f; g = 1.0f-f; b = 1.0f; break;
        case 4: r = f;    g = 0.0f; b = 1.0f; break;
        default: r = 1.0f; g = 0.0f; b = 1.0f-f; break;
    }

    uint8_t r8 = (uint8_t)(r * 255);
    uint8_t g8 = (uint8_t)(g * 255);
    uint8_t b8 = (uint8_t)(b * 255);

    return (r8 << 16) | (g8 << 8) | b8;
}

// Draw pixel to buffer
internal_fnc void DrawPixel(game_offscreen_buffer* buffer, int x, int y, uint32_t color)
{
    if (x < 0 || x >= buffer->width || y < 0 || y >= buffer->height) return;

    uint32_t* pixel = (uint32_t*)buffer->memory;
    pixel[y * buffer->width + x] = color;
}

// Draw filled circle
internal_fnc void DrawCircle(game_offscreen_buffer* buffer, int cx, int cy, int radius, uint32_t color)
{
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            if (dx*dx + dy*dy <= radius*radius)
            {
                DrawPixel(buffer, cx + dx, cy + dy, color);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════
// MAIN RENDERING FUNCTION
// ═══════════════════════════════════════════════════════════

internal_fnc void RenderGaugeFields(gauge_theory_state* state, game_offscreen_buffer* buffer)
{
    gauge_field_lattice* lattice = state->lattice;

    // Determine lattice-to-screen scaling
    float scale_x = (float)buffer->width / (float)GAUGE_LATTICE_WIDTH;
    float scale_y = (float)buffer->height / (float)GAUGE_LATTICE_HEIGHT;

    // Render based on visualization mode
    switch (state->viz_mode)
    {
        case VIZ_GLUONS:
        {
            // Show SU(3) field strength (sum of all 8 gluons)
            float max_strength = 0.0f;
            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float strength = 0.0f;
                    for (int a = 0; a < 8; a++)
                    {
                        strength += fabsf(lattice->strengths[x][y].F_SU3[a]);
                    }
                    if (strength > max_strength) max_strength = strength;
                }
            }

            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float strength = 0.0f;
                    for (int a = 0; a < 8; a++)
                    {
                        strength += fabsf(lattice->strengths[x][y].F_SU3[a]);
                    }

                    uint32_t color = ValueToColor(strength, 0.0f, max_strength);

                    // Draw scaled pixel
                    int sx = (int)(x * scale_x);
                    int sy = (int)(y * scale_y);
                    for (int dy = 0; dy < (int)scale_y; dy++)
                    {
                        for (int dx = 0; dx < (int)scale_x; dx++)
                        {
                            DrawPixel(buffer, sx + dx, sy + dy, color);
                        }
                    }
                }
            }
            break;
        }

        case VIZ_WEAK:
        {
            // Show SU(2) field strength
            float max_strength = 0.0f;
            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float strength = 0.0f;
                    for (int i = 0; i < 3; i++)
                    {
                        strength += fabsf(lattice->strengths[x][y].F_SU2[i]);
                    }
                    if (strength > max_strength) max_strength = strength;
                }
            }

            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float strength = 0.0f;
                    for (int i = 0; i < 3; i++)
                    {
                        strength += fabsf(lattice->strengths[x][y].F_SU2[i]);
                    }

                    uint32_t color = ValueToColor(strength, 0.0f, max_strength);

                    int sx = (int)(x * scale_x);
                    int sy = (int)(y * scale_y);
                    for (int dy = 0; dy < (int)scale_y; dy++)
                    {
                        for (int dx = 0; dx < (int)scale_x; dx++)
                        {
                            DrawPixel(buffer, sx + dx, sy + dy, color);
                        }
                    }
                }
            }
            break;
        }

        case VIZ_EM:
        {
            // Show U(1) electromagnetic field strength
            float max_strength = 0.0f;
            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float strength = fabsf(lattice->strengths[x][y].F_U1);
                    if (strength > max_strength) max_strength = strength;
                }
            }

            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float strength = fabsf(lattice->strengths[x][y].F_U1);
                    uint32_t color = ValueToColor(strength, 0.0f, max_strength);

                    int sx = (int)(x * scale_x);
                    int sy = (int)(y * scale_y);
                    for (int dy = 0; dy < (int)scale_y; dy++)
                    {
                        for (int dx = 0; dx < (int)scale_x; dx++)
                        {
                            DrawPixel(buffer, sx + dx, sy + dy, color);
                        }
                    }
                }
            }
            break;
        }

        case VIZ_CURVATURE:
        {
            // Show emergent spacetime curvature
            float max_R = 0.0f;
            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float R = fabsf(lattice->curvature[x][y].R);
                    if (R > max_R) max_R = R;
                }
            }

            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    float R = lattice->curvature[x][y].R;

                    // Use diverging colormap: negative curvature (blue), zero (black), positive (red)
                    uint32_t color;
                    if (R < 0.0f)
                    {
                        float intensity = (-R / max_R) * 255.0f;
                        if (intensity > 255.0f) intensity = 255.0f;
                        color = (uint32_t)intensity; // Blue channel
                    }
                    else
                    {
                        float intensity = (R / max_R) * 255.0f;
                        if (intensity > 255.0f) intensity = 255.0f;
                        color = ((uint32_t)intensity) << 16; // Red channel
                    }

                    int sx = (int)(x * scale_x);
                    int sy = (int)(y * scale_y);
                    for (int dy = 0; dy < (int)scale_y; dy++)
                    {
                        for (int dx = 0; dx < (int)scale_x; dx++)
                        {
                            DrawPixel(buffer, sx + dx, sy + dy, color);
                        }
                    }
                }
            }
            break;
        }

        case VIZ_METRIC:
        {
            // Show metric tensor distortion (deviation from flat)
            for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
            {
                for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
                {
                    metric_tensor* g = &lattice->metrics[x][y];

                    // Measure distortion: ||g - η||
                    float distortion = sqrtf((g->g11 - 1.0f)*(g->g11 - 1.0f) +
                                             g->g12*g->g12 +
                                             (g->g22 - 1.0f)*(g->g22 - 1.0f));

                    uint32_t color = ValueToColor(distortion, 0.0f, 1.0f);

                    int sx = (int)(x * scale_x);
                    int sy = (int)(y * scale_y);
                    for (int dy = 0; dy < (int)scale_y; dy++)
                    {
                        for (int dx = 0; dx < (int)scale_x; dx++)
                        {
                            DrawPixel(buffer, sx + dx, sy + dy, color);
                        }
                    }
                }
            }
            break;
        }

        case VIZ_PARTICLES:
        {
            // Clear background to black
            uint32_t* pixels = (uint32_t*)buffer->memory;
            for (int i = 0; i < buffer->width * buffer->height; i++)
            {
                pixels[i] = 0x00000000;
            }

            // Draw quarks (colored by hypercharge phase)
            for (int i = 0; i < state->quark_count; i++)
            {
                quark_state* quark = &state->quarks[i];

                int screen_x = (int)((quark->x / LATTICE_SPACING) * scale_x);
                int screen_y = (int)((quark->y / LATTICE_SPACING) * scale_y);

                uint32_t color = PhaseToColor(quark->hypercharge);
                DrawCircle(buffer, screen_x, screen_y, 3, color);
            }

            // Draw leptons (colored by hypercharge phase)
            for (int i = 0; i < state->lepton_count; i++)
            {
                lepton_state* lepton = &state->leptons[i];

                int screen_x = (int)((lepton->x / LATTICE_SPACING) * scale_x);
                int screen_y = (int)((lepton->y / LATTICE_SPACING) * scale_y);

                uint32_t color = PhaseToColor(lepton->hypercharge);
                DrawCircle(buffer, screen_x, screen_y, 2, color);
            }
            break;
        }
    }
}
