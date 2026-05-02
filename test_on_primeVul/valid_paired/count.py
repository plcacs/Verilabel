import pandas as pd

# Load your CSV file
df = pd.read_csv('Vulresult_valid.csv')

# Count the occurrences in the 'prediction' column
counts = df['prediction'].value_counts()

# Access specific counts (using .get to handle cases where a value might be missing)
fixing = counts.get('Fixing', 0)
not_fixing = counts.get('Not_Fixing', 0)

print(f"Fixing: {fixing}")
print(f"Not_Fixing: {not_fixing}")
