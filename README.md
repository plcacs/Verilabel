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
