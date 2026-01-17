import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Read your data
data = pd.read_csv('output.csv', header=None)
values = data[0].dropna()
values = values / 1000

# Choose your desired bin width
bin_width = 50.0  # Change this to your preferred bin size

# Calculate bin edges based on bin width
min_val = values.min()
max_val = values.max()

# Create bins from min to max with fixed width
# + bin_width to ensure we include the max value
bins = np.arange(min_val, max_val + bin_width, bin_width)

print(f"Data range: {min_val:.4g} to {max_val:.4g}")
print(f"Bin width: {bin_width}")
print(f"Number of bins: {len(bins) - 1}")
print(f"Bin edges: {bins}")

# Plot histogram with fixed bin width
plt.figure(figsize=(10, 6))
counts, bins, patches = plt.hist(values, bins=bins, alpha=0.7, 
                                 edgecolor='black', color='steelblue')
#
#Ширина корзин

# Log scale to handle steepness
plt.yscale('log')
plt.xlabel('Время, мкс')
plt.ylabel('Частота (Логарифмическая шкала)')
plt.title(f"Гистограмма  времени между операциями чтения и записи\nЧисло корзин={np.ceil((max_val - min_val)/bin_width)}, Ширина корзин=50 мкс")
plt.grid(True, alpha=0.3)

# Add bin labels
for i, (count, patch) in enumerate(zip(counts, patches)):
    if count > 0:
        x = patch.get_x() + patch.get_width()/2
        y = count * 1.1
        plt.text(x, y, f'{int(count)}', ha='center', va='bottom', fontsize=9)

plt.tight_layout()
plt.show()
