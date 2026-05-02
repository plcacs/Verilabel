import pandas as pd
import os

# 1. Load your existing CSV
df = pd.read_csv('Vulresult_train.csv')

# 2. Define the folders
before_folder = 'before'
after_folder = 'after'

def get_file_content(file_id, folder_path):
    # Construct path: folder/id.c
    path = os.path.join(folder_path, f"{file_id}")
    
    if os.path.exists(path):
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            return f.read()
    return None # Returns empty if file is missing

# 3. Create new columns by mapping the ID to the file contents
df['before_func'] = df['id'].apply(lambda x: get_file_content(x, before_folder))
df['after_func'] = df['id'].apply(lambda x: get_file_content(x, after_folder))

# 4. Save the new CSV
df.to_csv('updated_predictions.csv', index=False)

print("Done! New CSV saved as updated_predictions.csv")
