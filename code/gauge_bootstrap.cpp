#include "gauge_theory.h"
#include "game.h"
#include <math.h>
#include <string.h>

/*
    BOOTSTRAP ANALYSIS MODULE

    Measures self-consistency of gauge ⇄ geometry duality:
    - How much gauge fields determine metric
    - How much metric determines gauge evolution
    - κζ computed from both gauge and metric eigenvalues
    - Convergence to bootstrap fixed point
*/

// ═══════════════════════════════════════════════════════════
// BOOTSTRAP PARAMETER COMPUTATION
// ═══════════════════════════════════════════════════════════

struct bootstrap_metrics
{
    float beta;                    // Overall bootstrap parameter
    float gauge_to_geometry;       // Coupling strength: gauge → metric
    float geometry_to_gauge;       // Coupling strength: metric → gauge
    float kappa_zeta_gauge;        // κζ from gauge field eigenvalues
    float kappa_zeta_metric;       // κζ from metric eigenvalues
    float kappa_zeta_difference;   // |κζ_gauge - κζ_metric|
    bool is_fixed_point;           // Within tolerance of β = 1
    float avg_curvature;           // Mean Ricci scalar
    float total_action;            // Sum of gauge field energy
};

internal_fnc float ComputeTensorNorm(float T11, float T12, float T22)
{
    // Frobenius norm of 2×2 symmetric tensor
    return sqrtf(T11*T11 + 2.0f*T12*T12 + T22*T22);
}

internal_fnc float ComputeMetricDeviation(metric_tensor* g)
{
    // Measure ||g - η|| where η = identity
    float g11_dev = g->g11 - 1.0f;
    float g12_dev = g->g12;
    float g22_dev = g->g22 - 1.0f;

    return sqrtf(g11_dev*g11_dev + 2.0f*g12_dev*g12_dev + g22_dev*g22_dev);
}

internal_fnc bootstrap_metrics ComputeBootstrapMetrics(gauge_theory_state* state)
{
    bootstrap_metrics result = {0};
    gauge_field_lattice* lattice = state->lattice;

    // 1. Measure gauge → geometry coupling
    float gauge_to_geom_sum = 0.0f;
    int count = 0;

    for (int y = 1; y < GAUGE_LATTICE_HEIGHT - 1; y++)
    {
        for (int x = 1; x < GAUGE_LATTICE_WIDTH - 1; x++)
        {
            stress_energy_tensor* T = &lattice->stress[x][y];
            metric_tensor* g = &lattice->metrics[x][y];

            float T_norm = ComputeTensorNorm(T->T11, T->T12, T->T22);
            float g_deviation = ComputeMetricDeviation(g);

            if (T_norm > 1e-8f)
            {
                // How much does stress-energy induce metric deviation?
                gauge_to_geom_sum += g_deviation / T_norm;
                count++;
            }
        }
    }

    result.gauge_to_geometry = (count > 0) ? (gauge_to_geom_sum / count) : 0.0f;

    // 2. Measure geometry → gauge coupling (via curvature influence on fields)
    float geom_to_gauge_sum = 0.0f;
    int count2 = 0;

    for (int y = 1; y < GAUGE_LATTICE_HEIGHT - 1; y++)
    {
        for (int x = 1; x < GAUGE_LATTICE_WIDTH - 1; x++)
        {
            spacetime_curvature* curv = &lattice->curvature[x][y];
            field_strength* F = &lattice->strengths[x][y];

            float R = fabsf(curv->R);

            // Total field strength
            float F_total = 0.0f;
            for (int a = 0; a < 8; a++) F_total += fabsf(F->F_SU3[a]);
            for (int i = 0; i < 3; i++) F_total += fabsf(F->F_SU2[i]);
            F_total += fabsf(F->F_U1);

            if (R > 1e-8f)
            {
                // How much does curvature correlate with field strength?
                geom_to_gauge_sum += F_total / R;
                count2++;
            }
        }
    }

    result.geometry_to_gauge = (count2 > 0) ? (geom_to_gauge_sum / count2) : 0.0f;

    // 3. Bootstrap parameter β = √(G→M coupling × M→G coupling)
    result.beta = sqrtf(result.gauge_to_geometry * result.geometry_to_gauge);

    // 4. Compute κζ from gauge field eigenvalues
    result.kappa_zeta_gauge = ComputeKappaZetaFromGaugeFields(lattice);

    // 5. Compute κζ from metric eigenvalues
    result.kappa_zeta_metric = ComputeKappaZetaFromMetric(lattice);

    // 6. Duality measure
    result.kappa_zeta_difference = fabsf(result.kappa_zeta_gauge - result.kappa_zeta_metric);

    // 7. Fixed point test (β ≈ 1 within tolerance)
    result.is_fixed_point = (fabsf(result.beta - 1.0f) < 0.1f);

    // 8. Additional diagnostics
    result.avg_curvature = ComputeAverageCurvature(lattice);
    result.total_action = ComputeTotalAction(state);

    return result;
}

// ═══════════════════════════════════════════════════════════
// κζ FROM GAUGE FIELDS (Stress-Energy Eigenvalues)
// ═══════════════════════════════════════════════════════════

