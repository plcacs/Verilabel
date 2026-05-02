import pandas as pd
import os

# Load the CSV
df = pd.read_csv("ghostscript_generate/ghostscript_fixing.csv")

# Get all indices from folder filenames
folder_path = "Fixing/Before"  # change this to your folder path

folder_indices = set()
for filename in os.listdir(folder_path):
    if filename.endswith(".c"):
        index = filename.split("_")[0]  # extract "177741" from "177741_CVE-2011-4128.c"
        folder_indices.add(int(index))

print(f"Found {len(folder_indices)} files in folder")

# Filter CSV rows where index appears in folder
df["index"] = df["index"].astype(int)
filtered = df[df["index"].isin(folder_indices)]

print(f"Matched {len(filtered)} rows from CSV")

# Save
filtered.to_csv("Fixing_ghostscript.csv", index=False)
print("Saved: filtered_output.csv")