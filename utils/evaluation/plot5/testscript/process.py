import re

def extract_and_calculate_average(input_file, output_file):
    with open(input_file, 'r') as file:
        lines = file.readlines()

    output_lines = []
    temp_nums = []

    for line in lines:
        try:
            num = float(line.strip())
            temp_nums.append(num)
            if len(temp_nums) == 3:
                avg = sum(temp_nums) / len(temp_nums)
                output_lines.append(str(avg))
                temp_nums = []
        except ValueError:
            continue

    if temp_nums:
        avg = sum(temp_nums) / len(temp_nums)
        output_lines.append(str(avg))

    with open(output_file, 'w') as file:
        file.write('\n'.join(output_lines))

def extract_between_strings(input_string, start_string, end_string):
    start_index = input_string.find(start_string)
    if start_index == -1:
        return None
    
    start_index += len(start_string)
    
    end_index = input_string.find(end_string, start_index)
    if end_index == -1:
        return None
    
    return input_string[start_index:end_index]

def extract_lines(input_file, output_file):
    with open(input_file, 'r') as file:
        lines = file.readlines()

    output_lines = []

    for line in lines:
        if "mount -t" in line or "IO Summary" in line or line.startswith("filebench -f"):
            if "IO Summary" in line:
                io_summary = extract_between_strings(line,"rd/wr ","mb/s ")
                output_lines.append(io_summary)
            else:
                output_lines.append(line)
    with open(output_file, 'w') as file:
        file.write('\n'.join(output_lines))

dirpath="path/to/your/directory"
input_file=dirpath+"log"
tmp_file=dirpath+"tmp"
output_file=dirpath+"output"

extract_lines(input_file, tmp_file)
extract_and_calculate_average(tmp_file, output_file)