#define PIN_DEPRECATED_WARNINGS 0
#include <stdio.h>
#include <map>
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <inttypes.h>
#include <chrono>
#include "pin.H"

using std::string;
using std::map;
using std::ofstream; 
using std::ios;

ofstream OutFile;

// Name of output file
KNOB <string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "first_tool.out", "output file");

// Addresses used for 
ADDRINT mainModuleBase = 0;
ADDRINT mainModuleHigh = 0;

// Callback for loaded images - to find the base and high of the program, and thus calculate offsets
VOID Image(IMG img, VOID* v) {
    if (IMG_IsMainExecutable(img)) {
        mainModuleBase = IMG_LowAddress(img);
        mainModuleHigh = IMG_HighAddress(img);
    }
}

// Currently unused, but will most likely be used in the future to increase generality (less register-specific condionals)
enum PatternState {
    PS_NONE,
    PS_MOVZWL,
    PS_OR_REGX_R13D, 
    PS_LEA,
    PS_MOV 
};

// Used to save interesting memory reads
struct MemoryRead {
    ADDRINT readAddress;
    ADDRINT insAddress;
    UINT32 regId;
    BOOL ORred = false;
};
// When we find a bytecode load
struct ByteCodeRead {
    ADDRINT readAddress;
    ADDRINT insAddress;
};

PatternState currentState = PS_NONE;
std::vector<MemoryRead> possibleByteCodeLoads; 
std::vector<ByteCodeRead> byteCodeReads; 

VOID PossibleBytecodeLoad(ADDRINT instrAddress, ADDRINT memoryLoadAddress, UINT32 regId) {
    MemoryRead memoryRead; 
    memoryRead.readAddress = memoryLoadAddress;
    memoryRead.insAddress = instrAddress - mainModuleBase;
    memoryRead.regId = regId;
    auto it = std::find_if(possibleByteCodeLoads.begin(), possibleByteCodeLoads.end(),
                           [regId](const MemoryRead& tempMemoryRead) {
                               return tempMemoryRead.regId == regId;
                           });

    if (it != possibleByteCodeLoads.end()) {
        possibleByteCodeLoads.erase(it);
    } 
    possibleByteCodeLoads.push_back(memoryRead);
    currentState = PS_NONE;
}

VOID NotBytecodeLoad(UINT32 regId) {
    auto it = std::find_if(possibleByteCodeLoads.begin(), possibleByteCodeLoads.end(),
                           [regId](const MemoryRead& tempMemoryRead) {
                               return tempMemoryRead.regId == regId;
                           });

    if (it != possibleByteCodeLoads.end()) {
        // Erase the old element and insert the modified one
        possibleByteCodeLoads.erase(it);
    } 
    currentState = PS_NONE;
}

VOID OnOrInstruction(UINT32 regId) {
    auto it = std::find_if(possibleByteCodeLoads.begin(), possibleByteCodeLoads.end(),
                           [regId](const MemoryRead& memoryRead) {
                               return memoryRead.regId == regId;
                           });

    if (it != possibleByteCodeLoads.end()) {
        MemoryRead modifiedMemoryRead = *it;
        modifiedMemoryRead.ORred = true;
        possibleByteCodeLoads.erase(it);
        possibleByteCodeLoads.push_back(modifiedMemoryRead);
        currentState = PS_OR_REGX_R13D;
    }
}

VOID OnOffsetLoad() {
    if (currentState != PS_OR_REGX_R13D) {
        possibleByteCodeLoads.clear();
        return;
    }

    auto it = std::find_if(possibleByteCodeLoads.begin(), possibleByteCodeLoads.end(),
                        [](const MemoryRead& memoryRead) {
                            return memoryRead.ORred == true;
                        });
                        
    if (it != possibleByteCodeLoads.end()){
        ByteCodeRead byteCodeRead;
        byteCodeRead.readAddress = it->readAddress;
        byteCodeRead.insAddress = it->insAddress;
        byteCodeReads.push_back(byteCodeRead);
        possibleByteCodeLoads.clear();
    }
}

