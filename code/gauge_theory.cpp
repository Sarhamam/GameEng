#include "gauge_theory.h"
#include "game.h"
#include <math.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════
// STRUCTURE CONSTANTS (Lie algebra)
// ═══════════════════════════════════════════════════════════

// SU(3) structure constants f^{abc}
// For simplicity, we'll compute these on-the-fly from Gell-Mann matrices
// Full tables are available in QCD textbooks

// SU(2) structure constants: ε^{ijk} = Levi-Civita symbol
internal_fnc float Epsilon(int i, int j, int k)
{
    if (i == 0 && j == 1 && k == 2) return 1.0f;
    if (i == 1 && j == 2 && k == 0) return 1.0f;
    if (i == 2 && j == 0 && k == 1) return 1.0f;
    if (i == 0 && j == 2 && k == 1) return -1.0f;
    if (i == 2 && j == 1 && k == 0) return -1.0f;
    if (i == 1 && j == 0 && k == 2) return -1.0f;
    return 0.0f;
}

// SU(3) structure constants: f^{abc} from Gell-Mann matrix commutators
// [λ_a, λ_b] = 2i f^{abc} λ_c
// These are totally antisymmetric: f^{abc} = -f^{bac} = -f^{acb}
// Reference: Particle Data Group, "Review of Particle Physics", QCD section
internal_fnc float StructureConstantSU3(int a, int b, int c)
{
    // Indices are 0-7 (8 generators of SU(3))
    // We'll use a lookup approach for the non-zero values

    // Helper to create unique key for (a,b,c) triple
    // We'll check all permutations due to antisymmetry

    // Normalize to canonical order a < b
    if (a > b) {
        return -StructureConstantSU3(b, a, c);  // Antisymmetry
    }
    if (a == b) return 0.0f;  // Antisymmetric → diagonal is zero

    // Now a < b, check the non-zero structure constants
    // The non-zero f^{abc} values (using 0-indexing, so subtract 1 from textbook):

    const float sqrt3 = 1.732050808f;

    // f^{123} = 1 → f^{012} in 0-indexing
    if (a == 0 && b == 1 && c == 2) return 1.0f;
    if (a == 0 && b == 2 && c == 1) return -1.0f;
    if (a == 1 && b == 2 && c == 0) return 1.0f;

    // f^{147} = 1/2 → f^{036} in 0-indexing
    if (a == 0 && b == 3 && c == 6) return 0.5f;
    if (a == 0 && b == 6 && c == 3) return -0.5f;
    if (a == 3 && b == 6 && c == 0) return 0.5f;

    // f^{156} = -1/2 → f^{045} in 0-indexing
    if (a == 0 && b == 4 && c == 5) return -0.5f;
    if (a == 0 && b == 5 && c == 4) return 0.5f;
    if (a == 4 && b == 5 && c == 0) return -0.5f;

    // f^{246} = 1/2 → f^{135} in 0-indexing
    if (a == 1 && b == 3 && c == 5) return 0.5f;
    if (a == 1 && b == 5 && c == 3) return -0.5f;
    if (a == 3 && b == 5 && c == 1) return 0.5f;

    // f^{257} = 1/2 → f^{146} in 0-indexing
    if (a == 1 && b == 4 && c == 6) return 0.5f;
    if (a == 1 && b == 6 && c == 4) return -0.5f;
    if (a == 4 && b == 6 && c == 1) return 0.5f;

    // f^{345} = 1/2 → f^{234} in 0-indexing
    if (a == 2 && b == 3 && c == 4) return 0.5f;
    if (a == 2 && b == 4 && c == 3) return -0.5f;
    if (a == 3 && b == 4 && c == 2) return 0.5f;

    // f^{367} = -1/2 → f^{256} in 0-indexing
    if (a == 2 && b == 5 && c == 6) return -0.5f;
    if (a == 2 && b == 6 && c == 5) return 0.5f;
    if (a == 5 && b == 6 && c == 2) return -0.5f;

    // f^{458} = √3/2 → f^{347} in 0-indexing
    if (a == 3 && b == 4 && c == 7) return sqrt3 / 2.0f;
    if (a == 3 && b == 7 && c == 4) return -sqrt3 / 2.0f;
    if (a == 4 && b == 7 && c == 3) return sqrt3 / 2.0f;

    // f^{678} = √3/2 → f^{567} in 0-indexing
    if (a == 5 && b == 6 && c == 7) return sqrt3 / 2.0f;
    if (a == 5 && b == 7 && c == 6) return -sqrt3 / 2.0f;
    if (a == 6 && b == 7 && c == 5) return sqrt3 / 2.0f;

    // All other combinations are zero
    return 0.0f;
}

// ═══════════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════════

