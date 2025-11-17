#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

// Simple κζ calculator for gauge theory eigenvalues

float compute_kappa_zeta(float* eigenvalues, int count)
{
    if (count == 0) return 0.0f;

    float tau = 1.5f;
    float sigma = 0.5f;

    float M_tau = 0.0f;
    float M_sigma = 0.0f;

    for (int i = 0; i < count; i++)
    {
        M_tau += powf(eigenvalues[i], tau);      // λ^1.5
        M_sigma += powf(eigenvalues[i], -sigma); // λ^(-0.5)
    }

    M_tau /= count;
    M_sigma /= count;

    printf("  M_τ (⟨λ^1.5⟩)  = %.6f\n", M_tau);
    printf("  M_σ (⟨λ^-0.5⟩) = %.6f\n", M_sigma);

    return (M_sigma > 1e-8f) ? (M_tau / M_sigma) : 0.0f;
}

void simulate_stress_energy_eigenvalues(float* eigenvalues, int* count, int lattice_size)
{
    // Simulate a typical stress-energy distribution from gauge fields
    // T_μν has eigenvalues that scale with field strength squared

    *count = 0;
    srand(time(NULL));

    for (int i = 0; i < lattice_size; i++)
    {
        // Each lattice point contributes 2 eigenvalues (2×2 tensor)
        // Simulate field strength ~ 0.01 to 0.5
        float field_strength = 0.01f + (rand() / (float)RAND_MAX) * 0.49f;

        // T ~ F², so eigenvalues ~ F²
        float base_energy = field_strength * field_strength;

        // Add some variation (anisotropy)
        float lambda1 = base_energy * (0.8f + 0.4f * (rand() / (float)RAND_MAX));
        float lambda2 = base_energy * (0.8f + 0.4f * (rand() / (float)RAND_MAX));

        if (lambda1 > 1e-8f) eigenvalues[(*count)++] = lambda1;
        if (lambda2 > 1e-8f) eigenvalues[(*count)++] = lambda2;
    }
}

void simulate_metric_eigenvalues(float* eigenvalues, int* count, int lattice_size, float coupling_alpha)
{
    // Metric eigenvalues from g = η + α·T
    // Typically close to 1 (flat space) with small deviations

    *count = 0;
    srand(time(NULL) + 1);

    for (int i = 0; i < lattice_size; i++)
    {
        // Base metric is Minkowski (eigenvalues = 1)
        // Perturbations from stress-energy
        float T_eigen = 0.0001f + (rand() / (float)RAND_MAX) * 0.01f;

        float lambda1 = 1.0f + coupling_alpha * T_eigen;
        float lambda2 = 1.0f + coupling_alpha * T_eigen * (0.5f + 0.5f * (rand() / (float)RAND_MAX));

        if (lambda1 > 1e-8f) eigenvalues[(*count)++] = lambda1;
        if (lambda2 > 1e-8f) eigenvalues[(*count)++] = lambda2;
    }
}

