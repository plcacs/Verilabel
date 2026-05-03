This artifact contains 3 CSV files used for vulnerability fix classification.

Files
1. 300data_humanlevel.csv
   Human-level analysis of 300 samples serving as the ground truth for evaluation.

2. BigVul_result.csv
   VeriLabel fix-or-not predictions for 6,438 entries from the BigVul dataset.
   Column         Description 
   id             Unique sample identifier 
   project        Project the sample belongs to
   prediction     VeriLabel output (Fixing / Not_Fixing)
   before_func    Function code before the change
   after_func     Function code after the change

3. PrimeVul_result.csv
   VeriLabel fix-or-not predictions for 4,446 entries from the PrimeVul dataset.
   Column         Description
   id             Unique sample identifier
   prediction     VeriLabel output (Fixing / Not_Fixing)
   before_func    Function code before the change
   after_func     Function code after the change


4. Reproducing the Results
   To regenerate the predictions from scratch, follow these steps:

   From BigVul_result.csv or PrimeVul_result.csv, extract the before_func column and save each entry as a separate file in a folder named before/, using the corresponding id as the filename.
   Do the same for the after_func column, saving each file into a folder named after/.
   Run the analysis script:

      python Verilabel_beta3_test.py

   The script will generate several output files. The final fix classification results can be found in "Vulresult.csv".

5. Reproducing Paper Results (Easier Method)
   For convenience, we have provided two ready-to-use folders:

   test_on_BigVul/ — contains input pairs for all 10 BigVul projects
   test_on_PrimeVul/ — contains input pairs for PrimeVul (test, train, and validation splits)

   To reproduce the results:

   Clone the repository.
   Navigate to any project folder inside test_on_BigVul/ or to a subfolder inside test_on_PrimeVul/.
   Run the corresponding script:

      python Verilabel_beta3_test.py

   This will generate a Vulresult.csv file in that directory.

   All tables reported in the paper can be reproduced from the generated Vulresult.csv files.

6. Reproducing Table 6: Baseline Model Evaluation
This artifact contains CSV files and prediction results used for vulnerability fix classification and baseline model evaluation.

## Files

### 1. `300data_humanlevel.csv`

Human-level analysis of 300 samples serving as the ground truth for evaluation.

### 2. `BigVul_result.csv`

VeriLabel fix-or-not predictions for 6,438 entries from the BigVul dataset.

| Column | Description |
|---|---|
| `id` | Unique sample identifier |
| `project` | Project the sample belongs to |
| `prediction` | VeriLabel output (`Fixing` / `Not_Fixing`) |
| `before_func` | Function code before the change |
| `after_func` | Function code after the change |

### 3. `PrimeVul_result.csv`

VeriLabel fix-or-not predictions for 4,446 entries from the PrimeVul dataset.

| Column | Description |
|---|---|
| `id` | Unique sample identifier |
| `prediction` | VeriLabel output (`Fixing` / `Not_Fixing`) |
| `before_func` | Function code before the change |
| `after_func` | Function code after the change |

### 4. `Four_Models_Predictions.zip`

This file contains the prediction results produced by four baseline models across three datasets and five random seeds. These prediction results are used to reproduce the averaged baseline results reported in **Table 6**.

The evaluated baseline models are:

- **LineVul**
- **DeepDFA**
- **CodeBERT**
- **ReVeal**

The baseline implementations and resources were obtained from the following sources:

- LineVul: <https://github.com/awsm-research/LineVul>
- DeepDFA: <https://github.com/ISU-PAAL/DeepDFA>
- CodeBERT and ReVeal projects: <https://doi.org/10.6084/m9.figshare.20791240>

Each model was evaluated on the following three datasets:

1. **Setting 1**: `CWE_IDs_Vulnerable_plus_k_times_non_vulnerable_split.zip`
2. **Setting 2**: `CWE_IDs_Vulnerable_plus_k_times_non_vulnerable_refiltered_by_Bigvul_Fixing_split.tar.gz`
3. **Original BigVul dataset**

For **Setting 1** and **Setting 2**, we first applied the required data preprocessing procedures to convert our datasets into the input formats expected by each baseline model. After preprocessing, each model was retrained on the corresponding processed dataset. For the original BigVul dataset, we followed the original baseline settings and used it as the reference dataset for comparison.

For each model–dataset combination, we conducted **five independent training runs** using different random seeds. The trained models were then used to generate predictions on the corresponding test sets. We summarize the prediction results by computing **F1-score**, **Recall**, and **Precision** for each run, and then report the average values across the five seeds.

## Reproducing VeriLabel Fix Classification Results

To regenerate the VeriLabel predictions from scratch, follow these steps:

1. From `BigVul_result.csv` or `PrimeVul_result.csv`, extract the `before_func` column and save each entry as a separate file in a folder named `before/`, using the corresponding `id` as the filename.
2. Do the same for the `after_func` column, saving each entry into a folder named `after/`.
3. Run the analysis script:

   ```bash
   python Verilabel_beta3_test.py