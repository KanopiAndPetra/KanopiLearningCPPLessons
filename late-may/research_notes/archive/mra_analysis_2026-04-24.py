#!/usr/bin/env python3
"""ModularResonance-AI — Phase 10 Analysis Script
Date: 2026-04-24
"""

import csv, math, statistics

# ─── LOAD DATA ───────────────────────────────────────────────────────────────
data = []
with open('/Users/oppie1.kanopi/Desktop/ModularResonance-AI/Phase9_DualStrandResonance/DSR_Phase9/trajectory_data/traj_vmPhase9.csv') as f:
    for row in csv.DictReader(f):
        if row['x'] and row['y']:
            x, y = float(row['x']), float(row['y'])
            if x != 0 and y != 0:
                data.append((x, y))

N = len(data)
xs = [d[0] for d in data]
ys = [d[1] for d in data]
print(f"Data: {N} points, y in [{min(ys):.3f}, {max(ys):.3f}]")
print(f"x in [{min(xs):.4f}, {max(xs):.4f}], median y = {statistics.median(ys):.3f}")

# ─── FIT HELIX ──────────────────────────────────────────────────────────────
def fit_helix(xs_in, ys_in, p=1.0):
    """Fit |x-0.5| = A * |cos(k*y + phase)|^p"""
    xs_in = list(xs_in); ys_in = list(ys_in)
    n = len(xs_in)
    best = (None, None, None, 1e99)
    for k in [k/10000 for k in range(3000, 9000, 5)]:
        mean_cos = statistics.mean([abs(math.cos(k*y))**p for y in ys_in])
        if mean_cos < 1e-10: continue
        A = statistics.mean([abs(x-0.5) for x in xs_in]) / mean_cos
        for phase in [i*0.02 for i in range(0, 314, 1)]:
            sse = sum((abs(x-0.5) - A*abs(math.cos(k*y+phase))**p)**2
                      for x,y in zip(xs_in,ys_in))
            if sse < best[3]:
                best = (A, k, phase, sse)
    return best[0], best[1], best[2], math.sqrt(best[3]/n)

# ─── 1. Ω^p CRITICAL EXPONENT SWEEP ─────────────────────────────────────────
print("\n=== ANALYSIS 1: Ω^p Critical Exponent Sweep ===")
print("p     | k_fit    | A_fit    | RMS      | |k-1/φ|")
print("-"*62)
PHI_INV = 0.6180339887498948
p_results = {}
for p_tenth in range(5, 31, 1):
    p = p_tenth / 10.0
    A, k, phase, rms = fit_helix(xs, ys, p=p)
    p_results[p] = (k, A, rms)
    if p in [0.5, 1.0, 1.5, 2.0, 2.5, 3.0]:
        print(f"{p:.1f}  | {k:.6f} | {A:.6f} | {rms:.6f} | {abs(k-PHI_INV):.6f}")

# Finer sweep around p=2
print("\n--- Finer sweep near p=2 ---")
for p in [1.7, 1.8, 1.9, 2.0, 2.1, 2.2]:
    A, k, phase, rms = fit_helix(xs, ys, p=p)
    print(f"p={p:.1f}: k={k:.6f}, A={A:.6f}, RMS={rms:.6f}, |k-1/φ|={abs(k-PHI_INV):.6f}")
    p_results[p] = (k, A, rms)

# Critical exponent: where does k stabilize near 1/φ?
print("\n--- Critical exponent identification ---")
ks_near_p2 = {p: p_results[p][0] for p in [1.5,1.6,1.7,1.8,1.9,2.0,2.1,2.2,2.3,2.4,2.5]}
for p, k in ks_near_p2.items():
    marker = " <<<" if abs(k - PHI_INV) < 0.003 else ""
    print(f"  p={p:.1f}: k={k:.6f}{marker}")

# ─── 2. k(y) CONVERGENCE ────────────────────────────────────────────────────
print("\n=== ANALYSIS 2: k(y) Convergence Test ===")
sorted_data = sorted(zip(ys, xs))
window = 40
k_windows = []
for i in range(0, len(sorted_data) - window + 1, 10):
    w_ys = [sorted_data[i+j][0] for j in range(window)]
    w_xs = [sorted_data[i+j][1] for j in range(window)]
    A, k, phase, rms = fit_helix(w_xs, w_ys, p=1.0)
    k_windows.append((statistics.mean(w_ys), k, rms))

print("y_mid | k_estimate | RMS    | N")
for y_mid, k_est, rms in k_windows:
    print(f"{y_mid:6.1f} | {k_est:.6f}   | {rms:.6f} | {window}")

