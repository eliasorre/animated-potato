import matplotlib.pyplot as plt
import sys
import os

def process_data(data):
    sorted_data = sorted(data.items(), key=lambda x: x[1], reverse=True)
    
    # Top 40 addresses and their counts
    top_40 = sorted_data[:40]

    # Average of the rest
    rest = sorted_data[40:]
    avg_of_rest = sum([count for _, count in rest]) / len(rest) if rest else 0
    
    return top_40, avg_of_rest, sorted_data

def plot_data(sorted_data, title, filename):
    _, counts = zip(*sorted_data)
    indices = range(1, len(counts) + 1)

    # Plotting
    plt.figure(figsize=(15, 7))
    plt.plot(indices, counts, marker='o')
    plt.xlabel("Addresses (in decreasing order of counts)")
    plt.ylabel("#Count")
    plt.title(title)
    plt.tight_layout()
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.savefig(os.path.join("analysis", filename))
    plt.close()

def main(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()

    memory_data = {}
    instruction_data = {}
    processing_memory = True  # We start with memory data

    for line in lines:
        line = line.strip()
        if line == "Instructions - Address: Count:":
            processing_memory = False
            continue

        try:
            address, count = map(int, line.split())
            if processing_memory:
                memory_data[address] = count
            else:
                instruction_data[address] = count
        except:
            continue

    # Process memory data
    memory_top_40, memory_avg_of_rest, memory_sorted = process_data(memory_data)
    print("Memory - Top 40 addresses:")
    for address, count in memory_top_40:
        print(address, count)
    print("Memory - Average of the rest:", memory_avg_of_rest)
    
    # Plot memory data
    plot_data(memory_sorted, "Top Memory Load Addresses", "memory_load_addresses.png")

    # Process instruction data
    instruction_top_40, instruction_avg_of_rest, instruction_sorted = process_data(instruction_data)
    print("\nInstructions - Top 40 addresses:")
    for address, count in instruction_top_40:
        print(address, count)
    print("Instructions - Average of the rest:", instruction_avg_of_rest)

    # Plot instruction data
    plot_data(instruction_sorted[1:], "Top Instruction Addresses", "instruction_addresses.png")

    with open('analysis/output.txt', 'w') as f:
        f.write('Top 40 - Memory loads')
        for i, j in memory_top_40:
            f.write(str(hex(i)) + " " + str(j))
            f.write("\n")
        f.write(str(memory_avg_of_rest))

        f.write('Top 40 - Instruction loads')
        for i, j in instruction_top_40:
            f.write(str(hex(i)) + " " + str(j))
            f.write("\n")
        f.write(str(instruction_avg_of_rest))

if __name__ == "__main__":
    filename = sys.argv[1]
    main(filename)




