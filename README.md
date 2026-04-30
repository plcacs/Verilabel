We have 3 .csv files here
1. 300data_humanlevel.csv (human level analysis)
2. BigVul_result.csv is 6438 BigVul fix label by verilabel.
3. PrimeVul_result.csv is 4446 PrimeVul fix label by verilabel
4. if you like to reproduce the data.
   Extract the "before_func" column and put in a "before" folder and keep the file name as corresponding id.
   Do same for "after_func" and put in after folder.
   Then run "treesitter_beta3_test.py" . it will create several files. Check "Vulresult.csv" for the outcome.
