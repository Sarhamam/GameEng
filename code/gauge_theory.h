#pragma once
#include <math.h>

/*
    SU(3) × SU(2) × U(1) Gauge Theory Testbed

    Architecture:
    - 12 gauge bosons: 8 gluons (SU(3)) + 3 weak (SU(2)) + 1 photon (U(1))
    - Emergent spacetime curvature from gauge field stress-energy
    - 2D spatial manifold (screen coordinates)

    Key Insight:
    The metric g_μν is NOT fixed. It emerges as the "residual" from
    gauge field dynamics. Strong gauge fields → curved spacetime.
*/

// ═══════════════════════════════════════════════════════════
// 1. LIE ALGEBRA GENERATORS
// ═══════════════════════════════════════════════════════════

// SU(3) Gell-Mann matrices (8 generators for color)
struct su3_generator
{
    float matrix[3][3][2]; // 3×3 complex matrices (real, imag)
};

// SU(2) Pauli matrices (3 generators for weak isospin)
struct su2_generator
{
    float matrix[2][2][2]; // 2×2 complex matrices (real, imag)
};

// U(1) generator (just identity, but track phase)
struct u1_generator
{
    float phase; // Single complex phase
};

// Structure constants
global const float f_SU3[8][8][8] = {0}; // f^{abc} for SU(3)
global const float epsilon_SU2[3][3][3] = {0}; // ε^{ijk} for SU(2)

// Coupling constants
#define G_STRONG  1.0f   // g_s (QCD coupling)
#define G_WEAK    0.65f  // g_w (weak coupling)
#define G_EM      0.3f   // g' (U(1) hypercharge)

// ═══════════════════════════════════════════════════════════
// 2. GAUGE FIELD CONFIGURATION
// ═══════════════════════════════════════════════════════════

// 12 gauge bosons at a single point in spacetime
struct gauge_field_point
{
    // SU(3): 8 gluon fields (color)
    float G[8][2]; // G^a_μ for a=1..8, μ=1..2 (x,y components)

    // SU(2): 3 weak boson fields (W+, W-, Z)
    float W[3][2]; // W^i_μ for i=1..3, μ=1..2

    // U(1): 1 photon field
    float B[2];    // B_μ for μ=1..2
};

// Field strength tensors (curvature of gauge connection)
struct field_strength
{
    // SU(3) field strength: F^a_μν = ∂_μ G^a_ν - ∂_ν G^a_μ + g_s f^{abc} G^b_μ G^c_ν
    float F_SU3[8]; // Scalar in 2D: F^a_xy for a=1..8

    // SU(2) field strength: F^i_μν = ∂_μ W^i_ν - ∂_ν W^i_μ + g_w ε^{ijk} W^j_μ W^k_ν
    float F_SU2[3]; // Scalar in 2D: F^i_xy for i=1..3

    // U(1) field strength: F_μν = ∂_μ B_ν - ∂_ν B_μ
    float F_U1;     // Scalar in 2D: F_xy
};

// ═══════════════════════════════════════════════════════════
// 3. SPACETIME GEOMETRY (EMERGENT)
// ═══════════════════════════════════════════════════════════

// 2D metric tensor g_μν (induced from gauge fields)
struct metric_tensor
{
    float g11; // g_xx
    float g12; // g_xy (= g_yx by symmetry)
    float g22; // g_yy
};

// Stress-energy tensor from gauge fields
struct stress_energy_tensor
{
    float T11; // T_xx
    float T12; // T_xy
    float T22; // T_yy
};

// Curvature scalar (emergent)
struct spacetime_curvature
{
    float R;        // Ricci scalar
    float R11, R22; // Ricci tensor components
};

// ═══════════════════════════════════════════════════════════
// 4. MATTER FIELDS (Particles with internal quantum numbers)
// ═══════════════════════════════════════════════════════════

// Quark: (color triplet, weak doublet)
struct quark_state
{
    float x, y;          // Position in spacetime
    float vx, vy;        // Velocity

    // Color (SU(3)): 3 complex amplitudes
    float color[3][2];   // ψ_color ∈ ℂ³

    // Weak isospin (SU(2)): 2 complex amplitudes
    float weak[2][2];    // ψ_weak ∈ ℂ²

    // U(1) hypercharge
    float hypercharge;   // Single phase
};

// Lepton: (color singlet, weak doublet)
struct lepton_state
{
    float x, y;
    float vx, vy;

    // No color (singlet)

    // Weak isospin (SU(2)): 2 complex amplitudes
    float weak[2][2];

    // U(1) hypercharge
    float hypercharge;
};

// ═══════════════════════════════════════════════════════════
// 5. GAUGE FIELD LATTICE (Discretized spacetime)
// ═══════════════════════════════════════════════════════════

#define GAUGE_LATTICE_WIDTH  128
#define GAUGE_LATTICE_HEIGHT 128
#define LATTICE_SPACING      1.0f // Spatial resolution (in game units)

struct gauge_field_lattice
{
    // Gauge fields at each lattice point
    gauge_field_point fields[GAUGE_LATTICE_WIDTH][GAUGE_LATTICE_HEIGHT];

    // Derived quantities (computed from fields)
    field_strength strengths[GAUGE_LATTICE_WIDTH][GAUGE_LATTICE_HEIGHT];
    metric_tensor metrics[GAUGE_LATTICE_WIDTH][GAUGE_LATTICE_HEIGHT];
    stress_energy_tensor stress[GAUGE_LATTICE_WIDTH][GAUGE_LATTICE_HEIGHT];
    spacetime_curvature curvature[GAUGE_LATTICE_WIDTH][GAUGE_LATTICE_HEIGHT];
};