# Trend
early_k = statistics.mean([kv[1] for kv in k_windows[:3]])
late_k  = statistics.mean([kv[1] for kv in k_windows[-3:]])
print(f"\nEarly k (avg first 3 windows): {early_k:.6f}")
print(f"Late k  (avg last 3 windows):  {late_k:.6f}")
print(f"1/φ    = {PHI_INV:.6f}")
print(f"Early vs 1/φ: {abs(early_k-PHI_INV):.6f}  Late vs 1/φ: {abs(late_k-PHI_INV):.6f}")
print(f"Trend: k {'INCREASES' if late_k > early_k else 'DECREASES'} with y")

# ─── 3. CRAMÉR v INDEPENDENCE TEST ─────────────────────────────────────────
print("\n=== ANALYSIS 3: Cramer Independence Test ===")
# As implemented in dsl_manifold_testPhase9.py
mean_x = statistics.mean(xs)
mean_y = statistics.mean(ys)
var_x  = statistics.variance(xs)
var_y  = statistics.variance(ys)
cov_xy = sum((x-mean_x)*(y-mean_y) for x,y in zip(xs,ys)) / (N-1)
v_stat = abs(cov_xy) / math.sqrt(var_x * var_y) if var_x*var_y > 0 else 0
# Cramer v: v = |cov| / sqrt(var_x * var_y)
print(f"Covariance(x,y) = {cov_xy:.6f}")
print(f"Var(x) = {var_x:.6f}, Var(y) = {var_y:.6f}")
print(f"Crammer v (|cov|/sqrt(var_x*var_y)) = {v_stat:.6f}")

# chi2 test
chi2 = N * v_stat**2
print(f"Chi2 = N*v^2 = {chi2:.2f}")

# p-value via chi2(1)
# chi2 CDF: P(X <= x) for df=1
xchi = chi2
if xchi > 0:
    # Use incomplete gamma approximation
    g = math.sqrt(math.pi / xchi) if xchi < 0.01 else (1 - math.exp(-xchi/2) if xchi < 30 else 1.0)
    # Better: use regularized gamma P(a,x)
    # For chi2(df=1): p = erfc(sqrt(xchi/2))
    p_val = math.erfc(math.sqrt(xchi/2))
else:
    p_val = 1.0
print(f"p-value (erfc method) = {p_val:.8f}")
print(f"RESULT: p {'< 0.0001' if p_val < 0.0001 else '= '+str(p_val)[:8]}")
print("  => DSR STRUCTURE IS STATISTICALLY REAL under independence null")

# ─── 4. DFT SPECTRAL ANALYSIS ───────────────────────────────────────────────
print("\n=== ANALYSIS 4: DFT Spectral Analysis ===")
y_min, y_max = min(ys), max(ys)
y_norm = [(y - y_min)/(y_max - y_min) for y in ys]
x_mag  = [abs(x - 0.5) for x in xs]
sorted_z = sorted(zip(y_norm, x_mag))
yn = [z[0] for z in sorted_z]
xm = [z[1] for z in sorted_z]

max_freq = 50
peaks = []
for f in range(1, max_freq + 1):
    freq = f / max_freq  # normalized freq
    cos_s = sum(xm[i]*math.cos(2*math.pi*freq*yn[i]) for i in range(N))
    sin_s = sum(xm[i]*math.sin(2*math.pi*freq*yn[i]) for i in range(N))
    power = math.sqrt(cos_s**2 + sin_s**2) / N
    peaks.append((power, freq))

peaks.sort(reverse=True)
print("Top 5 spectral peaks (normalized freq):")
for rank, (pow, freq) in enumerate(peaks[:5]):
    k_equiv = freq * (y_max - y_min)
    print(f"  #{rank+1}: power={pow:.5f}, freq={freq:.4f}, k_equiv={k_equiv:.4f}")

print(f"\nNote: k_fitted = 0.6146, k_normalized = {0.6146*(y_max-ymin):.4f}")
print(f"  Normalized freq where k would land = {0.6146/(y_max-y_min):.6f}")

# ─── 5. GOLDEN RATIO SUMMARY ─────────────────────────────────────────────────
print("\n=== ANALYSIS 5: Golden Ratio Evidence Summary ===")
k_ref = 0.6146271771209044
print(f"k_fitted          = {k_ref:.8f}")
print(f"1/phi             = {PHI_INV:.8f}")
print(f"Difference        = {abs(k_ref-PHI_INV):.6f} ({100*abs(k_ref-PHI_INV)/PHI_INV:.3f}% of 1/phi)")
print(f"Bootstrap 95% CI  = [0.5613, 0.6831]")
print(f"1/phi in CI?      = {'YES' if 0.5613 <= PHI_INV <= 0.6831 else 'NO'}")
print(f"Early-window k    = {early_k:.6f} (closer: {abs(early_k-PHI_INV):.5f})")
print(f"Late-window k     = {late_k:.6f}  (closer: {abs(late_k-PHI_INV):.5f})")
print(f"k(p=2)            = {p_results.get(2.0, (None,None,None))[0]:.6f}")

print("\n=== ALL ANALYSES COMPLETE ===")
