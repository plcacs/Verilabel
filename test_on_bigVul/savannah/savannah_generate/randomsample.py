import os
import random

# ── CONFIG ─────────────────────────────────────────────
FOLDER_PATH   = "savannah/before"   # change this
SAMPLE_SIZE   = 20                      # how many files to pick
RANDOM_SEED   = 42                       # for reproducibility
OUTPUT_FILE   = "sampled_files.txt"      # optional: save the list
# ───────────────────────────────────────────────────────

# 1. Collect all file names in the folder
all_files = [
    f for f in os.listdir(FOLDER_PATH)
    if os.path.isfile(os.path.join(FOLDER_PATH, f))
]

print(f"Total files found : {len(all_files)}")

# 2. Guard: can't sample more than what exists
if SAMPLE_SIZE > len(all_files):
    print(f"Warning: only {len(all_files)} files available. Sampling all of them.")
    SAMPLE_SIZE = len(all_files)

# 3. Random sample (without replacement)
random.seed(RANDOM_SEED)
sampled_files = random.sample(all_files, SAMPLE_SIZE)

print(f"Files sampled     : {len(sampled_files)}")

# 4. Print the sampled file names
for i, fname in enumerate(sampled_files, 1):
    print(f"  {i:>4}. {fname}")

# 5. Optionally save to a text file
with open(OUTPUT_FILE, "w") as f:
    for fname in sampled_files:
        f.write(fname + "\n")

print(f"\nSaved to: {OUTPUT_FILE}")