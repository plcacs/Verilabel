import pandas as pd
import os

# Configuration
before_dir = 'before'
after_dir = 'after'
output_csv = 'ghostscript_notfix.csv'

data = []

# List all .c files in the before folder
for filename in os.listdir(before_dir):
    if filename.endswith('.c'):
        file_id = filename.replace('.c', '')
        
        # Define paths
        before_path = os.path.join(before_dir, filename)
        after_path = os.path.join(after_dir, filename)
        
        # Read 'before' content
        with open(before_path, 'r', encoding='utf-8', errors='ignore') as f:
            before_content = f.read()
            
        # Read 'after' content (assumes file exists since names match)
        if os.path.exists(after_path):
            with open(after_path, 'r', encoding='utf-8', errors='ignore') as f:
                after_content = f.read()
        else:
            after_content = None

        # Append row to data list
        data.append({
            'id': file_id,
            'project':'ghostscript',
            'prediction': 'Not_Fixing',
            'before_func': before_content,
            'after_func': after_content
        })

# Create DataFrame and Export
df = pd.DataFrame(data)
df.to_csv(output_csv, index=False)

print(f"Created {output_csv} with {len(df)} rows.")
