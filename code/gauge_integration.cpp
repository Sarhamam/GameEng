/*
    GAUGE THEORY INTEGRATION MODULE

    This file provides integration between the existing GameEng
    and the new SU(3)×SU(2)×U(1) gauge theory testbed.

    Usage:
    1. Call InitGaugeTheoryTestbed() in GameUpdateAndRender initialization
    2. Call UpdateGaugeTheoryTestbed() each frame
    3. Use keyboard to switch visualization modes:
       - '1': Show gluons (SU(3))
       - '2': Show weak bosons (SU(2))
       - '3': Show photon (U(1))
       - '4': Show curvature
       - '5': Show metric
       - '6': Show particles
       - 'R': Reset to initial conditions
       - 'N': Add random noise
*/

#include "gauge_theory.h"
#include "game.h"

// Forward declarations from gauge_render.cpp
extern void RenderGaugeFields(gauge_theory_state* state, game_offscreen_buffer* buffer);

// ═══════════════════════════════════════════════════════════
// TESTBED INITIALIZATION
// ═══════════════════════════════════════════════════════════

internal_fnc void InitGaugeTheoryTestbed(gauge_theory_state* state, void* memory)
{
    // Allocate lattice from provided memory
    state->lattice = (gauge_field_lattice*)memory;

    // Initialize lattice with zero fields and flat metric
    InitGaugeFieldLattice(state->lattice);

    // Set up initial gauge field configuration
    // Example: dipole configuration for electromagnetic field
    float width = (float)GAUGE_LATTICE_WIDTH * LATTICE_SPACING;
    float height = (float)GAUGE_LATTICE_HEIGHT * LATTICE_SPACING;

    SetDipoleGaugeField(state->lattice,
                        width * 0.3f, height * 0.5f,   // Positive charge
                        width * 0.7f, height * 0.5f);  // Negative charge

    // Add some noise to other gauge sectors
    AddGaugeFieldNoise(state->lattice, 0.05f);

    // Initialize particles
    state->quark_count = 0;
    state->lepton_count = 0;

    // Add test quarks
    for (int i = 0; i < 50; i++)
    {
        quark_state* quark = &state->quarks[state->quark_count++];

        // Random position
        quark->x = ((float)(i * 73 + 17) / 100.0f) * width;
        quark->y = ((float)(i * 37 + 53) / 100.0f) * height;

        // Random velocity
        float angle = ((float)i / 50.0f) * 2.0f * PI32;
        quark->vx = cosf(angle) * 10.0f;
        quark->vy = sinf(angle) * 10.0f;

        // Initialize color state (random phases)
        for (int c = 0; c < 3; c++)
        {
            float phase = ((float)(i + c) / 10.0f) * 2.0f * PI32;
            quark->color[c][0] = cosf(phase);
            quark->color[c][1] = sinf(phase);
        }

        // Initialize weak isospin
        for (int w = 0; w < 2; w++)
        {
            quark->weak[w][0] = 1.0f;
            quark->weak[w][1] = 0.0f;
        }

        // Hypercharge
        quark->hypercharge = ((float)i / 50.0f) * 2.0f * PI32;
    }

    // Add test leptons
    for (int i = 0; i < 30; i++)
    {
        lepton_state* lepton = &state->leptons[state->lepton_count++];

        lepton->x = ((float)(i * 43 + 29) / 100.0f) * width;
        lepton->y = ((float)(i * 71 + 11) / 100.0f) * height;

        float angle = ((float)i / 30.0f) * 2.0f * PI32 + PI32;
        lepton->vx = cosf(angle) * 5.0f;
        lepton->vy = sinf(angle) * 5.0f;

        for (int w = 0; w < 2; w++)
        {
            lepton->weak[w][0] = 1.0f;
            lepton->weak[w][1] = 0.0f;
        }

        lepton->hypercharge = ((float)i / 30.0f) * 2.0f * PI32;
    }

    // Simulation parameters
    state->dt = 0.01f;
    state->gauge_coupling_alpha = 0.1f;

    // Start with EM field visualization
    state->viz_mode = VIZ_EM;
}

// ═══════════════════════════════════════════════════════════
// TESTBED UPDATE (Called each frame)
// ═══════════════════════════════════════════════════════════