internal_fnc void InitGaugeFieldLattice(gauge_field_lattice* lattice)
{
    // Zero out all fields
    memset(lattice, 0, sizeof(gauge_field_lattice));

    // Set metrics to flat spacetime (Euclidean)
    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            lattice->metrics[x][y].g11 = 1.0f;
            lattice->metrics[x][y].g12 = 0.0f;
            lattice->metrics[x][y].g22 = 1.0f;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// FIELD STRENGTH COMPUTATION
// ═══════════════════════════════════════════════════════════

internal_fnc void ComputeFieldStrengths(gauge_field_lattice* lattice)
{
    // For each lattice point, compute F_μν = ∂_μ A_ν - ∂_ν A_μ + [A_μ, A_ν]
    // In 2D: only one independent component F_xy

    for (int y = 1; y < GAUGE_LATTICE_HEIGHT - 1; y++)
    {
        for (int x = 1; x < GAUGE_LATTICE_WIDTH - 1; x++)
        {
            gauge_field_point* field = &lattice->fields[x][y];
            field_strength* strength = &lattice->strengths[x][y];

            // Finite difference approximations
            // ∂_x A_y ≈ (A_y(x+1) - A_y(x-1)) / (2*dx)
            // ∂_y A_x ≈ (A_x(y+1) - A_x(y-1)) / (2*dy)

            float dx = LATTICE_SPACING;

            // SU(3): F^a_xy = ∂_x G^a_y - ∂_y G^a_x + g_s f^{abc} G^b_x G^c_y
            for (int a = 0; a < 8; a++)
            {
                float dGy_dx = (lattice->fields[x+1][y].G[a][1] - lattice->fields[x-1][y].G[a][1]) / (2.0f * dx);
                float dGx_dy = (lattice->fields[x][y+1].G[a][0] - lattice->fields[x][y-1].G[a][0]) / (2.0f * dx);

                float linear_part = dGy_dx - dGx_dy;

                // Nonlinear term (commutator): sum over b,c with structure constants
                // g_s f^{abc} G^b_x G^c_y
                float nonlinear_part = 0.0f;
                for (int b = 0; b < 8; b++)
                {
                    for (int c = 0; c < 8; c++)
                    {
                        float f_abc = StructureConstantSU3(a, b, c);
                        nonlinear_part += G_STRONG * f_abc * field->G[b][0] * field->G[c][1];
                    }
                }

                strength->F_SU3[a] = linear_part + nonlinear_part;
            }

            // SU(2): F^i_xy = ∂_x W^i_y - ∂_y W^i_x + g_w ε^{ijk} W^j_x W^k_y
            for (int i = 0; i < 3; i++)
            {
                float dWy_dx = (lattice->fields[x+1][y].W[i][1] - lattice->fields[x-1][y].W[i][1]) / (2.0f * dx);
                float dWx_dy = (lattice->fields[x][y+1].W[i][0] - lattice->fields[x][y-1].W[i][0]) / (2.0f * dx);

                float linear_part = dWy_dx - dWx_dy;

                // Nonlinear term with ε^{ijk}
                float nonlinear_part = 0.0f;
                for (int j = 0; j < 3; j++)
                {
                    for (int k = 0; k < 3; k++)
                    {
                        nonlinear_part += G_WEAK * Epsilon(i, j, k) * field->W[j][0] * field->W[k][1];
                    }
                }

                strength->F_SU2[i] = linear_part + nonlinear_part;
            }

            // U(1): F_xy = ∂_x B_y - ∂_y B_x (Abelian, no nonlinear term)
            float dBy_dx = (lattice->fields[x+1][y].B[1] - lattice->fields[x-1][y].B[1]) / (2.0f * dx);
            float dBx_dy = (lattice->fields[x][y+1].B[0] - lattice->fields[x][y-1].B[0]) / (2.0f * dx);

            strength->F_U1 = dBy_dx - dBx_dy;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// STRESS-ENERGY TENSOR FROM GAUGE FIELDS
// ═══════════════════════════════════════════════════════════

internal_fnc void ComputeStressEnergy(gauge_field_lattice* lattice)
{
    // T_μν = Σ_a F^a_μρ F^a_ν^ρ - (1/4) g_μν F^a_ρσ F^a^ρσ
    // In 2D with single F_xy component: simplified form

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            field_strength* F = &lattice->strengths[x][y];
            metric_tensor* g = &lattice->metrics[x][y];
            stress_energy_tensor* T = &lattice->stress[x][y];

            // Sum F² = Σ_a (F^a)² from all gauge groups
            float F_squared = 0.0f;

            // SU(3) contribution
            for (int a = 0; a < 8; a++)
            {
                F_squared += F->F_SU3[a] * F->F_SU3[a];
            }

            // SU(2) contribution
            for (int i = 0; i < 3; i++)
            {
                F_squared += F->F_SU2[i] * F->F_SU2[i];
            }

            // U(1) contribution
            F_squared += F->F_U1 * F->F_U1;

            // Stress-energy tensor: T_μν = F_μρ F_ν^ρ - (1/4) g_μν F_ρσ F^ρσ
            // In 2D with only F_xy component (pure "magnetic" field):
            //   T_11 = -F²/2
            //   T_22 = -F²/2
            //   T_12 = 0
            // This gives correct anisotropic structure (no shear for pure F_xy)
            T->T11 = -0.5f * F_squared;
            T->T12 = 0.0f;  // No shear for pure F_xy field
            T->T22 = -0.5f * F_squared;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// INDUCE METRIC FROM STRESS-ENERGY (Einstein Equation)
// ═══════════════════════════════════════════════════════════

internal_fnc void InduceMetricFromGaugeFields(gauge_field_lattice* lattice, float alpha)
{
    // Simplified Einstein equation solver:
    // g_μν^{new} = η_μν + α * (convolution of T_μν with Green's function)
    //
    // For 2D, we use a simplified local approximation:
    // g_μν(x) = η_μν + α * T_μν(x)

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            stress_energy_tensor* T = &lattice->stress[x][y];
            metric_tensor* g = &lattice->metrics[x][y];

            // Flat metric + perturbation from stress-energy
            g->g11 = 1.0f + alpha * T->T11;
            g->g12 = alpha * T->T12;
            g->g22 = 1.0f + alpha * T->T22;

            // Ensure positive-definiteness (clamp eigenvalues)
            float det = MetricDeterminant(g);
            if (det < 0.1f)
            {
                // Metric became degenerate, reset to flat
                g->g11 = 1.0f;
                g->g12 = 0.0f;
                g->g22 = 1.0f;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════
// CURVATURE COMPUTATION FROM METRIC
// ═══════════════════════════════════════════════════════════

internal_fnc void ComputeCurvature(gauge_field_lattice* lattice)
{
    // Compute Ricci scalar R from metric g_μν
    // In 2D: R = -2 * (∂²_x log√g + ∂²_y log√g) where g = det(g_μν)

    float dx = LATTICE_SPACING;

    for (int y = 1; y < GAUGE_LATTICE_HEIGHT - 1; y++)
    {
        for (int x = 1; x < GAUGE_LATTICE_WIDTH - 1; x++)
        {
            // log√g = (1/2) * log(det(g_μν))
            float det_center = MetricDeterminant(&lattice->metrics[x][y]);
            float det_xp = MetricDeterminant(&lattice->metrics[x+1][y]);
            float det_xm = MetricDeterminant(&lattice->metrics[x-1][y]);
            float det_yp = MetricDeterminant(&lattice->metrics[x][y+1]);
            float det_ym = MetricDeterminant(&lattice->metrics[x][y-1]);

            float log_det_center = 0.5f * logf(det_center + 1e-8f);
            float log_det_xp = 0.5f * logf(det_xp + 1e-8f);
            float log_det_xm = 0.5f * logf(det_xm + 1e-8f);
            float log_det_yp = 0.5f * logf(det_yp + 1e-8f);
            float log_det_ym = 0.5f * logf(det_ym + 1e-8f);

            // Second derivatives via finite differences
            float d2_log_det_x = (log_det_xp - 2.0f*log_det_center + log_det_xm) / (dx*dx);
            float d2_log_det_y = (log_det_yp - 2.0f*log_det_center + log_det_ym) / (dx*dx);

            lattice->curvature[x][y].R = -2.0f * (d2_log_det_x + d2_log_det_y);

            // Ricci tensor components (simplified)
            lattice->curvature[x][y].R11 = lattice->curvature[x][y].R * lattice->metrics[x][y].g11;
            lattice->curvature[x][y].R22 = lattice->curvature[x][y].R * lattice->metrics[x][y].g22;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// GAUGE FIELD DYNAMICS (Yang-Mills Evolution)
// ═══════════════════════════════════════════════════════════

internal_fnc void UpdateGaugeFields(gauge_field_lattice* lattice, float dt)
{
    // Evolve gauge fields via Yang-Mills equations with metric coupling
    // Using gradient descent on the action: A_μ^{new} = A_μ - α * ∂S/∂A_μ
    // where S = (1/4) ∫ F^a_μν F^a^μν √g d²x
    //
    // The derivative ∂S/∂A_μ gives the equation of motion:
    //   ∂S/∂A^a_μ = -D_ν F^a^μν
    // where D_ν is the gauge covariant derivative
    //
    // This couples to metric through √g and g^μν, providing feedback loop

    float dx = LATTICE_SPACING;
    float damping = 0.95f;  // Damping to prevent instabilities

    // Temporary storage for field updates
    gauge_field_lattice temp;
    memcpy(&temp, lattice, sizeof(gauge_field_lattice));

    for (int y = 1; y < GAUGE_LATTICE_HEIGHT - 1; y++)
    {
        for (int x = 1; x < GAUGE_LATTICE_WIDTH - 1; x++)
        {
            metric_tensor* g = &lattice->metrics[x][y];
            float sqrt_g = sqrtf(MetricDeterminant(g));

            // ─────────────────────────────────────────────────
            // SU(3) field evolution
            // ─────────────────────────────────────────────────
            for (int a = 0; a < 8; a++)
            {
                for (int mu = 0; mu < 2; mu++)  // μ ∈ {0,1} for x,y
                {
                    // Compute -D_ν F^{aμν} via finite differences
                    // This is the Yang-Mills source term

                    float F_a_mu_nu = 0.0f;

                    if (mu == 0)  // A_x component
                    {
                        // F^{a x y} = F^a_xy
                        float F_xy = lattice->strengths[x][y].F_SU3[a];
                        float F_xy_yp = lattice->strengths[x][y+1].F_SU3[a];
                        float F_xy_ym = lattice->strengths[x][y-1].F_SU3[a];

                        // D_y F^{a x y} ≈ ∂_y F^{a x y} + g_s f^{abc} A^b_y F^{c x y}
                        float dF_dy = (F_xy_yp - F_xy_ym) / (2.0f * dx);

                        // Gauge covariant correction (simplified)
                        float gauge_correction = 0.0f;
                        for (int b = 0; b < 8; b++)
                        {
                            for (int c = 0; c < 8; c++)
                            {
                                float f_abc = StructureConstantSU3(a, b, c);
                                gauge_correction += G_STRONG * f_abc *
                                    lattice->fields[x][y].G[b][1] * lattice->strengths[x][y].F_SU3[c];
                            }
                        }

                        F_a_mu_nu = dF_dy + gauge_correction;
                    }
                    else  // A_y component (mu == 1)
                    {
                        // F^{a y x} = -F^{a x y}
                        float F_yx = -lattice->strengths[x][y].F_SU3[a];
                        float F_yx_xp = -lattice->strengths[x+1][y].F_SU3[a];
                        float F_yx_xm = -lattice->strengths[x-1][y].F_SU3[a];

                        // D_x F^{a y x}
                        float dF_dx = (F_yx_xp - F_yx_xm) / (2.0f * dx);

                        float gauge_correction = 0.0f;
                        for (int b = 0; b < 8; b++)
                        {
                            for (int c = 0; c < 8; c++)
                            {
                                float f_abc = StructureConstantSU3(a, b, c);
                                gauge_correction += G_STRONG * f_abc *
                                    lattice->fields[x][y].G[b][0] * (-lattice->strengths[x][y].F_SU3[c]);
                            }
                        }

                        F_a_mu_nu = dF_dx + gauge_correction;
                    }

                    // Update: A^a_μ ← A^a_μ - dt * (1/√g) * D_ν F^{aμν}
                    float delta = -dt * F_a_mu_nu / (sqrt_g + 1e-8f);
                    temp.fields[x][y].G[a][mu] = damping * lattice->fields[x][y].G[a][mu] + delta;
                }
            }

            // ─────────────────────────────────────────────────
            // SU(2) field evolution
            // ─────────────────────────────────────────────────
            for (int i = 0; i < 3; i++)
            {
                for (int mu = 0; mu < 2; mu++)
                {
                    float F_i_mu_nu = 0.0f;

                    if (mu == 0)
                    {
                        float F_xy = lattice->strengths[x][y].F_SU2[i];
                        float F_xy_yp = lattice->strengths[x][y+1].F_SU2[i];
                        float F_xy_ym = lattice->strengths[x][y-1].F_SU2[i];
                        float dF_dy = (F_xy_yp - F_xy_ym) / (2.0f * dx);

                        float gauge_correction = 0.0f;
                        for (int j = 0; j < 3; j++)
                        {
                            for (int k = 0; k < 3; k++)
                            {
                                float eps_ijk = Epsilon(i, j, k);
                                gauge_correction += G_WEAK * eps_ijk *
                                    lattice->fields[x][y].W[j][1] * lattice->strengths[x][y].F_SU2[k];
                            }
                        }

                        F_i_mu_nu = dF_dy + gauge_correction;
                    }
                    else
                    {
                        float F_yx = -lattice->strengths[x][y].F_SU2[i];
                        float F_yx_xp = -lattice->strengths[x+1][y].F_SU2[i];
                        float F_yx_xm = -lattice->strengths[x-1][y].F_SU2[i];
                        float dF_dx = (F_yx_xp - F_yx_xm) / (2.0f * dx);

                        float gauge_correction = 0.0f;
                        for (int j = 0; j < 3; j++)
                        {
                            for (int k = 0; k < 3; k++)
                            {
                                float eps_ijk = Epsilon(i, j, k);
                                gauge_correction += G_WEAK * eps_ijk *
                                    lattice->fields[x][y].W[j][0] * (-lattice->strengths[x][y].F_SU2[k]);
                            }
                        }

                        F_i_mu_nu = dF_dx + gauge_correction;
                    }

                    float delta = -dt * F_i_mu_nu / (sqrt_g + 1e-8f);
                    temp.fields[x][y].W[i][mu] = damping * lattice->fields[x][y].W[i][mu] + delta;
                }
            }

            // ─────────────────────────────────────────────────
            // U(1) field evolution (Abelian, simpler)
            // ─────────────────────────────────────────────────
            for (int mu = 0; mu < 2; mu++)
            {
                float F_mu_nu = 0.0f;

                if (mu == 0)  // B_x
                {
                    float F_xy = lattice->strengths[x][y].F_U1;
                    float F_xy_yp = lattice->strengths[x][y+1].F_U1;
                    float F_xy_ym = lattice->strengths[x][y-1].F_U1;
                    F_mu_nu = (F_xy_yp - F_xy_ym) / (2.0f * dx);
                }
                else  // B_y
                {
                    float F_yx = -lattice->strengths[x][y].F_U1;
                    float F_yx_xp = -lattice->strengths[x+1][y].F_U1;
                    float F_yx_xm = -lattice->strengths[x-1][y].F_U1;
                    F_mu_nu = (F_yx_xp - F_yx_xm) / (2.0f * dx);
                }

                float delta = -dt * F_mu_nu / (sqrt_g + 1e-8f);
                temp.fields[x][y].B[mu] = damping * lattice->fields[x][y].B[mu] + delta;
            }
        }
    }

    // Copy updated fields back
    memcpy(lattice->fields, temp.fields, sizeof(lattice->fields));
}

// ═══════════════════════════════════════════════════════════
// PARALLEL TRANSPORT (Matter field evolution)
// ═══════════════════════════════════════════════════════════

internal_fnc void ParallelTransportQuark(quark_state* quark, gauge_field_point* field, float dt)
{
    // D_μ ψ = (∂_μ + ig_s A^{SU(3)}_μ + ig_w A^{SU(2)}_μ + ig' A^{U(1)}_μ) ψ
    // Update: ψ(t + dt) = ψ(t) - i * A_μ(v^μ) * ψ(t) * dt

    float vx = quark->vx;
    float vy = quark->vy;

    // SU(3) contribution: rotate color state
    for (int c = 0; c < 3; c++)
    {
        float phase_shift = 0.0f;
        for (int a = 0; a < 8; a++)
        {
            // Simplified: A^a_μ v^μ contribution
            phase_shift += G_STRONG * (field->G[a][0] * vx + field->G[a][1] * vy);
        }

        // Rotate complex amplitude by phase
        float old_real = quark->color[c][0];
        float old_imag = quark->color[c][1];
        float cos_theta = cosf(phase_shift * dt);
        float sin_theta = sinf(phase_shift * dt);

        quark->color[c][0] = old_real * cos_theta - old_imag * sin_theta;
        quark->color[c][1] = old_real * sin_theta + old_imag * cos_theta;
    }

    // SU(2) contribution: rotate weak isospin state
    for (int w = 0; w < 2; w++)
    {
        float phase_shift = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            phase_shift += G_WEAK * (field->W[i][0] * vx + field->W[i][1] * vy);
        }

        float old_real = quark->weak[w][0];
        float old_imag = quark->weak[w][1];
        float cos_theta = cosf(phase_shift * dt);
        float sin_theta = sinf(phase_shift * dt);

        quark->weak[w][0] = old_real * cos_theta - old_imag * sin_theta;
        quark->weak[w][1] = old_real * sin_theta + old_imag * cos_theta;
    }

    // U(1) contribution: rotate hypercharge phase
    float phase_shift_U1 = G_EM * (field->B[0] * vx + field->B[1] * vy);
    quark->hypercharge += phase_shift_U1 * dt;

    // Normalize hypercharge to [0, 2π)
    while (quark->hypercharge > 2.0f * PI32) quark->hypercharge -= 2.0f * PI32;
    while (quark->hypercharge < 0.0f) quark->hypercharge += 2.0f * PI32;
}

internal_fnc void ParallelTransportLepton(lepton_state* lepton, gauge_field_point* field, float dt)
{
    // Leptons: no color (SU(3) singlet), but have weak isospin and hypercharge

    float vx = lepton->vx;
    float vy = lepton->vy;

    // SU(2) weak isospin
    for (int w = 0; w < 2; w++)
    {
        float phase_shift = 0.0f;
        for (int i = 0; i < 3; i++)
        {
            phase_shift += G_WEAK * (field->W[i][0] * vx + field->W[i][1] * vy);
        }

        float old_real = lepton->weak[w][0];
        float old_imag = lepton->weak[w][1];
        float cos_theta = cosf(phase_shift * dt);
        float sin_theta = sinf(phase_shift * dt);

        lepton->weak[w][0] = old_real * cos_theta - old_imag * sin_theta;
        lepton->weak[w][1] = old_real * sin_theta + old_imag * cos_theta;
    }

    // U(1) hypercharge
    float phase_shift_U1 = G_EM * (field->B[0] * vx + field->B[1] * vy);
    lepton->hypercharge += phase_shift_U1 * dt;

    while (lepton->hypercharge > 2.0f * PI32) lepton->hypercharge -= 2.0f * PI32;
    while (lepton->hypercharge < 0.0f) lepton->hypercharge += 2.0f * PI32;
}

// ═══════════════════════════════════════════════════════════
// PARTICLE DYNAMICS
// ═══════════════════════════════════════════════════════════

internal_fnc void UpdateQuarkDynamics(gauge_theory_state* state, float dt)
{
    gauge_field_lattice* lattice = state->lattice;

    for (int i = 0; i < state->quark_count; i++)
    {
        quark_state* quark = &state->quarks[i];

        // Sample gauge field and metric at particle position
        gauge_field_point field = SampleGaugeField(lattice, quark->x, quark->y);
        metric_tensor g = SampleMetric(lattice, quark->x, quark->y);

        // 1. Parallel transport (gauge field interaction)
        ParallelTransportQuark(quark, &field, dt);

        // 2. Geodesic motion in curved spacetime
        // For simplicity: modify velocity based on metric connection
        // Simplified: v^μ_new = v^μ - Γ^μ_ρσ v^ρ v^σ dt
        // (Full Christoffel symbol computation omitted for brevity)

        // 3. Update position
        quark->x += quark->vx * dt;
        quark->y += quark->vy * dt;

        // Periodic boundary conditions
        float width = (float)GAUGE_LATTICE_WIDTH * LATTICE_SPACING;
        float height = (float)GAUGE_LATTICE_HEIGHT * LATTICE_SPACING;
        if (quark->x < 0.0f) quark->x += width;
        if (quark->x >= width) quark->x -= width;
        if (quark->y < 0.0f) quark->y += height;
        if (quark->y >= height) quark->y -= height;
    }
}

internal_fnc void UpdateLeptonDynamics(gauge_theory_state* state, float dt)
{
    gauge_field_lattice* lattice = state->lattice;

    for (int i = 0; i < state->lepton_count; i++)
    {
        lepton_state* lepton = &state->leptons[i];

        gauge_field_point field = SampleGaugeField(lattice, lepton->x, lepton->y);
        metric_tensor g = SampleMetric(lattice, lepton->x, lepton->y);

        ParallelTransportLepton(lepton, &field, dt);

        lepton->x += lepton->vx * dt;
        lepton->y += lepton->vy * dt;

        float width = (float)GAUGE_LATTICE_WIDTH * LATTICE_SPACING;
        float height = (float)GAUGE_LATTICE_HEIGHT * LATTICE_SPACING;
        if (lepton->x < 0.0f) lepton->x += width;
        if (lepton->x >= width) lepton->x -= width;
        if (lepton->y < 0.0f) lepton->y += height;
        if (lepton->y >= height) lepton->y -= height;
    }
}

// ═══════════════════════════════════════════════════════════
// SAMPLING (Bilinear interpolation)
// ═══════════════════════════════════════════════════════════

internal_fnc gauge_field_point SampleGaugeField(gauge_field_lattice* lattice, float x, float y)
{
    // Convert to lattice coordinates
    float lx = x / LATTICE_SPACING;
    float ly = y / LATTICE_SPACING;

    int x0 = (int)lx;
    int y0 = (int)ly;
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // Clamp to lattice bounds
    x0 = (x0 < 0) ? 0 : (x0 >= GAUGE_LATTICE_WIDTH) ? GAUGE_LATTICE_WIDTH - 1 : x0;
    x1 = (x1 < 0) ? 0 : (x1 >= GAUGE_LATTICE_WIDTH) ? GAUGE_LATTICE_WIDTH - 1 : x1;
    y0 = (y0 < 0) ? 0 : (y0 >= GAUGE_LATTICE_HEIGHT) ? GAUGE_LATTICE_HEIGHT - 1 : y0;
    y1 = (y1 < 0) ? 0 : (y1 >= GAUGE_LATTICE_HEIGHT) ? GAUGE_LATTICE_HEIGHT - 1 : y1;

    float tx = lx - (float)x0;
    float ty = ly - (float)y0;

    // Bilinear interpolation
    gauge_field_point result = {0};

    gauge_field_point* f00 = &lattice->fields[x0][y0];
    gauge_field_point* f10 = &lattice->fields[x1][y0];
    gauge_field_point* f01 = &lattice->fields[x0][y1];
    gauge_field_point* f11 = &lattice->fields[x1][y1];

    for (int a = 0; a < 8; a++)
    {
        for (int mu = 0; mu < 2; mu++)
        {
            result.G[a][mu] = (1.0f - tx) * (1.0f - ty) * f00->G[a][mu]
                            + tx * (1.0f - ty) * f10->G[a][mu]
                            + (1.0f - tx) * ty * f01->G[a][mu]
                            + tx * ty * f11->G[a][mu];
        }
    }

    for (int i = 0; i < 3; i++)
    {
        for (int mu = 0; mu < 2; mu++)
        {
            result.W[i][mu] = (1.0f - tx) * (1.0f - ty) * f00->W[i][mu]
                            + tx * (1.0f - ty) * f10->W[i][mu]
                            + (1.0f - tx) * ty * f01->W[i][mu]
                            + tx * ty * f11->W[i][mu];
        }
    }

    for (int mu = 0; mu < 2; mu++)
    {
        result.B[mu] = (1.0f - tx) * (1.0f - ty) * f00->B[mu]
                     + tx * (1.0f - ty) * f10->B[mu]
                     + (1.0f - tx) * ty * f01->B[mu]
                     + tx * ty * f11->B[mu];
    }

    return result;
}

internal_fnc metric_tensor SampleMetric(gauge_field_lattice* lattice, float x, float y)
{
    float lx = x / LATTICE_SPACING;
    float ly = y / LATTICE_SPACING;

    int x0 = (int)lx;
    int y0 = (int)ly;
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    x0 = (x0 < 0) ? 0 : (x0 >= GAUGE_LATTICE_WIDTH) ? GAUGE_LATTICE_WIDTH - 1 : x0;
    x1 = (x1 < 0) ? 0 : (x1 >= GAUGE_LATTICE_WIDTH) ? GAUGE_LATTICE_WIDTH - 1 : x1;
    y0 = (y0 < 0) ? 0 : (y0 >= GAUGE_LATTICE_HEIGHT) ? GAUGE_LATTICE_HEIGHT - 1 : y0;
    y1 = (y1 < 0) ? 0 : (y1 >= GAUGE_LATTICE_HEIGHT) ? GAUGE_LATTICE_HEIGHT - 1 : y1;

    float tx = lx - (float)x0;
    float ty = ly - (float)y0;

    metric_tensor* g00 = &lattice->metrics[x0][y0];
    metric_tensor* g10 = &lattice->metrics[x1][y0];
    metric_tensor* g01 = &lattice->metrics[x0][y1];
    metric_tensor* g11 = &lattice->metrics[x1][y1];

    metric_tensor result;
    result.g11 = (1.0f - tx) * (1.0f - ty) * g00->g11
               + tx * (1.0f - ty) * g10->g11
               + (1.0f - tx) * ty * g01->g11
               + tx * ty * g11->g11;

    result.g12 = (1.0f - tx) * (1.0f - ty) * g00->g12
               + tx * (1.0f - ty) * g10->g12
               + (1.0f - tx) * ty * g01->g12
               + tx * ty * g11->g12;

    result.g22 = (1.0f - tx) * (1.0f - ty) * g00->g22
               + tx * (1.0f - ty) * g10->g22
               + (1.0f - tx) * ty * g01->g22
               + tx * ty * g11->g22;

    return result;
}

// ═══════════════════════════════════════════════════════════
// INITIALIZATION PATTERNS
// ═══════════════════════════════════════════════════════════

internal_fnc void SetUniformGaugeField(gauge_field_lattice* lattice, int field_type)
{
    // Set a constant background field
    // field_type: 0=SU3, 1=SU2, 2=U1

    float field_strength = 0.1f;

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            if (field_type == 0)
            {
                // Uniform SU(3) field in first component
                lattice->fields[x][y].G[0][0] = field_strength;
            }
            else if (field_type == 1)
            {
                // Uniform SU(2) field in first component
                lattice->fields[x][y].W[0][0] = field_strength;
            }
            else if (field_type == 2)
            {
                // Uniform U(1) field (constant B_x)
                lattice->fields[x][y].B[0] = field_strength;
            }
        }
    }
}

internal_fnc void SetDipoleGaugeField(gauge_field_lattice* lattice, float x1, float y1, float x2, float y2)
{
    // Create a dipole field: positive charge at (x1,y1), negative at (x2,y2)
    // For U(1) photon field as example

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            float px = (float)x * LATTICE_SPACING;
            float py = (float)y * LATTICE_SPACING;

            // Vector from charges to point
            float r1_x = px - x1;
            float r1_y = py - y1;
            float r1_sq = r1_x*r1_x + r1_y*r1_y + 1e-4f;

            float r2_x = px - x2;
            float r2_y = py - y2;
            float r2_sq = r2_x*r2_x + r2_y*r2_y + 1e-4f;

            // A ~ (1/r) for point charge in 2D
            lattice->fields[x][y].B[0] = -r1_y / r1_sq + r2_y / r2_sq;
            lattice->fields[x][y].B[1] =  r1_x / r1_sq - r2_x / r2_sq;
        }
    }
}

internal_fnc void SetVortexGaugeField(gauge_field_lattice* lattice, float cx, float cy, float strength)
{
    // Create vortex configuration centered at (cx, cy)
    // A_θ = strength / r (azimuthal field)

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            float px = (float)x * LATTICE_SPACING;
            float py = (float)y * LATTICE_SPACING;

            float dx = px - cx;
            float dy = py - cy;
            float r = sqrtf(dx*dx + dy*dy + 1e-4f);

            // Convert (dx, dy) to azimuthal direction (-dy/r, dx/r)
            float A_x = -strength * dy / (r*r);
            float A_y =  strength * dx / (r*r);

            // Assign to photon field as example
            lattice->fields[x][y].B[0] = A_x;
            lattice->fields[x][y].B[1] = A_y;
        }
    }
}

internal_fnc void AddGaugeFieldNoise(gauge_field_lattice* lattice, float amplitude)
{
    // Add random fluctuations to all gauge fields

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            // Simple pseudo-random (not high quality, but sufficient for visualization)
            uint32_t seed = x * 73856093 + y * 19349663;

            for (int a = 0; a < 8; a++)
            {
                seed = seed * 1664525 + 1013904223;
                float rand1 = ((float)(seed & 0xFFFF) / 65536.0f) * 2.0f - 1.0f;
                seed = seed * 1664525 + 1013904223;
                float rand2 = ((float)(seed & 0xFFFF) / 65536.0f) * 2.0f - 1.0f;

                lattice->fields[x][y].G[a][0] += amplitude * rand1;
                lattice->fields[x][y].G[a][1] += amplitude * rand2;
            }

            for (int i = 0; i < 3; i++)
            {
                seed = seed * 1664525 + 1013904223;
                float rand1 = ((float)(seed & 0xFFFF) / 65536.0f) * 2.0f - 1.0f;
                seed = seed * 1664525 + 1013904223;
                float rand2 = ((float)(seed & 0xFFFF) / 65536.0f) * 2.0f - 1.0f;

                lattice->fields[x][y].W[i][0] += amplitude * rand1;
                lattice->fields[x][y].W[i][1] += amplitude * rand2;
            }

            seed = seed * 1664525 + 1013904223;
            float rand1 = ((float)(seed & 0xFFFF) / 65536.0f) * 2.0f - 1.0f;
            seed = seed * 1664525 + 1013904223;
            float rand2 = ((float)(seed & 0xFFFF) / 65536.0f) * 2.0f - 1.0f;

            lattice->fields[x][y].B[0] += amplitude * rand1;
            lattice->fields[x][y].B[1] += amplitude * rand2;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// OBSERVABLES
// ═══════════════════════════════════════════════════════════

internal_fnc float ComputeAverageCurvature(gauge_field_lattice* lattice)
{
    float sum = 0.0f;
    int count = 0;

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            sum += lattice->curvature[x][y].R;
            count++;
        }
    }

    return sum / (float)count;
}

internal_fnc float ComputeTotalAction(gauge_theory_state* state)
{
    float action = 0.0f;
    gauge_field_lattice* lattice = state->lattice;

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            field_strength* F = &lattice->strengths[x][y];

            // Gauge kinetic term: Σ F^2
            for (int a = 0; a < 8; a++) action += F->F_SU3[a] * F->F_SU3[a];
            for (int i = 0; i < 3; i++) action += F->F_SU2[i] * F->F_SU2[i];
            action += F->F_U1 * F->F_U1;
        }
    }

    return action * LATTICE_SPACING * LATTICE_SPACING; // Volume element
}