internal_fnc float ComputeKappaZetaFromGaugeFields(gauge_field_lattice* lattice)
{
    // Construct distribution of stress-energy eigenvalues
    float eigenvalues[GAUGE_LATTICE_WIDTH * GAUGE_LATTICE_HEIGHT];
    int count = 0;

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            stress_energy_tensor* T = &lattice->stress[x][y];

            // For 2×2 symmetric matrix, eigenvalues are:
            // λ = (T11 + T22)/2 ± √[(T11 - T22)²/4 + T12²]
            float trace = T->T11 + T->T22;
            float det = T->T11 * T->T22 - T->T12 * T->T12;
            float discriminant = trace*trace/4.0f - det;

            if (discriminant >= 0.0f)
            {
                float sqrt_disc = sqrtf(discriminant);
                float lambda1 = trace/2.0f + sqrt_disc;
                float lambda2 = trace/2.0f - sqrt_disc;

                // Store both eigenvalues
                if (lambda1 > 1e-8f) eigenvalues[count++] = lambda1;
                if (lambda2 > 1e-8f) eigenvalues[count++] = lambda2;
            }
        }
    }

    if (count == 0) return 0.0f;

    // Compute Mellin moments
    float tau = 1.5f;
    float sigma = 0.5f;

    float M_tau = 0.0f;
    float M_sigma = 0.0f;

    for (int i = 0; i < count; i++)
    {
        M_tau += powf(eigenvalues[i], tau);
        M_sigma += powf(eigenvalues[i], -sigma);
    }

    M_tau /= count;
    M_sigma /= count;

    return (M_sigma > 1e-8f) ? (M_tau / M_sigma) : 0.0f;
}

// ═══════════════════════════════════════════════════════════
// κζ FROM METRIC (Metric Eigenvalues)
// ═══════════════════════════════════════════════════════════

internal_fnc float ComputeKappaZetaFromMetric(gauge_field_lattice* lattice)
{
    // Construct distribution of metric eigenvalues
    float eigenvalues[GAUGE_LATTICE_WIDTH * GAUGE_LATTICE_HEIGHT];
    int count = 0;

    for (int y = 0; y < GAUGE_LATTICE_HEIGHT; y++)
    {
        for (int x = 0; x < GAUGE_LATTICE_WIDTH; x++)
        {
            metric_tensor* g = &lattice->metrics[x][y];

            // For 2×2 symmetric matrix
            float trace = g->g11 + g->g22;
            float det = g->g11 * g->g22 - g->g12 * g->g12;
            float discriminant = trace*trace/4.0f - det;

            if (discriminant >= 0.0f && det > 1e-8f)
            {
                float sqrt_disc = sqrtf(discriminant);
                float lambda1 = trace/2.0f + sqrt_disc;
                float lambda2 = trace/2.0f - sqrt_disc;

                if (lambda1 > 1e-8f) eigenvalues[count++] = lambda1;
                if (lambda2 > 1e-8f) eigenvalues[count++] = lambda2;
            }
        }
    }

    if (count == 0) return 0.0f;

    // Compute Mellin moments
    float tau = 1.5f;
    float sigma = 0.5f;

    float M_tau = 0.0f;
    float M_sigma = 0.0f;

    for (int i = 0; i < count; i++)
    {
        M_tau += powf(eigenvalues[i], tau);
        M_sigma += powf(eigenvalues[i], -sigma);
    }

    M_tau /= count;
    M_sigma /= count;

    return (M_sigma > 1e-8f) ? (M_tau / M_sigma) : 0.0f;
}

// ═══════════════════════════════════════════════════════════
// CONVERGENCE ANALYSIS
// ═══════════════════════════════════════════════════════════

#define MAX_HISTORY 1000

struct convergence_history
{
    float beta_history[MAX_HISTORY];
    float kz_diff_history[MAX_HISTORY];
    float curvature_history[MAX_HISTORY];
    int current_step;
    bool converged;
    float convergence_rate;
};

internal_fnc void InitConvergenceHistory(convergence_history* history)
{
    memset(history, 0, sizeof(convergence_history));
}

