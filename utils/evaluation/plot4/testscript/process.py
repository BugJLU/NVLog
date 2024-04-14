import re

def extract_read_write_lines(input_file, output_file):
    with open(input_file, 'r') as file:
        lines = file.readlines()

    output_lines = []

    for line in lines:
        if "READ: bw=" in line or "WRITE: bw=" in line:
            bw_values = re.findall(r'bw=(.*?)MiB\/s', line)
            if bw_values:
                output_lines.extend(bw_values)
            
    with open(output_file, 'w') as file:
        file.write('\n'.join(output_lines))

def process_file(input_file, output_file):
    with open(input_file, 'r') as file:
        lines = file.readlines()

    numbers = []
    sum_total = 0
    count = 0
    output_lines = []

    for line in lines:
        try:
            num = float(line.strip())
            numbers.append(num)
            count += 1

            if count == 6:
                sum_total = sum(numbers)
                avg = sum_total / 3
                output_lines.append(str(avg))

                numbers = []
                sum_total = 0
                count = 0

        except ValueError:
            continue

    if count > 0:
        sum_total = sum(numbers)
        avg = sum_total / 3
        output_lines.append(str(avg))

    with open(output_file, 'w') as file:
        file.write('\n'.join(output_lines))

dirpath="path/to/your/directory"
input_file=dirpath+"log"
tmp_file=dirpath+"tmp"
output_file=dirpath+"output"

# 调用函数，例如提取"input.txt"文件中的数据并将结果输出到"output.txt"文件中
extract_read_write_lines(input_file, tmp_file)
process_file(tmp_file, output_file)