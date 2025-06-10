#!/usr/bin/env python3
"""
Timer Consistency and Performance Analysis
Analyzes CSV data from freq_timer tests and generates visualizations.
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
from pathlib import Path
import argparse

# Set style for better plots
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")

def analyze_consistency_data(df_consistency):
    """Analyze timer consistency between freq_timer and system clock."""
    print("=== Consistency Analysis ===")
    
    # Calculate statistics
    time_diffs = df_consistency['time_diff_ns']
    freq_times = df_consistency['freq_timer_ns']
    std_times = df_consistency['std_time_ns']
    
    print(f"Samples analyzed: {len(df_consistency)}")
    print(f"Mean time difference: {time_diffs.mean():.2f} ± {time_diffs.std():.2f} ns")
    print(f"Median time difference: {time_diffs.median():.2f} ns")
    print(f"Max absolute difference: {abs(time_diffs).max():.2f} ns")
    
    # Calculate relative error
    relative_errors = abs(time_diffs) / std_times * 100
    print(f"Mean relative error: {relative_errors.mean():.4f}%")
    print(f"Max relative error: {relative_errors.max():.4f}%")
    
    # Create consistency plots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('Timer Consistency Analysis', fontsize=16, fontweight='bold')
    
    # Plot 1: Scatter plot of freq_timer vs std_time
    axes[0, 0].scatter(std_times / 1000, freq_times / 1000, alpha=0.6, s=1)
    axes[0, 0].plot([std_times.min()/1000, std_times.max()/1000], 
                    [std_times.min()/1000, std_times.max()/1000], 'r--', alpha=0.8)
    axes[0, 0].set_xlabel('System Time (μs)')
    axes[0, 0].set_ylabel('freq_timer Time (μs)')
    axes[0, 0].set_title('freq_timer vs System Time')
    axes[0, 0].grid(True, alpha=0.3)
    
    # Plot 2: Time difference histogram
    axes[0, 1].hist(time_diffs, bins=50, alpha=0.7, edgecolor='black')
    axes[0, 1].axvline(time_diffs.mean(), color='red', linestyle='--', 
                       label=f'Mean: {time_diffs.mean():.1f}ns')
    axes[0, 1].axvline(time_diffs.median(), color='orange', linestyle='--', 
                       label=f'Median: {time_diffs.median():.1f}ns')
    axes[0, 1].set_xlabel('Time Difference (ns)')
    axes[0, 1].set_ylabel('Frequency')
    axes[0, 1].set_title('Distribution of Time Differences')
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3)
    
    # Plot 3: Time difference over sample number
    sample_subset = df_consistency.iloc[::10]  # Every 10th sample for clarity
    axes[1, 0].plot(sample_subset['sample_id'], sample_subset['time_diff_ns'], 
                    alpha=0.7, linewidth=0.5)
    axes[1, 0].axhline(0, color='red', linestyle='--', alpha=0.8)
    axes[1, 0].set_xlabel('Sample Number')
    axes[1, 0].set_ylabel('Time Difference (ns)')
    axes[1, 0].set_title('Time Difference Over Time')
    axes[1, 0].grid(True, alpha=0.3)
    
    # Plot 4: Relative error distribution
    axes[1, 1].hist(relative_errors, bins=50, alpha=0.7, edgecolor='black')
    axes[1, 1].axvline(relative_errors.mean(), color='red', linestyle='--', 
                       label=f'Mean: {relative_errors.mean():.3f}%')
    axes[1, 1].set_xlabel('Relative Error (%)')
    axes[1, 1].set_ylabel('Frequency')
    axes[1, 1].set_title('Distribution of Relative Errors')
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('timer_consistency_analysis.png', dpi=300, bbox_inches='tight')
    print("Consistency analysis saved to 'timer_consistency_analysis.png'")
    return fig

def analyze_overhead_data(df_overhead):
    """Analyze timing overhead for different methods."""
    print("\n=== Overhead Analysis ===")
    
    # Calculate statistics by method
    methods = df_overhead['method'].unique()
    overhead_stats = {}
    
    for method in methods:
        method_data = df_overhead[df_overhead['method'] == method]['duration_ns']
        # Remove extreme outliers (> 99.9th percentile) for cleaner analysis
        p999 = method_data.quantile(0.999)
        clean_data = method_data[method_data <= p999]
        
        overhead_stats[method] = {
            'mean': clean_data.mean(),
            'median': clean_data.median(),
            'std': clean_data.std(),
            'min': clean_data.min(),
            'p95': clean_data.quantile(0.95),
            'p99': clean_data.quantile(0.99)
        }
        
        print(f"\n{method}:")
        print(f"  Mean: {overhead_stats[method]['mean']:.1f} ± {overhead_stats[method]['std']:.1f} ns")
        print(f"  Median: {overhead_stats[method]['median']:.1f} ns")
        print(f"  Min: {overhead_stats[method]['min']:.1f} ns")
        print(f"  95th percentile: {overhead_stats[method]['p95']:.1f} ns")
        print(f"  99th percentile: {overhead_stats[method]['p99']:.1f} ns")
    
    # Create overhead plots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('Timer Overhead Analysis', fontsize=16, fontweight='bold')
    
    # Plot 1: Box plot comparison
    clean_overhead_data = []
    method_labels = []
    for method in methods:
        method_data = df_overhead[df_overhead['method'] == method]['duration_ns']
        p999 = method_data.quantile(0.999)
        clean_data = method_data[method_data <= p999]
        clean_overhead_data.append(clean_data)
        method_labels.append(method.replace('_', '_\n'))  # Line break for readability
    
    bp = axes[0, 0].boxplot(clean_overhead_data, labels=method_labels, patch_artist=True)
    for patch, color in zip(bp['boxes'], sns.color_palette("husl", len(methods))):
        patch.set_facecolor(color)
        patch.set_alpha(0.7)
    axes[0, 0].set_ylabel('Duration (ns)')
    axes[0, 0].set_title('Overhead Distribution by Method')
    axes[0, 0].grid(True, alpha=0.3)
    
    # Plot 2: Histogram comparison
    for i, method in enumerate(methods):
        method_data = df_overhead[df_overhead['method'] == method]['duration_ns']
        p999 = method_data.quantile(0.999)
        clean_data = method_data[method_data <= p999]
        axes[0, 1].hist(clean_data, bins=50, alpha=0.6, label=method, density=True)
    
    axes[0, 1].set_xlabel('Duration (ns)')
    axes[0, 1].set_ylabel('Density')
    axes[0, 1].set_title('Overhead Distribution Comparison')
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3)
    
    # Plot 3: Time series for freq_timer_now_ns
    freq_timer_data = df_overhead[df_overhead['method'] == 'freq_timer_now_ns']
    subset = freq_timer_data.iloc[::100]  # Every 100th sample
    axes[1, 0].plot(subset['call_number'], subset['duration_ns'], alpha=0.7, linewidth=0.5)
    axes[1, 0].axhline(overhead_stats['freq_timer_now_ns']['mean'], 
                       color='red', linestyle='--', alpha=0.8, 
                       label=f"Mean: {overhead_stats['freq_timer_now_ns']['mean']:.1f}ns")
    axes[1, 0].set_xlabel('Call Number')
    axes[1, 0].set_ylabel('Duration (ns)')
    axes[1, 0].set_title('freq_timer_now_ns Overhead Over Time')
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    
    # Plot 4: Performance comparison bar chart
    means = [overhead_stats[method]['mean'] for method in methods]
    stds = [overhead_stats[method]['std'] for method in methods]
    
    bars = axes[1, 1].bar(range(len(methods)), means, yerr=stds, 
                          alpha=0.7, capsize=5, 
                          color=sns.color_palette("husl", len(methods)))
    axes[1, 1].set_xticks(range(len(methods)))
    axes[1, 1].set_xticklabels([m.replace('_', '_\n') for m in methods])
    axes[1, 1].set_ylabel('Mean Duration (ns)')
    axes[1, 1].set_title('Mean Overhead Comparison')
    axes[1, 1].grid(True, alpha=0.3)
    
    # Add value labels on bars
    for bar, mean in zip(bars, means):
        height = bar.get_height()
        axes[1, 1].text(bar.get_x() + bar.get_width()/2., height + 1,
                        f'{mean:.1f}ns', ha='center', va='bottom')
    
    plt.tight_layout()
    plt.savefig('timer_overhead_analysis.png', dpi=300, bbox_inches='tight')
    print("Overhead analysis saved to 'timer_overhead_analysis.png'")
    return fig

def analyze_monotonicity_data(df_monotonicity):
    """Analyze monotonicity and timing deltas."""
    print("\n=== Monotonicity Analysis ===")
    
    # Calculate statistics
    violations = df_monotonicity['violation'].sum()
    total_samples = len(df_monotonicity)
    delta_ns = df_monotonicity['delta_ns']
    delta_cycles = df_monotonicity['delta_cycles']
    
    print(f"Total samples: {total_samples}")
    print(f"Monotonicity violations: {violations} ({violations/total_samples*100:.6f}%)")
    print(f"Mean time delta: {delta_ns.mean():.2f} ± {delta_ns.std():.2f} ns")
    print(f"Mean cycle delta: {delta_cycles.mean():.2f} ± {delta_cycles.std():.2f} cycles")
    
    # Create monotonicity plots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('Timer Monotonicity Analysis', fontsize=16, fontweight='bold')
    
    # Plot 1: Delta distribution
    positive_deltas = delta_ns[delta_ns > 0]
    axes[0, 0].hist(positive_deltas, bins=50, alpha=0.7, edgecolor='black')
    axes[0, 0].axvline(positive_deltas.mean(), color='red', linestyle='--', 
                       label=f'Mean: {positive_deltas.mean():.1f}ns')
    axes[0, 0].axvline(positive_deltas.median(), color='orange', linestyle='--', 
                       label=f'Median: {positive_deltas.median():.1f}ns')
    axes[0, 0].set_xlabel('Time Delta (ns)')
    axes[0, 0].set_ylabel('Frequency')
    axes[0, 0].set_title('Distribution of Positive Time Deltas')
    axes[0, 0].legend()
    axes[0, 0].grid(True, alpha=0.3)
    
    # Plot 2: Cycle deltas
    positive_cycle_deltas = delta_cycles[delta_cycles > 0]
    axes[0, 1].hist(positive_cycle_deltas, bins=50, alpha=0.7, edgecolor='black')
    axes[0, 1].axvline(positive_cycle_deltas.mean(), color='red', linestyle='--', 
                       label=f'Mean: {positive_cycle_deltas.mean():.1f}')
    axes[0, 1].set_xlabel('Cycle Delta')
    axes[0, 1].set_ylabel('Frequency')
    axes[0, 1].set_title('Distribution of Positive Cycle Deltas')
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3)
    
    # Plot 3: Time deltas over time (subset for clarity)
    subset = df_monotonicity.iloc[::100]  # Every 100th sample
    axes[1, 0].plot(subset['call_number'], subset['delta_ns'], alpha=0.7, linewidth=0.5)
    axes[1, 0].axhline(0, color='red', linestyle='--', alpha=0.8)
    axes[1, 0].set_xlabel('Call Number')
    axes[1, 0].set_ylabel('Time Delta (ns)')
    axes[1, 0].set_title('Time Deltas Over Time')
    axes[1, 0].grid(True, alpha=0.3)
    
    # Plot 4: Cumulative timestamp plot (subset)
    subset = df_monotonicity.iloc[::1000]  # Every 1000th sample
    relative_time = (subset['timestamp_ns'] - subset['timestamp_ns'].iloc[0]) / 1e9  # Convert to seconds
    axes[1, 1].plot(subset['call_number'], relative_time, alpha=0.8, linewidth=1)
    axes[1, 1].set_xlabel('Call Number')
    axes[1, 1].set_ylabel('Relative Time (seconds)')
    axes[1, 1].set_title('Cumulative Timestamp Progress')
    axes[1, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('timer_monotonicity_analysis.png', dpi=300, bbox_inches='tight')
    print("Monotonicity analysis saved to 'timer_monotonicity_analysis.png'")
    return fig

def analyze_resolution_data(df_resolution):
    """Analyze timer resolution characteristics."""
    print("\n=== Resolution Analysis ===")
    
    # Calculate statistics
    time_diffs = df_resolution['time_diff_ns']
    cycles_diffs = df_resolution['cycles_diff']
    zero_diffs = df_resolution['zero_diff'].sum()
    total_samples = len(df_resolution)
    
    non_zero_time_diffs = time_diffs[time_diffs > 0]
    non_zero_cycle_diffs = cycles_diffs[cycles_diffs > 0]
    
    print(f"Total measurements: {total_samples}")
    print(f"Zero time differences: {zero_diffs} ({zero_diffs/total_samples*100:.2f}%)")
    print(f"Minimum non-zero time difference: {non_zero_time_diffs.min():.0f} ns")
    print(f"Mean non-zero time difference: {non_zero_time_diffs.mean():.1f} ± {non_zero_time_diffs.std():.1f} ns")
    print(f"Minimum non-zero cycle difference: {non_zero_cycle_diffs.min():.0f} cycles")
    print(f"Mean non-zero cycle difference: {non_zero_cycle_diffs.mean():.1f} cycles")
    
    # Create resolution plots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('Timer Resolution Analysis', fontsize=16, fontweight='bold')
    
    # Plot 1: Time difference distribution
    axes[0, 0].hist(non_zero_time_diffs, bins=50, alpha=0.7, edgecolor='black')
    axes[0, 0].axvline(non_zero_time_diffs.mean(), color='red', linestyle='--', 
                       label=f'Mean: {non_zero_time_diffs.mean():.1f}ns')
    axes[0, 0].axvline(non_zero_time_diffs.min(), color='green', linestyle='--', 
                       label=f'Min: {non_zero_time_diffs.min():.0f}ns')
    axes[0, 0].set_xlabel('Time Difference (ns)')
    axes[0, 0].set_ylabel('Frequency')
    axes[0, 0].set_title('Distribution of Non-Zero Time Differences')
    axes[0, 0].legend()
    axes[0, 0].grid(True, alpha=0.3)
    
    # Plot 2: Cycle difference distribution
    axes[0, 1].hist(non_zero_cycle_diffs, bins=50, alpha=0.7, edgecolor='black')
    axes[0, 1].axvline(non_zero_cycle_diffs.mean(), color='red', linestyle='--', 
                       label=f'Mean: {non_zero_cycle_diffs.mean():.1f}')
    axes[0, 1].axvline(non_zero_cycle_diffs.min(), color='green', linestyle='--', 
                       label=f'Min: {non_zero_cycle_diffs.min():.0f}')
    axes[0, 1].set_xlabel('Cycle Difference')
    axes[0, 1].set_ylabel('Frequency')
    axes[0, 1].set_title('Distribution of Non-Zero Cycle Differences')
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3)
    
    # Plot 3: Zero vs non-zero differences
    zero_nonzero_counts = [zero_diffs, total_samples - zero_diffs]
    labels = ['Zero Differences', 'Non-Zero Differences']
    colors = ['lightcoral', 'lightblue']
    
    wedges, texts, autotexts = axes[1, 0].pie(zero_nonzero_counts, labels=labels, 
                                              colors=colors, autopct='%1.1f%%', 
                                              startangle=90)
    axes[1, 0].set_title('Zero vs Non-Zero Time Differences')
    
    # Plot 4: Time vs Cycle differences scatter
    sample_subset = df_resolution.iloc[::10]  # Every 10th sample for clarity
    non_zero_subset = sample_subset[(sample_subset['time_diff_ns'] > 0) & 
                                   (sample_subset['cycles_diff'] > 0)]
    
    axes[1, 1].scatter(non_zero_subset['cycles_diff'], non_zero_subset['time_diff_ns'], 
                       alpha=0.6, s=10)
    axes[1, 1].set_xlabel('Cycle Difference')
    axes[1, 1].set_ylabel('Time Difference (ns)')
    axes[1, 1].set_title('Time vs Cycle Differences')
    axes[1, 1].grid(True, alpha=0.3)
    
    # Add trend line
    if len(non_zero_subset) > 10:
        z = np.polyfit(non_zero_subset['cycles_diff'], non_zero_subset['time_diff_ns'], 1)
        p = np.poly1d(z)
        x_trend = np.linspace(non_zero_subset['cycles_diff'].min(), 
                             non_zero_subset['cycles_diff'].max(), 100)
        axes[1, 1].plot(x_trend, p(x_trend), "r--", alpha=0.8, 
                        label=f'Trend: {z[0]:.2f}ns/cycle')
        axes[1, 1].legend()
    
    plt.tight_layout()
    plt.savefig('timer_resolution_analysis.png', dpi=300, bbox_inches='tight')
    print("Resolution analysis saved to 'timer_resolution_analysis.png'")
    return fig

def generate_summary_report(df_consistency, df_overhead, df_monotonicity, df_resolution):
    """Generate a comprehensive summary report."""
    print("\n=== Summary Report ===")
    
    # Calculate key metrics
    consistency_error = abs(df_consistency['time_diff_ns']).mean()
    monotonicity_violations = df_monotonicity['violation'].sum()
    
    # Get overhead stats for each method
    overhead_means = {}
    for method in df_overhead['method'].unique():
        method_data = df_overhead[df_overhead['method'] == method]['duration_ns']
        p999 = method_data.quantile(0.999)
        clean_data = method_data[method_data <= p999]
        overhead_means[method] = clean_data.mean()
    
    resolution_min = df_resolution[df_resolution['time_diff_ns'] > 0]['time_diff_ns'].min()
    zero_diff_pct = df_resolution['zero_diff'].sum() / len(df_resolution) * 100
    
    # Create summary figure
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('freq_timer Performance Summary', fontsize=16, fontweight='bold')
    
    # Plot 1: Key metrics summary
    metrics = ['Consistency\nError (ns)', 'Monotonicity\nViolations', 
               'Resolution\n(ns)', 'Zero Diffs\n(%)']
    values = [consistency_error, monotonicity_violations, resolution_min, zero_diff_pct]
    colors = ['lightblue', 'lightgreen', 'lightyellow', 'lightcoral']
    
    bars = axes[0, 0].bar(metrics, values, color=colors, alpha=0.7, edgecolor='black')
    axes[0, 0].set_title('Key Performance Metrics')
    axes[0, 0].set_ylabel('Value')
    
    # Add value labels on bars
    for bar, value in zip(bars, values):
        height = bar.get_height()
        axes[0, 0].text(bar.get_x() + bar.get_width()/2., height + max(values)*0.01,
                        f'{value:.1f}', ha='center', va='bottom', fontweight='bold')
    
    # Plot 2: Overhead comparison
    methods = list(overhead_means.keys())
    means = list(overhead_means.values())
    bars = axes[0, 1].bar(range(len(methods)), means, 
                          color=sns.color_palette("husl", len(methods)), 
                          alpha=0.7, edgecolor='black')
    axes[0, 1].set_xticks(range(len(methods)))
    axes[0, 1].set_xticklabels([m.replace('_', '_\n') for m in methods])
    axes[0, 1].set_ylabel('Mean Overhead (ns)')
    axes[0, 1].set_title('Timing Method Overhead Comparison')
    
    for bar, mean in zip(bars, means):
        height = bar.get_height()
        axes[0, 1].text(bar.get_x() + bar.get_width()/2., height + 1,
                        f'{mean:.1f}ns', ha='center', va='bottom', fontweight='bold')
    
    # Plot 3: Consistency over time
    subset = df_consistency.iloc[::50]  # Every 50th sample
    axes[1, 0].plot(subset['sample_id'], abs(subset['time_diff_ns']), 
                    alpha=0.7, linewidth=1, color='blue')
    axes[1, 0].axhline(consistency_error, color='red', linestyle='--', 
                       label=f'Mean: {consistency_error:.1f}ns')
    axes[1, 0].set_xlabel('Sample Number')
    axes[1, 0].set_ylabel('Absolute Error (ns)')
    axes[1, 0].set_title('Consistency Error Over Time')
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    
    # Plot 4: Resolution distribution
    non_zero_diffs = df_resolution[df_resolution['time_diff_ns'] > 0]['time_diff_ns']
    axes[1, 1].hist(non_zero_diffs, bins=30, alpha=0.7, edgecolor='black', color='green')
    axes[1, 1].axvline(non_zero_diffs.mean(), color='red', linestyle='--', 
                       label=f'Mean: {non_zero_diffs.mean():.1f}ns')
    axes[1, 1].axvline(resolution_min, color='orange', linestyle='--', 
                       label=f'Min: {resolution_min:.0f}ns')
    axes[1, 1].set_xlabel('Time Difference (ns)')
    axes[1, 1].set_ylabel('Frequency')
    axes[1, 1].set_title('Timer Resolution Distribution')
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('timer_summary_report.png', dpi=300, bbox_inches='tight')
    print("Summary report saved to 'timer_summary_report.png'")
    
    # Print summary statistics
    print(f"\nPerformance Summary:")
    print(f"  Consistency Error: {consistency_error:.2f} ns")
    print(f"  Monotonicity Violations: {monotonicity_violations}")
    print(f"  Timer Resolution: {resolution_min:.0f} ns")
    print(f"  Zero Differences: {zero_diff_pct:.1f}%")
    print(f"\nTiming Overhead:")
    for method, overhead in overhead_means.items():
        print(f"  {method}: {overhead:.1f} ns")
    
    return fig

def main():
    parser = argparse.ArgumentParser(description='Analyze freq_timer CSV data and generate plots')
    parser.add_argument('--generate-data', action='store_true', 
                       help='Generate CSV data files first')
    args = parser.parse_args()
    
    if args.generate_data:
        print("Generating CSV data files...")
        import subprocess
        import os
        
        # Build and run the CSV test
        build_cmd = "gcc tests/csv_output_test.c -Iinclude -Ltarget/release -lfreq_timer -lpthread -lm -o csv_test"
        result = subprocess.run(build_cmd.split(), capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Build failed: {result.stderr}")
            return 1
        
        # Run the test
        env = os.environ.copy()
        env['LD_LIBRARY_PATH'] = 'target/release'
        result = subprocess.run('./csv_test', capture_output=True, text=True, env=env)
        print(result.stdout)
        if result.returncode != 0:
            print(f"CSV generation failed: {result.stderr}")
            return 1
        
        # Clean up
        os.remove('csv_test')
    
    # Check if CSV files exist
    required_files = ['consistency_data.csv', 'overhead_data.csv', 
                     'monotonicity_data.csv', 'resolution_data.csv']
    
    for file in required_files:
        if not Path(file).exists():
            print(f"Error: {file} not found. Run with --generate-data first.")
            return 1
    
    print("Loading CSV data files...")
    
    # Load data
    df_consistency = pd.read_csv('consistency_data.csv')
    df_overhead = pd.read_csv('overhead_data.csv')
    df_monotonicity = pd.read_csv('monotonicity_data.csv')
    df_resolution = pd.read_csv('resolution_data.csv')
    
    print(f"Loaded {len(df_consistency)} consistency samples")
    print(f"Loaded {len(df_overhead)} overhead samples")
    print(f"Loaded {len(df_monotonicity)} monotonicity samples")
    print(f"Loaded {len(df_resolution)} resolution samples")
    
    # Generate analyses
    analyze_consistency_data(df_consistency)
    analyze_overhead_data(df_overhead)
    analyze_monotonicity_data(df_monotonicity)
    analyze_resolution_data(df_resolution)
    generate_summary_report(df_consistency, df_overhead, df_monotonicity, df_resolution)
    
    print(f"\nAnalysis complete! Generated plots:")
    print(f"  - timer_consistency_analysis.png")
    print(f"  - timer_overhead_analysis.png")
    print(f"  - timer_monotonicity_analysis.png")
    print(f"  - timer_resolution_analysis.png")
    print(f"  - timer_summary_report.png")
    
    return 0

if __name__ == "__main__":
    exit(main())