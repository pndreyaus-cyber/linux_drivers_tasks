import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

BIN_NUM = 10

data = pd.read_csv('output.csv', header = None)
values = data[0].dropna()

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(15, 5))

counts1, bins1, patches1 = ax1.hist(values, bins=BIN_NUM, alpha=0.7, color='skyblue', edgecolor='black')
ax1.set_xlabel('Value')
ax1.set_ylabel('Frequency')
ax1.set_title('Linear Scale\n(Your "steep" plot)')
ax1.grid(True, alpha=0.3)

# Add count labels
for count, patch in zip(counts1, patches1):
    if count > 0:
        x = patch.get_x() + patch.get_width()/2
        y = patch.get_height()
        ax1.text(x, y, f'{int(count)}', ha='center', va='bottom', fontsize=9)

# 2. Logarithmic Y-axis (COMPRESSES the steepness!)
counts2, bins2, patches2 = ax2.hist(values, bins=BIN_NUM, alpha=0.7, color='lightcoral', edgecolor='black')
ax2.set_yscale('log')  # <-- THIS IS THE KEY!
ax2.set_xlabel('Value')
ax2.set_ylabel('Frequency (log scale)')
ax2.set_title('Log Y-axis\n(Compresses steep differences)')
ax2.grid(True, alpha=0.3)

# Add count labels (log scale needs careful placement)
for count, patch in zip(counts2, patches2):
    if count > 0:
        x = patch.get_x() + patch.get_width()/2
        y = count * 1.2  # Slightly above
        ax2.text(x, y, f'{int(count)}', ha='center', va='bottom', fontsize=9)

# 3. Square root scale (another alternative - less extreme than log)
counts3, bins3, patches3 = ax3.hist(values, bins=BIN_NUM, alpha=0.7, color='lightgreen', edgecolor='black')
ax3.set_yscale('function', functions=(lambda x: x**0.5, lambda x: x**2))  # Square root scale
ax3.set_xlabel('Value')
ax3.set_ylabel('Frequency (sqrt scale)')
ax3.set_title('Square Root Scale\n(Gentler compression)')
ax3.grid(True, alpha=0.3)

# Add count labels
for count, patch in zip(counts3, patches3):
    if count > 0:
        x = patch.get_x() + patch.get_width()/2
        y = patch.get_height()
        ax3.text(x, y, f'{int(count)}', ha='center', va='bottom', fontsize=9)

plt.tight_layout()
plt.show()

# Print which bin has the most values
max_bin_idx = np.argmax(counts1)
print(f"\n=== ANALYSIS ===")
print(f"Total data points: {len(values)}")
print(f"Most values are in bin {max_bin_idx+1}: {bins1[max_bin_idx]:.4g} to {bins1[max_bin_idx+1]:.4g}")
print(f"This bin contains {counts1[max_bin_idx]} values ({100*counts1[max_bin_idx]/len(values):.1f}% of all data)")
print(f"\nTop 3 bins:")
sorted_indices = np.argsort(counts1)[::-1]
for i, idx in enumerate(sorted_indices[:3], 1):
    print(f"{i}. Bin {idx+1}: {counts1[idx]} values")