internal_fnc void UpdateGaugeTheoryTestbed(gauge_theory_state* state, game_input* input, float dt)
{
    // Handle keyboard input for mode switching
    game_controller_input* controller = &input->controllers[0];

    // Check for mode switches (using transitions to detect key press)
    static bool key_1_was_down = false;
    static bool key_2_was_down = false;
    static bool key_3_was_down = false;
    static bool key_4_was_down = false;
    static bool key_5_was_down = false;
    static bool key_6_was_down = false;
    static bool key_r_was_down = false;
    static bool key_n_was_down = false;

    // Note: This requires extending game_controller_input with number keys
    // For now, use shoulder buttons as temporary controls

    if (controller->leftShoulder.endedDown && !key_1_was_down)
    {
        // Cycle through visualization modes
        state->viz_mode = (decltype(state->viz_mode))((state->viz_mode + 1) % 6);
    }
    key_1_was_down = controller->leftShoulder.endedDown;

    if (controller->rightShoulder.endedDown && !key_r_was_down)
    {
        // Reset field configuration
        SetDipoleGaugeField(state->lattice,
                            64.0f, 64.0f,
                            96.0f, 64.0f);
        AddGaugeFieldNoise(state->lattice, 0.05f);
    }
    key_r_was_down = controller->rightShoulder.endedDown;

    if (controller->action.endedDown && !key_n_was_down)
    {
        // Add noise perturbation
        AddGaugeFieldNoise(state->lattice, 0.1f);
    }
    key_n_was_down = controller->action.endedDown;

    // Physics update

    // 1. Compute field strengths from gauge potentials
    ComputeFieldStrengths(state->lattice);

    // 2. Compute stress-energy tensor from field strengths
    ComputeStressEnergy(state->lattice);

    // 3. Induce metric from stress-energy (Einstein equation)
    InduceMetricFromGaugeFields(state->lattice, state->gauge_coupling_alpha);

    // 4. Compute spacetime curvature from metric
    ComputeCurvature(state->lattice);

    // 5. Update particle dynamics
    UpdateQuarkDynamics(state, state->dt);
    UpdateLeptonDynamics(state, state->dt);

    // Diagnostics (optional)
    float avg_curvature = ComputeAverageCurvature(state->lattice);
    float total_action = ComputeTotalAction(state);

    // Could display these values as debug text if desired
    // For now, they're just computed for validation
}

// ═══════════════════════════════════════════════════════════
// EXAMPLE: Modified GameUpdateAndRender with gauge theory
// ═══════════════════════════════════════════════════════════

/*

Add to game.h:

struct game_state
{
    world_position playerPos;

    // NEW: Gauge theory state
    gauge_theory_state* gaugeTheory;
    bool useGaugeTheory; // Toggle between classic game and gauge testbed
};

Then modify GameUpdateAndRender like this:

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert(sizeof(game_state) <= gameMemory->permanentSize);
    game_state* state = (game_state*)gameMemory->permanentMemory;

    if (!gameMemory->isInitialised)
    {
        gameMemory->isInitialised = true;
        state->playerPos.tileX = 3;
        state->playerPos.tileY = 3;
        state->playerPos.xOffset = 5.0f;
        state->playerPos.yOffset = 5.0f;

        // Initialize gauge theory testbed
        state->useGaugeTheory = true;
        state->gaugeTheory = (gauge_theory_state*)gameMemory->transientMemory;

        // Allocate lattice after gauge_theory_state struct
        void* latticeMemory = (uint8_t*)state->gaugeTheory + sizeof(gauge_theory_state);
        InitGaugeTheoryTestbed(state->gaugeTheory, latticeMemory);
    }

    // Toggle mode with a key (e.g., Tab or a specific button)
    if (playerInput->controllers[0].up.endedDown && playerInput->controllers[0].down.endedDown)
    {
        state->useGaugeTheory = !state->useGaugeTheory;
    }

    if (state->useGaugeTheory)
    {
        // Gauge theory mode
        UpdateGaugeTheoryTestbed(state->gaugeTheory, playerInput, playerInput->dtdf);
        RenderGaugeFields(state->gaugeTheory, screenBuffer);
    }
    else
    {
        // Original game mode
        // ... existing game code ...
    }
}

*/

// ═══════════════════════════════════════════════════════════
// MEMORY LAYOUT CALCULATION
// ═══════════════════════════════════════════════════════════

internal_fnc size_t GetRequiredGaugeTheoryMemory()
{
    return sizeof(gauge_theory_state) + sizeof(gauge_field_lattice);
}

// Approximately:
// - gauge_theory_state: ~100 KB (particles)
// - gauge_field_lattice: ~20 MB (128×128 lattice with 12 gauge fields + metrics)
// Total: ~20 MB (fits comfortably in 4 GB transient memory)
