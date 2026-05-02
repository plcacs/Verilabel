import pandas as pd
import os

def create_c_files_from_csv(csv_file, code_column1, code_column2, name_column1, name_column2, output_folder1, output_folder2):
    """
    Reads a CSV file, extracts the specified columns, and creates .c files with the extracted filenames.

    Parameters:
    csv_file (str): Path to the input CSV file.
    code_column (str): Column name containing C code.
    name_column (str): Column name containing file names.
    output_folder (str): Directory to save generated .c files.
    """
    
    # Read the CSV file
    df = pd.read_csv(csv_file)

    # Check if both columns exist
    if code_column1 not in df.columns or code_column2 not in df.columns or name_column1 not in df.columns:
        print(f"Error: One or both specified columns '{code_column1}' and '{name_column1}' not found in the CSV file.")
        return

    # Create output directory if not exists
    os.makedirs(output_folder1, exist_ok=True)
    os.makedirs(output_folder2, exist_ok=True)
    # Iterate through rows and create .c files
    for i, row in df.iterrows():
        value1 = str(row[name_column1]).strip()
        value2 = str(row[name_column2]).strip()
        file_name = f"{value1}_{value2}"  # Get filename from the name_column
        before_file_content = str(row[code_column1])       # Get code from the code_column
        after_file_content = str(row[code_column2])  
        # Skip if filename or code is empty
        if pd.isna(file_name) or pd.isna(before_file_content) or pd.isna(after_file_content) or not file_name:
            continue

        # Ensure filename has a .c extension
        if not file_name.endswith(".c"):
            file_name += ".c"

        file_path1 = os.path.join(output_folder1, file_name)
        file_path2 = os.path.join(output_folder2, file_name)
        # Write content to .c file
        with open(file_path1, "w") as f:
            f.write(before_file_content)
        with open(file_path2, "w") as f:
            f.write(after_file_content)

        print(f"Created: {file_path1}")
        print(f"Created: {file_path2}")

# Example Usage
csv_file = "php_fixing.csv"    # Replace with your CSV file
code_column1 ="func_before"
code_column2 = "func_after"      # Column containing C code
name_column1 = "index"  # Column containing filenames
name_column2 = "CVE ID" 
output_folder1 = "before" # Output folder for generated .c files
output_folder2 ="after"

create_c_files_from_csv(csv_file, code_column1,code_column2, name_column1, name_column2,output_folder1, output_folder2)