internal_fnc float ComputeWilsonLoop(gauge_field_lattice* lattice, int x0, int y0, int width, int height)
{
    // Compute Wilson loop: W(C) = Tr[P exp(i ∮_C A·dx)]
    // For U(1) field, this simplifies to: W = exp(i ∮ B·dx)
    // For non-abelian, need path-ordered exponential

    // We'll compute for U(1) photon field as a simple case
    // Full non-abelian version requires matrix path ordering

    float phase_accumulation = 0.0f;

    // Bottom edge (left to right)
    for (int x = x0; x < x0 + width; x++)
    {
        if (x >= 0 && x < GAUGE_LATTICE_WIDTH && y0 >= 0 && y0 < GAUGE_LATTICE_HEIGHT)
        {
            phase_accumulation += lattice->fields[x][y0].B[0] * LATTICE_SPACING;
        }
    }

    // Right edge (bottom to top)
    for (int y = y0; y < y0 + height; y++)
    {
        int x_right = x0 + width;
        if (x_right >= 0 && x_right < GAUGE_LATTICE_WIDTH && y >= 0 && y < GAUGE_LATTICE_HEIGHT)
        {
            phase_accumulation += lattice->fields[x_right][y].B[1] * LATTICE_SPACING;
        }
    }

    // Top edge (right to left) - opposite direction
    for (int x = x0 + width; x > x0; x--)
    {
        int y_top = y0 + height;
        if (x >= 0 && x < GAUGE_LATTICE_WIDTH && y_top >= 0 && y_top < GAUGE_LATTICE_HEIGHT)
        {
            phase_accumulation -= lattice->fields[x][y_top].B[0] * LATTICE_SPACING;
        }
    }

    // Left edge (top to bottom) - opposite direction
    for (int y = y0 + height; y > y0; y--)
    {
        if (x0 >= 0 && x0 < GAUGE_LATTICE_WIDTH && y >= 0 && y < GAUGE_LATTICE_HEIGHT)
        {
            phase_accumulation -= lattice->fields[x0][y].B[1] * LATTICE_SPACING;
        }
    }

    // Wilson loop for U(1): W = Re[exp(i*phase)]
    // For gauge-invariant observable, we take the real part
    return cosf(phase_accumulation);
}
