#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstdint>

// Holds the definition of a user-defined macro, including its name,
// the list of parameter names, and the lines of code in its body.
struct Macro {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> body_lines;
};

// The main class that encapsulates all the logic for the cross-assembler.
class Assembler {
public:
    // *** Public Interface ***
    Assembler();
    void assemble(const std::vector<std::string>& lines);
    const std::vector<uint8_t>& getOutput() const;
    const std::map<std::string, uint16_t>& getSymbolTable() const;
    void set_listing_stream(std::ostream& stream);
    void set_octal_mode(bool enabled);
    const std::map<std::string, std::vector<int>>& getCrossReferenceData() const;

private:
    // *** State Variables ***
    std::ostream* listing_stream = nullptr;
    bool octal_mode = false;
    int lineno;                         // Current line number from the source file.
    uint16_t address;                   // Current memory address (location counter).
    int source_pass;                    // Which pass we are on (1 or 2).
    bool assembly_finished;             // Flag set by the END directive.
    int macro_expansion_counter;        // Counter to generate unique local labels.
    std::vector<uint8_t> output;        // The generated machine code.
    std::map<std::string, uint16_t> symbol_table; // Stores all defined labels and their addresses.
    std::map<std::string, Macro> macros;  // Stores all defined macros.
    std::vector<bool> if_stack;         // Manages nested IF/ENDIF conditional blocks.
    std::map<std::string, std::vector<int>> cross_reference_data; // Map of: {"symbol_name" -> vector of line numbers }

    // *** Parsed Tokens ***
    // Member variables to hold the parts of a single parsed line of assembly.
    std::string label, mnemonic, operand1, operand2, comment;

    // *** Mnemonic Dispatch ***
    // A map to connect mnemonic strings (e.g., "mov") to their handler functions.
    std::map<std::string, void (Assembler::*)()> mnemonic_handlers;
    enum ImmediateType { IMMEDIATE8 = 8, IMMEDIATE16 = 16 };

    // --- Core Methods ---
    void initialize_mnemonic_handlers();
    void reset_state();
    void preprocess_macros(const std::vector<std::string>& lines);
    void do_pass(const std::vector<std::string>& lines);
    void expand_and_process_line(const std::string& line, int original_lineno);
    void parse(std::string line);
    void process_instruction();
    void report_error(const std::string& message, int line_num) const;

    // --- Pass Logic ---
    void pass_action(int instruction_size, const std::vector<uint8_t>& output_bytes, bool should_add_label = true);
    void add_label();

    // --- Expression Evaluation Engine ---
    int evaluate_expression(const std::string& expr);
    int evaluate_expression(std::string::const_iterator& it, std::string::const_iterator end);
    int parse_expr_term(std::string::const_iterator& it, std::string::const_iterator end);
    int parse_expr_factor(std::string::const_iterator& it, std::string::const_iterator end);
    int evaluate_single_term(const std::string& term);
    bool evaluate_conditional(const std::string& expr);
    std::string get_token(std::string::const_iterator& it, std::string::const_iterator end);
    
    // --- Instruction & Directive Handlers ---
    void nop(); void lxi();  void stax(); void inx();  void inr();  void dcr();
    void mvi(); void rlc();  void dad();  void ldax(); void dcx();  void rrc();
    void ral(); void rar();  void shld(); void daa();  void lhld(); void cma();
    void sta(); void stc();  void lda();  void cmc();  void mov();  void hlt();
    void add(); void adc();  void sub();  void sbb();  void ana();  void xra();
    void ora(); void cmp();  void rnz();  void pop();  void jnz();  void jmp();
    void cnz(); void push(); void adi();  void rst();  void rz();   void ret();
    void jz();  void cz();   void call(); void aci();  void rnc();  void jnc();
    void i80_out(); void cnc();  void sui();  void rc();   void jc();   void i80_in();
    void cc();  void sbi();  void jpe();  void rpo();  void jpo();  void xthl();
    void cpo(); void ani();  void rpe();  void pchl(); void xchg(); void cpe();
    void xri(); void rp();   void jp();   void di();   void cp();   void ori();
    void rm();  void sphl(); void jm();   void ei();   void cm();   void cpi();
    void db();  void ds();   void dw();   void end();  void equ();  void name();
    void org(); void title();
    void sim(); void rim();

    // --- Helper Methods ---
    void check_operands(bool valid, const std::string& mnemonic_name);
    int register_offset8(std::string raw_register);
    int register_offset16();
    void immediate_operand(ImmediateType operand_type = IMMEDIATE8);
    void address16(const std::string& operand);
    int get_number(std::string input) const;
    bool should_skip() const;
    bool is_quote_delimited(const std::string& s) const;
    bool is_char_constant(const std::string& s) const;
};

#endif // ASSEMBLER_H