internal_fnc void UpdateConvergenceHistory(convergence_history* history, bootstrap_metrics metrics)
{
    if (history->current_step >= MAX_HISTORY) return;

    int idx = history->current_step;
    history->beta_history[idx] = metrics.beta;
    history->kz_diff_history[idx] = metrics.kappa_zeta_difference;
    history->curvature_history[idx] = metrics.avg_curvature;

    history->current_step++;

    // Check for convergence (last 100 steps)
    if (history->current_step > 100)
    {
        float recent_variance = 0.0f;
        float recent_mean = 0.0f;

        for (int i = history->current_step - 100; i < history->current_step; i++)
        {
            recent_mean += history->beta_history[i];
        }
        recent_mean /= 100.0f;

        for (int i = history->current_step - 100; i < history->current_step; i++)
        {
            float dev = history->beta_history[i] - recent_mean;
            recent_variance += dev * dev;
        }
        recent_variance /= 100.0f;

        // Converged if variance is very small
        history->converged = (recent_variance < 0.001f);

        // Estimate convergence rate (exponential decay constant)
        if (history->current_step > 10)
        {
            float initial = history->beta_history[0];
            float current = history->beta_history[history->current_step - 1];
            history->convergence_rate = -logf(fabsf(current - 1.0f) / fabsf(initial - 1.0f + 1e-8f)) / history->current_step;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// LOGGING AND DIAGNOSTICS
// ═══════════════════════════════════════════════════════════

internal_fnc void PrintBootstrapMetrics(bootstrap_metrics metrics)
{
    printf("\n=== BOOTSTRAP METRICS ===\n");
    printf("Bootstrap parameter β:     %.4f %s\n", metrics.beta,
           metrics.is_fixed_point ? "[FIXED POINT]" : "");
    printf("  Gauge → Geometry:        %.4f\n", metrics.gauge_to_geometry);
    printf("  Geometry → Gauge:        %.4f\n", metrics.geometry_to_gauge);
    printf("\n");
    printf("κζ from gauge fields:      %.4f\n", metrics.kappa_zeta_gauge);
    printf("κζ from metric:            %.4f\n", metrics.kappa_zeta_metric);
    printf("κζ duality difference:     %.6f\n", metrics.kappa_zeta_difference);
    printf("\n");
    printf("Average curvature R:       %.6f\n", metrics.avg_curvature);
    printf("Total action S:            %.6f\n", metrics.total_action);
    printf("========================\n\n");
}

#if DEV_BUILD
internal_fnc void LogBootstrapMetrics(bootstrap_metrics metrics, const char* filename)
{
    FILE* f = fopen(filename, "a");
    if (f)
    {
        fprintf(f, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                metrics.beta,
                metrics.gauge_to_geometry,
                metrics.geometry_to_gauge,
                metrics.kappa_zeta_gauge,
                metrics.kappa_zeta_metric,
                metrics.kappa_zeta_difference,
                metrics.avg_curvature,
                metrics.total_action);
        fclose(f);
    }
}
#endif

// ═══════════════════════════════════════════════════════════
// PHASE DIAGRAM ANALYSIS
// ═══════════════════════════════════════════════════════════

enum bootstrap_phase
{
    PHASE_SUBCRITICAL,   // β < 1: decays to flat spacetime
    PHASE_CRITICAL,      // β ≈ 1: self-sustaining
    PHASE_SUPERCRITICAL  // β > 1: runaway instability
};

internal_fnc bootstrap_phase ClassifyBootstrapPhase(float beta, float tolerance)
{
    if (beta < 1.0f - tolerance) return PHASE_SUBCRITICAL;
    if (beta > 1.0f + tolerance) return PHASE_SUPERCRITICAL;
    return PHASE_CRITICAL;
}

internal_fnc const char* GetPhaseName(bootstrap_phase phase)
{
    switch (phase)
    {
        case PHASE_SUBCRITICAL: return "Subcritical (decay)";
        case PHASE_CRITICAL: return "Critical (fixed point)";
        case PHASE_SUPERCRITICAL: return "Supercritical (runaway)";
        default: return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════
// EXPERIMENTAL PROTOCOLS
// ═══════════════════════════════════════════════════════════

internal_fnc void RunBootstrapExperiment(gauge_theory_state* state, int num_steps)
{
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║   BOOTSTRAP FIXED POINT EXPERIMENT                  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    convergence_history history;
    InitConvergenceHistory(&history);

    printf("Running %d time steps...\n", num_steps);

    for (int step = 0; step < num_steps; step++)
    {
        // Update physics
        ComputeFieldStrengths(state->lattice);
        ComputeStressEnergy(state->lattice);
        InduceMetricFromGaugeFields(state->lattice, state->gauge_coupling_alpha);
        ComputeCurvature(state->lattice);
        UpdateQuarkDynamics(state, state->dt);
        UpdateLeptonDynamics(state, state->dt);

        // Measure bootstrap metrics
        bootstrap_metrics metrics = ComputeBootstrapMetrics(state);
        UpdateConvergenceHistory(&history, metrics);

        // Print progress every 100 steps
        if (step % 100 == 0)
        {
            printf("Step %4d: β=%.4f, Δκζ=%.6f, R_avg=%.6f %s\n",
                   step, metrics.beta, metrics.kappa_zeta_difference,
                   metrics.avg_curvature,
                   metrics.is_fixed_point ? "✓" : " ");
        }

        #if DEV_BUILD
        // Log to file
        if (step % 10 == 0)
        {
            LogBootstrapMetrics(metrics, "bootstrap_log.csv");
        }
        #endif
    }

    // Final analysis
    printf("\n=== CONVERGENCE ANALYSIS ===\n");
    printf("Converged: %s\n", history.converged ? "YES" : "NO");
    printf("Convergence rate: %.6f\n", history.convergence_rate);

    bootstrap_metrics final_metrics = ComputeBootstrapMetrics(state);
    bootstrap_phase phase = ClassifyBootstrapPhase(final_metrics.beta, 0.1f);
    printf("Final phase: %s\n", GetPhaseName(phase));

    PrintBootstrapMetrics(final_metrics);
}
