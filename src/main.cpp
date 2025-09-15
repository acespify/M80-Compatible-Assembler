// main.cpp
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "Assembler.h"
#include <algorithm>
#include <iomanip>

// Forward declarations for helper functions
void write_binary_file(const std::string& filename, const std::vector<uint8_t>& data);
void write_symbol_table(const std::string& filename, const std::map<std::string, uint16_t>& table);
std::string get_base_filename(const std::string& path);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename.asm> [-o outfile] [-s]" << std::endl;
        return 1;
    }

    std::string in_filename = argv[1];
    std::string out_filename = "";
    bool save_symtab = false;

    // Simple argument parsing
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            out_filename = argv[++i];
        } else if (arg == "-s") {
            save_symtab = true;
        }
    }

    // Read input file
    std::ifstream infile(in_filename);
    if (!infile) {
        std::cerr << "Error: Cannot open input file " << in_filename << std::endl;
        return 1;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line)) {
        lines.push_back(line);
    }

    // Determine output filenames
    std::string base_name = get_base_filename(in_filename);
    if (out_filename.empty()) {
        out_filename = base_name + ".com";
    }
    std::string sym_filename = get_base_filename(out_filename) + ".sym";


    // Assemble the code
    Assembler asm80;
    try {
        asm80.assemble(lines);
    } catch (...) {
        // Error message is already printed by report_error
        return 1;
    }
    
    // Write output files
    write_binary_file(out_filename, asm80.getOutput());
    std::cout << asm80.getOutput().size() << " bytes written to " << out_filename << std::endl;

    if (save_symtab) {
        write_symbol_table(sym_filename, asm80.getSymbolTable());
        std::cout << asm80.getSymbolTable().size() << " symbols written to " << sym_filename << std::endl;
    }

    return 0;
}

// Helper function implementations
std::string get_base_filename(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);
    size_t last_dot = filename.rfind('.');
    return (last_dot == std::string::npos) ? filename : filename.substr(0, last_dot);
}

void write_binary_file(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error: Cannot open output file " << filename << std::endl;
        exit(1);
    }
    outfile.write(reinterpret_cast<const char*>(data.data()), data.size());
}

void write_symbol_table(const std::string& filename, const std::map<std::string, uint16_t>& table) {
    if (table.empty()) return;
    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "Error: Cannot open symbol file " << filename << std::endl;
        exit(1);
    }
    
    // Set up stream to print hex values
    outfile << std::hex << std::uppercase << std::setfill('0');
    for (const auto& pair : table) {
        std::string symbol = pair.first;
        if (symbol.length() > 16) symbol = symbol.substr(0, 16);
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
        
        outfile << std::setw(4) << pair.second << " " << symbol << std::endl;
    }
}