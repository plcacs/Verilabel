# VeriLabel Artifact

This artifact contains the data and scripts for vulnerability fix classification using VeriLabel.

---

## 📁 CSV Files

### 1. `300data_humanlevel.csv`
Human-level analysis of 300 samples serving as the ground truth for evaluation.

---

### 2. `BigVul_result.csv`
VeriLabel fix-or-not predictions for **6,438 entries** from the BigVul dataset.

| Column | Description |
|---|---|
| `id` | Unique sample identifier |
| `project` | Project the sample belongs to |
| `prediction` | VeriLabel output (`Fixing` / `Not_Fixing`) |
| `before_func` | Function code before the change |
| `after_func` | Function code after the change |

---

### 3. `PrimeVul_result.csv`
VeriLabel fix-or-not predictions for **4,446 entries** from the PrimeVul dataset.

| Column | Description |
|---|---|
| `id` | Unique sample identifier |
| `prediction` | VeriLabel output (`Fixing` / `Not_Fixing`) |
| `before_func` | Function code before the change |
| `after_func` | Function code after the change |

---

## 🔁 Reproducing the Results

### Option A: Easier Method (Recommended)

For convenience, two ready-to-use folders are provided:

- **`test_on_BigVul/`** — contains input pairs for all 10 BigVul projects
- **`test_on_PrimeVul/`** — contains input pairs for PrimeVul (test, train, and validation splits)

**Steps:**

1. Clone the repository:
```bash
   git clone <repository-url>
```
2. Navigate to any project folder inside `test_on_BigVul/` or to a subfolder inside `test_on_PrimeVul/`.
3. Run the script:
```bash
   python Verilabel_beta3_test.py
```
4. A `Vulresult.csv` file will be generated in that directory.

> Tables (through 1-5) reported in the paper can be reproduced from the generated `Vulresult.csv` files.

---
### Option B: From Scratch

1. From `BigVul_result.csv` or `PrimeVul_result.csv`, extract the `before_func` column and save each entry as a separate file in a folder named `before/`, using the corresponding `id` as the filename.
2. Do the same for the `after_func` column, saving files into a folder named `after/`.
3. Run the analysis script:
```bash
   python Verilabel_beta3_test.py
```
4. The script will generate several output files. The final fix classification results can be found in **`Vulresult.csv`**.

---

### 4. Reproducing Table 6: Baseline Model Evaluation

Prediction results produced by four baseline models across three datasets and five random seeds. These results are used to reproduce the averaged baseline results reported in **Table 6**.

| Item | Description |
|---|---|
| Evaluated models | LineVul, DeepDFA, CodeBERT, and ReVeal |
| Evaluation datasets | Setting 1, Setting 2, and the original BigVul dataset |
| Random seeds | Five independent training runs for each model-dataset combination |
| Reported metrics | F1-score, Recall, and Precision |
| Output usage | Used to compute the averaged baseline results reported in Table 6 |

The baseline implementations and resources were obtained from the following sources:

| Model | Source |
|---|---|
| LineVul | https://github.com/awsm-research/LineVul |
| DeepDFA | https://github.com/ISU-PAAL/DeepDFA |
| CodeBERT and ReVeal | https://doi.org/10.6084/m9.figshare.20791240 |

Each model was evaluated on the following three datasets:

| Dataset | File / Source |
|---|---|
| Setting 1 | `CWE_IDs_Vulnerable_plus_k_times_non_vulnerable_split.zip` |
| Setting 2 | `CWE_IDs_Vulnerable_plus_k_times_non_vulnerable_refiltered_by_Bigvul_Fixing_split.tar.gz` |
| Original BigVul dataset | Original BigVul dataset used by prior baseline studies |

The datasets for **Setting 1** and **Setting 2** can be downloaded from the following Google Drive folder:

```text
https://drive.google.com/drive/folders/1zvW7TGxRCbrXRah0FSc4I9z-AKwNUwj-
```

The **Original BigVul dataset** can be downloaded from (this is the original Author's drive for BigVul; accessing it will not reveal the current paper's author information):

```text
https://drive.google.com/uc?id=10-kjbsA806Zdk54Ax8J3WvLKGTzN8CMX
```

For **Setting 1** and **Setting 2**, we first applied the required data preprocessing procedures to convert the datasets into the input formats expected by each baseline model. Detailed preprocessing steps are available in the corresponding baseline model repositories listed above. In our evaluation, we followed those preprocessing pipelines and replaced the original input datasets with our Setting 1 and Setting 2 datasets. After preprocessing, each model was retrained on the corresponding processed dataset. For the **original BigVul dataset**, we followed the original baseline settings and used it as the reference dataset for comparison.

For each model-dataset combination, we conducted five independent training runs using different random seeds. The trained models were then used to generate predictions on the corresponding test sets. We summarize the prediction results by computing F1-score, Recall, and Precision for each run, and report the average values across the five seeds. Our preiction result were save on the file "Four_Models_Predictions.zip".


   
