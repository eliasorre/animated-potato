import sys
import numpy as np
from collections import Counter
import matplotlib.pyplot as plt


def read_debug_output_array(filename):
    print("Reading debug file")
    array = []
    with open(filename, 'r') as file:
        for line in file:
            try:
                array.append(int(line.split()[1], 16))
            except:
                continue
    return np.array(array)

def read_tool_output_array(filename):
    print("Reading tool file")
    array = []
    with open(filename, 'r') as file:
        for line in file:
            try:
                array.append(int(line.split()[1], 0))
            except:
                continue
    return np.array(array)
    
def extract_assembly_context(instruction_addresses, assembly_file_path, lines_above=5, lines_below=10):
    with open(assembly_file_path, 'r') as file:
        assembly_lines = file.readlines()

    results = []
    line_numbers = len(assembly_lines)
    for line_number, line in enumerate(assembly_lines):
        # Check if the line contains any of the instruction addresses
        print (f"{round((line_number/line_numbers)*100, 2)}" + " percent complete", end="\r", flush=True)
        try:
            if any(addr in line.strip().split()[0] for addr in instruction_addresses):
                # Calculate the range of lines to extract
                start = max(0, line_number - lines_above)
                end = min(len(assembly_lines), line_number + lines_below + 1)
                context = assembly_lines[start:end]

                # Add the extracted lines to the results
                results.append(''.join(context))
        except:
            continue

    return results


def create_memory_access_plots(debug_memory_accesses, tool_memory_accesses, threshold_value, output_file):
    # Convert memory addresses to integers
    addresses_int = np.array([address for address in debug_memory_accesses])

    # Initialize arrays for lower and higher addresses
    both_addresses = np.zeros_like(addresses_int)
    only_debug = np.zeros_like(addresses_int)
    j = 0
    # Populate arrays based on the threshold
    for i, address in enumerate(addresses_int):
        print(f"Progress: {round(i/len(addresses_int),4) * 100}%", end="\r", flush=True)
        if tool_memory_accesses[j] == address:
            both_addresses[i] = address
            j = j + 1
        else:
            only_debug[i] = address

    print("--- ---")
    print("Found in both: ", np.nonzero(both_addresses))
    print("Found only in debug: ", np.nonzero(only_debug))
    # Find non-zero elements in each array
    non_zero_indices_1 = np.nonzero(both_addresses)
    non_zero_indices_2 = np.nonzero(only_debug)
    print("Both: ", len(non_zero_indices_1[0]))
    print("Only debug: ", len(non_zero_indices_2[0]))
    # x-values are indices of the array
    x_values_1 = np.arange(len(both_addresses))[non_zero_indices_1]
    x_values_2 = np.arange(len(only_debug))[non_zero_indices_2]
    # Time axis (x-axis)
    time_axis = np.arange(len(addresses_int))

    # Create subplots with shared x-axis
    plt.figure(figsize=(14, 6))
    plt.scatter(x_values_1, both_addresses[non_zero_indices_1], color='blue', s=0.01)
    plt.scatter(x_values_2, only_debug[non_zero_indices_2], color='red', s=0.0000001)
    #plt.scatter(time_axis, addresses_int, color='blue', s=0.5)  # s is the size of the point
    plt.title("Detected Bytecode Accesses")
    plt.xlabel('Sequential Order')
    plt.ylabel('Memory Address (Hexadecimal)')
    tick_locations, _ = plt.yticks()
    plt.yticks(tick_locations, [hex(int(tick)) for tick in tick_locations])
    plt.savefig(f"analysis/{output_file}_all.png")
    plt.close()

    # Create subplots with shared x-axis
    plt.figure(figsize=(14, 6))
    plt.scatter(x_values_2, only_debug[non_zero_indices_2], color='red', s=0.01)
    #plt.scatter(time_axis, addresses_int, color='blue', s=0.5)  # s is the size of the point
    plt.title("Non-detected Bytecode Accesses")
    plt.xlabel('Sequential Order')
    plt.ylabel('Memory Address (Hexadecimal)')
    tick_locations, _ = plt.yticks()
    plt.yticks(tick_locations, [hex(int(tick)) for tick in tick_locations])
    plt.savefig(f"analysis/{output_file}_only_debug.png")
    plt.close()


def process_files(tool_output_file, debug_output_file, output_file, assembly_file_path):
    debug_array = read_debug_output_array(debug_output_file)
    tool_output_array = read_tool_output_array(tool_output_file)
    ca = Counter(debug_array)
    cb = Counter(tool_output_array)

    inital_size_debug = ca.total()
    print(inital_size_debug)
    inital_size_tool = cb.total()

    result_a = sorted((ca - cb).elements())
    result_b = sorted((cb - ca).elements())
    print(f"Left of debug array: {round(len(result_a)/inital_size_debug, 4) * 100} %")
    print(f"Left of tool array: {round(len(result_b)/inital_size_tool, 4) * 100}")
    create_memory_access_plots(debug_array, tool_output_array, int("0x640b5eece000", 0), f"{output_file}")

    with open(f"analysis/{output_file}.txt", 'w') as file:
        file.write(f"Percentage of Addresses in 'debugOutput' not found in 'toolOutput': {round(len(result_a)/inital_size_debug, 4) * 100}\n")
        file.write(f"Percentage of Addresses in 'toolOutput' not found in  'debugOutput': {round(len(result_b)/inital_size_tool, 4) * 100}\n")
        file.write("Lines in 'toolOutput' not found in 'debugOutput':\n")

    print(ca.keys() - cb.keys())

    if (assembly_file_path == ""): 
        return
    instruction_addresses = set(instruction_addresses)
    extracted_contexts = extract_assembly_context(instruction_addresses, assembly_file_path)
    with open("analysis/assembly_context.txt", "w") as file:
        for line in extracted_contexts:
            file.write(line + "\n")


# Example usage
if __name__ == "__main__":
    if (len(sys.argv) == 5):
        process_files(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        process_files(sys.argv[1], sys.argv[2], sys.argv[3], "")