// ═══════════════════════════════════════════════════════════
// 6. GAUGE THEORY STATE (Simulation state)
// ═══════════════════════════════════════════════════════════

#define MAX_QUARKS  256
#define MAX_LEPTONS 256

struct gauge_theory_state
{
    gauge_field_lattice* lattice;

    // Matter particles
    quark_state quarks[MAX_QUARKS];
    lepton_state leptons[MAX_LEPTONS];
    int quark_count;
    int lepton_count;

    // Simulation parameters
    float dt;                  // Time step
    float gauge_coupling_alpha; // Controls metric induction strength

    // Visualization mode
    enum {
        VIZ_GLUONS,     // Show SU(3) field strength
        VIZ_WEAK,       // Show SU(2) field strength
        VIZ_EM,         // Show U(1) field strength
        VIZ_CURVATURE,  // Show emergent spacetime curvature
        VIZ_METRIC,     // Show metric tensor distortion
        VIZ_PARTICLES   // Show matter fields
    } viz_mode;
};

// ═══════════════════════════════════════════════════════════
// 7. CORE OPERATIONS (Function declarations)
// ═══════════════════════════════════════════════════════════

// Initialize gauge field lattice
internal_fnc void InitGaugeFieldLattice(gauge_field_lattice* lattice);

// Compute field strengths from gauge potentials
internal_fnc void ComputeFieldStrengths(gauge_field_lattice* lattice);

// Compute stress-energy tensor from field strengths
internal_fnc void ComputeStressEnergy(gauge_field_lattice* lattice);

// Induce metric from stress-energy (Einstein equation solver)
internal_fnc void InduceMetricFromGaugeFields(gauge_field_lattice* lattice, float alpha);

// Compute spacetime curvature from metric
internal_fnc void ComputeCurvature(gauge_field_lattice* lattice);

// Parallel transport for matter fields
internal_fnc void ParallelTransportQuark(quark_state* quark, gauge_field_point* field, float dt);
internal_fnc void ParallelTransportLepton(lepton_state* lepton, gauge_field_point* field, float dt);

// Update particle positions under gauge + gravity forces
internal_fnc void UpdateQuarkDynamics(gauge_theory_state* state, float dt);
internal_fnc void UpdateLeptonDynamics(gauge_theory_state* state, float dt);

// Render gauge fields to screen
internal_fnc void RenderGaugeFields(gauge_theory_state* state, game_offscreen_buffer* buffer);

// ═══════════════════════════════════════════════════════════
// 8. HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════

// Sample gauge field at arbitrary position (bilinear interpolation)
internal_fnc gauge_field_point SampleGaugeField(gauge_field_lattice* lattice, float x, float y);

// Sample metric at arbitrary position
internal_fnc metric_tensor SampleMetric(gauge_field_lattice* lattice, float x, float y);

// Complex multiplication helper
inline void ComplexMult(float* result, float* a, float* b)
{
    // (a_r + i*a_i) * (b_r + i*b_i) = (a_r*b_r - a_i*b_i) + i*(a_r*b_i + a_i*b_r)
    float real = a[0]*b[0] - a[1]*b[1];
    float imag = a[0]*b[1] + a[1]*b[0];
    result[0] = real;
    result[1] = imag;
}

// Compute determinant of 2×2 metric
inline float MetricDeterminant(metric_tensor* g)
{
    return g->g11 * g->g22 - g->g12 * g->g12;
}

// Invert 2×2 metric
inline void InvertMetric(metric_tensor* g_inv, metric_tensor* g)
{
    float det = MetricDeterminant(g);
    Assert(det > 1e-8f); // Metric must be non-degenerate

    g_inv->g11 =  g->g22 / det;
    g_inv->g12 = -g->g12 / det;
    g_inv->g22 =  g->g11 / det;
}

// ═══════════════════════════════════════════════════════════
// 9. INITIALIZATION PATTERNS
// ═══════════════════════════════════════════════════════════

// Common gauge field configurations for testing

// Constant field (uniform background)
internal_fnc void SetUniformGaugeField(gauge_field_lattice* lattice, int field_type);

// Dipole configuration (2 opposite charges)
internal_fnc void SetDipoleGaugeField(gauge_field_lattice* lattice, float x1, float y1, float x2, float y2);

// Vortex configuration (topological defect)
internal_fnc void SetVortexGaugeField(gauge_field_lattice* lattice, float cx, float cy, float strength);

// Random fluctuations (vacuum noise)
internal_fnc void AddGaugeFieldNoise(gauge_field_lattice* lattice, float amplitude);

// ═══════════════════════════════════════════════════════════
// 10. MEASUREMENT OBSERVABLES
// ═══════════════════════════════════════════════════════════

// Wilson loop (gauge-invariant observable)
internal_fnc float ComputeWilsonLoop(gauge_field_lattice* lattice, int x0, int y0, int width, int height);

// Total action (sum of gauge kinetic term + matter term)
internal_fnc float ComputeTotalAction(gauge_theory_state* state);

// Average curvature scalar
internal_fnc float ComputeAverageCurvature(gauge_field_lattice* lattice);

// κζ ratio from gauge field eigenspectrum (connection to your existing framework!)
internal_fnc float ComputeKappaZetaFromGaugeFields(gauge_field_lattice* lattice);
