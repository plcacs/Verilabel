import pandas as pd

# 1. Load your CSV files
df1 = pd.read_csv('linux_sampled_files.csv')
df2 = pd.read_csv('linux_fixing.csv')

# 2. Merge on 'index' to pull in the specific columns you need
# 'how="left"' ensures you keep everything in your first file
result = pd.merge(df1, df2[['index', 'codeLink', 'commit_id']], on='index', how='left')

# 3. Save the combined data to a new file
result.to_csv('humanlevel.csv', index=False)