int main()
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║          κζ (KAPPA ZETA) CALCULATOR                  ║\n");
    printf("║    SU(3)×SU(2)×U(1) Gauge Theory Testbed            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    printf("Formula: κζ = M_τ / M_σ\n");
    printf("  where M_τ = ⟨λ^1.5⟩  (tau moment)\n");
    printf("        M_σ = ⟨λ^-0.5⟩ (sigma moment)\n\n");

    const int LATTICE_SIZE = 128 * 128; // 128×128 lattice
    const int MAX_EIGENVALUES = LATTICE_SIZE * 2;

    float* gauge_eigenvalues = (float*)malloc(MAX_EIGENVALUES * sizeof(float));
    float* metric_eigenvalues = (float*)malloc(MAX_EIGENVALUES * sizeof(float));

    int gauge_count, metric_count;

    // ═══════════════════════════════════════════════════════════
    // SCENARIO 1: Weak Coupling (α = 0.01)
    // ═══════════════════════════════════════════════════════════
    printf("═══════════════════════════════════════════════════════════\n");
    printf("SCENARIO 1: WEAK COUPLING (α = 0.01)\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    float alpha_weak = 0.01f;

    simulate_stress_energy_eigenvalues(gauge_eigenvalues, &gauge_count, LATTICE_SIZE);
    printf("Computing κζ from GAUGE FIELDS (stress-energy eigenvalues):\n");
    printf("  Number of eigenvalues: %d\n", gauge_count);
    float kz_gauge_weak = compute_kappa_zeta(gauge_eigenvalues, gauge_count);
    printf("  → κζ_gauge = %.6f\n\n", kz_gauge_weak);

    simulate_metric_eigenvalues(metric_eigenvalues, &metric_count, LATTICE_SIZE, alpha_weak);
    printf("Computing κζ from METRIC (geometry eigenvalues):\n");
    printf("  Number of eigenvalues: %d\n", metric_count);
    float kz_metric_weak = compute_kappa_zeta(metric_eigenvalues, metric_count);
    printf("  → κζ_metric = %.6f\n\n", kz_metric_weak);

    float duality_diff_weak = fabsf(kz_gauge_weak - kz_metric_weak);
    printf("DUALITY CHECK:\n");
    printf("  |κζ_gauge - κζ_metric| = %.6f %s\n\n",
           duality_diff_weak,
           (duality_diff_weak < 0.1f) ? "✓ GOOD" : "✗ POOR");

    // ═══════════════════════════════════════════════════════════
    // SCENARIO 2: Strong Coupling (α = 0.1)
    // ═══════════════════════════════════════════════════════════
    printf("═══════════════════════════════════════════════════════════\n");
    printf("SCENARIO 2: STRONG COUPLING (α = 0.1)\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    float alpha_strong = 0.1f;

    simulate_stress_energy_eigenvalues(gauge_eigenvalues, &gauge_count, LATTICE_SIZE);
    printf("Computing κζ from GAUGE FIELDS (stress-energy eigenvalues):\n");
    printf("  Number of eigenvalues: %d\n", gauge_count);
    float kz_gauge_strong = compute_kappa_zeta(gauge_eigenvalues, gauge_count);
    printf("  → κζ_gauge = %.6f\n\n", kz_gauge_strong);

    simulate_metric_eigenvalues(metric_eigenvalues, &metric_count, LATTICE_SIZE, alpha_strong);
    printf("Computing κζ from METRIC (geometry eigenvalues):\n");
    printf("  Number of eigenvalues: %d\n", metric_count);
    float kz_metric_strong = compute_kappa_zeta(metric_eigenvalues, metric_count);
    printf("  → κζ_metric = %.6f\n\n", kz_metric_strong);

    float duality_diff_strong = fabsf(kz_gauge_strong - kz_metric_strong);
    printf("DUALITY CHECK:\n");
    printf("  |κζ_gauge - κζ_metric| = %.6f %s\n\n",
           duality_diff_strong,
           (duality_diff_strong < 0.1f) ? "✓ GOOD" : "✗ POOR");

    // ═══════════════════════════════════════════════════════════
    // SCENARIO 3: Critical Coupling (α = 0.05, Bootstrap Fixed Point)
    // ═══════════════════════════════════════════════════════════
    printf("═══════════════════════════════════════════════════════════\n");
    printf("SCENARIO 3: CRITICAL COUPLING (α = 0.05, Fixed Point)\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    float alpha_critical = 0.05f;

    // At fixed point, gauge and geometry should be self-consistent
    // This means eigenvalue distributions should be more similar
    simulate_stress_energy_eigenvalues(gauge_eigenvalues, &gauge_count, LATTICE_SIZE);
    printf("Computing κζ from GAUGE FIELDS (stress-energy eigenvalues):\n");
    printf("  Number of eigenvalues: %d\n", gauge_count);
    float kz_gauge_critical = compute_kappa_zeta(gauge_eigenvalues, gauge_count);
    printf("  → κζ_gauge = %.6f\n\n", kz_gauge_critical);

    // At fixed point, metric eigenvalues should reflect gauge structure more strongly
    simulate_metric_eigenvalues(metric_eigenvalues, &metric_count, LATTICE_SIZE, alpha_critical);
    printf("Computing κζ from METRIC (geometry eigenvalues):\n");
    printf("  Number of eigenvalues: %d\n", metric_count);
    float kz_metric_critical = compute_kappa_zeta(metric_eigenvalues, metric_count);
    printf("  → κζ_metric = %.6f\n\n", kz_metric_critical);

    float duality_diff_critical = fabsf(kz_gauge_critical - kz_metric_critical);
    printf("DUALITY CHECK:\n");
    printf("  |κζ_gauge - κζ_metric| = %.6f %s\n\n",
           duality_diff_critical,
           (duality_diff_critical < 0.1f) ? "✓ GOOD" : "✗ POOR");

    // ═══════════════════════════════════════════════════════════
    // SUMMARY
    // ═══════════════════════════════════════════════════════════
    printf("═══════════════════════════════════════════════════════════\n");
    printf("SUMMARY: κζ ACROSS COUPLING STRENGTHS\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("Coupling α    κζ_gauge   κζ_metric   Δκζ        Status\n");
    printf("───────────────────────────────────────────────────────────\n");
    printf("0.01 (weak)   %.4f     %.4f      %.6f   %s\n",
           kz_gauge_weak, kz_metric_weak, duality_diff_weak,
           (duality_diff_weak < 0.1f) ? "✓" : "✗");
    printf("0.05 (crit)   %.4f     %.4f      %.6f   %s\n",
           kz_gauge_critical, kz_metric_critical, duality_diff_critical,
           (duality_diff_critical < 0.1f) ? "✓" : "✗");
    printf("0.10 (strong) %.4f     %.4f      %.6f   %s\n",
           kz_gauge_strong, kz_metric_strong, duality_diff_strong,
           (duality_diff_strong < 0.1f) ? "✓" : "✗");
    printf("───────────────────────────────────────────────────────────\n\n");

    printf("INTERPRETATION:\n");
    printf("• κζ measures the ratio of Mellin moments from eigenvalue spectra\n");
    printf("• At bootstrap fixed point, κζ_gauge ≈ κζ_metric (duality)\n");
    printf("• Typical values: κζ ∈ [0.01, 100] depending on field strength\n");
    printf("• Small Δκζ indicates gauge-geometry self-consistency\n\n");

    free(gauge_eigenvalues);
    free(metric_eigenvalues);

    return 0;
}
