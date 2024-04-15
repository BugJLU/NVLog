def extract_lines_with_prefix_and_save_next_line(input_file, output_file,start,end):
    with open(input_file, 'r') as file:
        lines = file.readlines()

    with open(output_file, 'w') as output:
        for i in range(len(lines)):
            line = lines[i]
            if line.startswith("OP="):
                output.write(line)
            if line.startswith("sudo mount"):
                output.write(line)
            if line.startswith("Run status group") and i + 1 < len(lines):
                result = extract_string_between(lines[i + 1], start, end)
                result = extract_and_maybe_convert_to_number(result, convert_to_number=False)
                output.write(result+"\n")

def extract_and_maybe_convert_to_number(input_string, convert_to_number=True):
    num_str = ""
    has_decimal = False
    for char in input_string:
        if char.isdigit() or char == ".":
            if char == ".":
                if has_decimal:
                    break 
                has_decimal = True
            num_str += char
        elif num_str:  
            break
    
    if num_str and convert_to_number:
        return float(num_str)
    else:
        return num_str

def extract_string_between(input_string, start_string, end_string):
    start_index = input_string.find(start_string)
    if start_index == -1:
        return ""  
    
    end_index = input_string.find(end_string, start_index + len(start_string))
    if end_index == -1:
        return ""  
    
    extracted_string = input_string[start_index + len(start_string):end_index]
    return extracted_string

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

dirpath="path/to/your/directory"
input_file = dirpath+"log"
tmp_file = dirpath+"tmp"
output_file = "output"
start_string = "bw="
end_string = " "       

extract_lines_with_prefix_and_save_next_line(input_file, tmp_file,start_string,end_string)
extract_and_calculate_average(tmp_file, output_file)
