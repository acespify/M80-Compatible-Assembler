// main.cpp
#include "debug.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "Assembler.h"
#include <algorithm>
#include <iomanip>

// Added for due to updates 9-15-25 ay
void to_lower(std::string& sVal);
void write_cross_reference_file(const std::string& filename, Assembler& ayM80);
std::string get_base_filename(const std::string& path); 

// Forward declarations for helper functions
void write_binary_file(const std::string& filename, const std::vector<uint8_t>& data);
void write_symbol_table(const std::string& filename, const std::map<std::string, uint16_t>& table);

// *** Main application logic for M80-Compatible-Assembler ***
int main(int argc, char* argv[]) {
    if (argc < 2) {
        // Updated usage message to show new switches (/l and /O)
        std::cerr << "Usage: " << argv[0] << " <source.asm> [-o out.com] [-s] [/L] [/O]" << std::endl;
        return 1;
    }

    std::string in_filename = "";
    std::string out_filename = "";
    bool save_symtab = false;

    // *** NEW 9-15-25 ay: Flags for M80 Switches ***
    bool generate_listing = false;
    bool octal_mode = false; 
    bool generate_cref = false;

    // *** NEW 9-15-25 ay: Updated argument parsing loop ***
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // ADD THIS DEBUG LINE
        DEBUG_LOG("Processing argument #" << i << ": [" << arg << "]");

        if (arg == "-o") {
            if (i + 1 < argc) {
                out_filename = argv[++i]; // Consume the next argument as the filename
            } else {
                std::cerr << "Error: -o switch requires a filename." << std::endl; return 1;
            }
        } else if (arg == "-s") {
            save_symtab = true;
        } else if (arg == "/L" || arg == "/l" || arg =="-L" || arg == "-l") {
            generate_listing = true;
        } else if (arg == "/C" || arg == "/c" || arg == "-C" || arg == "-c") {
            generate_cref = true;
        } else if (arg == "/O" || arg == "/o" || arg =="-O" || arg == "-o") {
            octal_mode = true;
        } else if (arg[0] == '-' || arg[0] == '/') {
            std::cerr << "Error: Unknown switch " << arg << std::endl; return 1;
        } else { // It's not a switch, must be the input file
            if (in_filename.empty()) {
                in_filename = arg;
            } else {
                std::cerr << "Error: Multiple input files specified." << std::endl; return 1;
            }
        }
    }

    // Making sure a filename has been provided.
    if (in_filename.empty()) {
        std::cerr << "Error: No input file specified." << std::endl;
    }

    // Read input file
    std::ifstream infile(in_filename);
    if (!infile) {
        std::cerr << "Error: Cannot open input file " << in_filename << std::endl;
        return 1;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(infile, line)) { lines.push_back(line); }

    // Determine output filenames
    std::string base_name = get_base_filename(in_filename);
    if (out_filename.empty()) {
        out_filename = base_name + ".com";
    }
    std::string sym_filename = base_name + ".sym";
    std::string lst_filename = base_name + ".lst"; // For listing filename
    std::string crf_filename = base_name + ".crf"; 
    
    // *** Handle the listing file stream ***
    std::ofstream listing_file;
    if(generate_listing) {
        listing_file.open(lst_filename);
        if(!listing_file){
            std::cerr << "ERROR: Cannot open listing file " << lst_filename << std::endl;
            return 1;
        }
    }

    // Assemble the code
    Assembler ayM80;
    if(generate_listing){
        ayM80.set_listing_stream(listing_file); // giving the stream to the assembler
    }
    ayM80.set_octal_mode(octal_mode);
    ayM80.assemble(lines);
        
    // Write output files
    write_binary_file(out_filename, ayM80.getOutput());
    std::cout << ayM80.getOutput().size() << " bytes written to " << out_filename << std::endl;

    if (generate_cref) {
        write_cross_reference_file(crf_filename, ayM80);
        std::cout << "Cross-Reference file written to " << crf_filename << std::endl;
    }
    if (generate_listing) {
        std::cout << "Listing file written to " << lst_filename << std::endl;
    }
    if (save_symtab) {
        write_symbol_table(sym_filename, ayM80.getSymbolTable());
        std::cout << ayM80.getSymbolTable().size() << " symbols written to " << sym_filename << std::endl;
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

void write_cross_reference_file(const std::string& filename, Assembler& ayM80) {
    const auto& crf_data = ayM80.getCrossReferenceData();
    const auto& sym_table = ayM80.getSymbolTable();
    if (crf_data.empty()) return;

    std::ofstream outfile(filename);
    if (!outfile) {
        std::cerr << "ERROR: Cannot open cross-reference file " << filename << std::endl;
        return;
    }

    outfile << "--- Cross-Reference Listing ---\n\n";

    for (const auto& pair : crf_data) {
        std::string symbol = pair.first;
        uint16_t address = sym_table.at(symbol);

        outfile << std::left << std::setw(20) << symbol
                << std::hex << std::uppercase << std::setfill('0')
                << std::setw(4) << address << "   ";
        
        std::vector<int> lines = pair.second;
        std::sort(lines.begin(), lines.end(), [](int a, int b) {
            return abs(a) < abs(b);
        });

        for (int line : lines) {
            if(line < 0) {
                outfile << "#" << abs(line) << " ";
            } else {
                outfile << line << " ";
            }
        }
        outfile << std::endl;
    }
    std::cout << crf_data.size() << " symbols written to " << filename << std::endl;
}