// // Planned for future use if making more general control flow
// VOID Jumped() {
//     //currentState = PS_NONE;
//     //possibleByteCodeLoads.clear();
// }

VOID Instruction(INS ins, VOID* v) {
    // Ensure the instruction is from the main module
    if (INS_Address(ins) >= mainModuleBase && INS_Address(ins) <= mainModuleHigh) {          
        if (INS_IsMemoryRead(ins)) {
            size_t numWrittenRegs = INS_MaxNumWRegs(ins);
            for (size_t i = 0; i < numWrittenRegs; ++i) {
                REG destReg = INS_RegW(ins, i);
                if (destReg != REG_INVALID()) {
                    UINT32 regId = static_cast<UINT32>(destReg);
                    if (INS_Opcode(ins) == XED_ICLASS_MOVZX) {
                        REG sourceReg = INS_RegR(ins, 0);
                        if (!(REG_StringShort(destReg) == "edx" && (REG_StringShort(sourceReg) == "r13b" || REG_StringShort(sourceReg) == "r12b"))) {
                            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PossibleBytecodeLoad,
                                        IARG_INST_PTR, 
                                        IARG_MEMORYREAD_EA, 
                                        IARG_UINT32, regId,
                                        IARG_END);
                        }
                    } else {
                        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)NotBytecodeLoad, 
                                    IARG_UINT32, regId,
                                    IARG_END);
                    }
                }
            }
        }


        if (INS_Opcode(ins) == XED_ICLASS_OR && INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1)) {
            REG reg = INS_RegW(ins, 0);
            if (REG_StringShort(reg) == "r13d" || REG_StringShort(reg) == "r12d") {
                REG sourceReg = INS_RegR(ins, 1);
                if (sourceReg != REG_INVALID()) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)OnOrInstruction,
                                IARG_UINT32, static_cast<UINT32>(sourceReg),
                                IARG_END);
                }
            }
        }
        if (INS_Opcode(ins) == XED_ICLASS_MOVZX) {
            REG sourceReg = INS_RegR(ins, 0);
            REG destReg = INS_RegW(ins, 0);
            if (destReg != REG_INVALID() && sourceReg != REG_INVALID()){
                if (REG_StringShort(destReg) == "edx" && (REG_StringShort(sourceReg) == "r13b" || REG_StringShort(sourceReg) == "r12b")) {
                    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)OnOffsetLoad, IARG_END);
                }
            }  
        }
        
        // // Planned for use if making more general control flow
        // if (INS_Opcode(ins) == XED_ICLASS_JMP) {
        //     INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Jumped, IARG_END);
        // }
    }
}


std::unordered_map<std::string, int> CountPatterns(const std::vector<std::vector<string>>& patterns) {
    std::unordered_map<std::string, int> counts;
    for (const auto& pattern : patterns) {
        std::string key;
        for (const string &insMnemonic : pattern) {
            key+= insMnemonic + ",";
        } 
        counts[key]++;
    }
    return counts;
}

VOID Fini(INT32 code, VOID* v) {        
    std::cout << "Time to generate results!" << std::endl;
    std::cout << "Number of bytecode founds: " << std::dec << byteCodeReads.size() << std::endl;
    OutFile.setf(ios::showbase);
    for (ByteCodeRead byteCodeRead : byteCodeReads) {
        OutFile << std::hex << "MemoryAddress: " << byteCodeRead.readAddress << " InsAddress: " << byteCodeRead.insAddress << std::endl;
    }
    OutFile.close();
}

// Print Help Message
INT32 Usage() {
    std::cerr << "This tool is used to label memory loads which are connected to bytecode loads (for CPython 3.11)" << std::endl;
    std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
    return -1;
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return Usage();
    
    OutFile.open(KnobOutputFile.Value().c_str());

    // Fix base adress to enable calculation of offset
    IMG_AddInstrumentFunction(Image, 0);

    std::printf("Starting PinTool! \n");
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();

    return 0;
}