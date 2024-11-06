#!/bin/bash

# Output file where all results will be concatenated
output="test_results.txt"

# Remove the output file if it already exists
rm -f "$output"

# Directory containing your test files
test_dir="test"

# Check if the test directory exists
if [ ! -d "$test_dir" ]; then
    echo "Test directory '$test_dir' does not exist."
    exit 1
fi

# Loop through all .io files in the test directory
for file in "$test_dir"/*.io; do
    # Check if there are any .io files
    if [ ! -e "$file" ]; then
        echo "No .io files found in '$test_dir' directory."
        exit 1
    fi

    # Get the filename without the directory path
    filename=$(basename "$file")

    # Write the filename to the output file
    echo "Filename: $filename" >> "$output"

    # Run the ioc compiler with the desired options and append the output
    ./ioc -print-quads -stop-after-print "$file" >> "$output" 2>&1

    # Add a separator between files
    echo -e "\n---\n" >> "$output"
done

echo "All test results have been saved to $output."
