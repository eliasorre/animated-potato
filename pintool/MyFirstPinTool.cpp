#include <stdio.h>
#include <map>
#include <iostream>
#include <fstream>
#include "pin.H"
using std::string;
using std::map;
using std::ofstream; 
using std::ios;

ofstream OutFile;

// Name of output file
KNOB <string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "first_tool.out", "output file");

map<ADDRINT, int> readAddressMap; 
map<ADDRINT, int> instrMap; 

ADDRINT mainModuleBase = 0;
ADDRINT mainModuleHigh = 0;

// Callback for loaded images
VOID Image(IMG img, VOID* v) {
    // Check if it's the main executable
    if (IMG_IsMainExecutable(img)) {
        mainModuleBase = IMG_LowAddress(img);
        mainModuleHigh = IMG_HighAddress(img);
    }
}


// This function increments the times memoryAddr has been read 
VOID incrementMap(ADDRINT baseAddress, ADDRINT readAddress, ADDRINT insAddress) { 
    if (insAddress >= mainModuleBase && insAddress <= mainModuleHigh) {  // Ensure the instruction is from the main module
        readAddressMap[readAddress - mainModuleBase]++; 
        instrMap[insAddress - mainModuleBase]++;
    }
}

// If instruction is memory read we want to add it to the map
VOID Instruction(INS ins, VOID* v) {
    if (INS_IsMemoryRead(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)incrementMap, 
            IARG_MEMORYREAD_EA, 
            IARG_INST_PTR, 
            IARG_END
        );
    }
}

VOID Fini(INT32 code, VOID* v) {
    OutFile.setf(ios::showbase);
    OutFile << std::endl; 
    OutFile << "Memory loads - Address: Count:" << std::endl;
    for (const auto& pair : readAddressMap) {
        OutFile << pair.first << " " << pair.second << std::endl; 
    }
    OutFile << std::endl; 
    OutFile << "Instructions - Address: Count:" << std::endl;
    for (const auto& pair : instrMap) {
        OutFile << pair.first << " " << pair.second << std::endl; 
    }
    OutFile << std::endl; 
    OutFile.close();
}

// Print Help Message
INT32 Usage() {
    std::cerr << "This tool counts the number of memory loads in a map" << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char* argv[]) {
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    // Fix base adress to calculate the correct addresses
    IMG_AddInstrumentFunction(Image, 0);

    // Register Instruction to be called instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}