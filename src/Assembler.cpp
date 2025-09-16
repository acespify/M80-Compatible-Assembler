#include "debug.h"
#include "Assembler.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <stdexcept>

// --- Helper Functions ---
// Removes leading whitespace from a string.
void ltrim(std::string &s) { s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); })); }

// Removes trailing whitespace from a string.
void rtrim(std::string &s) { s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end()); }

// Removes both leading and trailing whitespace.
void trim(std::string &s) { ltrim(s); rtrim(s); }

// Converts a string to lowercase.
void to_lower(std::string& sVal) { std::transform(sVal.begin(), sVal.end(), sVal.begin(), [](unsigned char c){ return std::tolower(c); }); }

// Splits a string by a delimiter, correctly handling nested quotes and angle brackets.
std::vector<std::string> split_args(const std::string& s, char delimiter) {
    std::string line_without_comment = s.substr(0, s.find(';'));
    std::vector<std::string> tokens;
    std::string current_token;
    int bracket_level = 0;
    bool in_quotes = false;
    for (char c : line_without_comment) {
        if (c == '\'' || c == '"') in_quotes = !in_quotes;
        if (c == '<') bracket_level++;
        if (c == '>') bracket_level--;
        if (c == delimiter && !in_quotes && bracket_level == 0) {
            trim(current_token);
            tokens.push_back(current_token);
            current_token = "";
        } else {
            current_token += c;
        }
    }
    trim(current_token);
    tokens.push_back(current_token);
    return tokens;
}

void Assembler::set_listing_stream(std::ostream& stream) {
    listing_stream = &stream;
}

void Assembler::set_octal_mode(bool enabled){
    this->octal_mode = enabled;
}

// --- Assembler Class Implementation ---
// Constructor: Initializes the mnemonic handler map.
Assembler::Assembler() { initialize_mnemonic_handlers(); reset_state(); }
// Resets all state variables to their defaults for a fresh assembly run.
void Assembler::reset_state() { lineno = 0; address = 0; source_pass = 1; assembly_finished = false; macro_expansion_counter = 0; output.clear(); symbol_table.clear(); macros.clear(); }

// Public gettters for the final output.
const std::vector<uint8_t>& Assembler::getOutput() const { return output; }
const std::map<std::string, uint16_t>& Assembler::getSymbolTable() const { return symbol_table; }
const std::map<std::string, std::vector<int>>& Assembler::getCrossReferenceData() const { return cross_reference_data; }

// Reports an error message to the console and exits the program.
void Assembler::report_error(const std::string& message, int line_num) const { std::cerr << "asm80> line " << (line_num + 1) << ": " << message << std::endl; exit(1); }

// Main entry point for the assembly process.
void Assembler::assemble(const std::vector<std::string>& lines) {
    reset_state();
    // Pass 0: Find all macro definitions before doing anything else.
    preprocess_macros(lines);
    // Pass 1: Build the symbol table.
    source_pass = 1;
    do_pass(lines);
    // Pass 2: Generate the machine code.
    source_pass = 2;
    address = 0;
    output.clear();
    assembly_finished = false;
    macro_expansion_counter = 0;
    do_pass(lines);
}

// Pass 0: Iterates through the source code to find and store all macro definitions.
void Assembler::preprocess_macros(const std::vector<std::string>& lines) {
    bool in_macro_def = false;
    Macro current_macro;
    for (int i = 0; i < lines.size(); ++i) {
        std::string temp_line = lines[i];
        trim(temp_line);
        std::stringstream ss(temp_line);
        std::string first_word, second_word;
        ss >> first_word >> second_word;
        to_lower(first_word);
        to_lower(second_word);
        if (second_word == "macro") {
            if (in_macro_def) report_error("nested macro definitions are not supported", i);
            in_macro_def = true;
            current_macro = Macro();
            current_macro.name = first_word;
            std::string params_part;
            std::getline(ss, params_part);
            current_macro.params = split_args(params_part, ',');
        } else if (first_word == "endm" || first_word == "mend") {
            if (!in_macro_def) report_error("ENDM without MACRO", i);
            in_macro_def = false;
            macros[current_macro.name] = current_macro;
        } else if (in_macro_def) {
            current_macro.body_lines.push_back(lines[i]);
        }
    }
    if (in_macro_def) report_error("MACRO definition not closed with ENDM", lines.size());
}

// Main loop for Pass 1 and Pass 2. Skips macro definitions and passes other lines to the processor.
void Assembler::do_pass(const std::vector<std::string>& lines) {
    bool in_macro_def = false;
    if_stack.clear();
    for (lineno = 0; lineno < lines.size(); ++lineno) {
        if (assembly_finished) break;
        std::string current_line = lines[lineno];

        // Updating for listing file logic
        uint16_t line_address = this->address;
        size_t bytes_before = this->output.size();

        std::string temp_line = current_line;
        trim(temp_line);
        if (temp_line.empty()) { if (source_pass == 2 && listing_stream) { *listing_stream << current_line << std::endl;} continue;} 
        std::stringstream ss(temp_line);
        std::string first_word, second_word;
        ss >> first_word >> second_word;
        to_lower(second_word);
        if (second_word == "macro") in_macro_def = true;
        if (in_macro_def) {
            std::string lower_first;
            std::stringstream(temp_line) >> lower_first;
            to_lower(lower_first);
            if (lower_first == "endm" || lower_first == "mend") in_macro_def = false;
            continue;
        }
        expand_and_process_line(current_line, lineno);

        // Listing File Logic
        if (source_pass == 2 && listing_stream) {
            size_t bytes_after = this->output.size();
            std::stringstream line_data_stream;

            // Selecting Hex or Octor formatting
            if (this->octal_mode) {
                // format address and bytes in OCTAL
                line_data_stream << std::oct << std::setfill('0') << std::setw(6) << line_address << "  ";
                for (size_t i = bytes_before; i < bytes_after; ++i) {
                    line_data_stream << std:: setw(3) << static_cast<int>(output[i]) << " ";
                }
            } else {
                // Formatting address and bytes in HEXADECIMAL (default)
                line_data_stream << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << line_address << "  ";
                for (size_t i = bytes_before; i < bytes_after; ++i) {
                    line_data_stream << std::setw(2) << static_cast<int>(output[i]) << " ";
                }
            }

            
            // Write the final formatted line to the listing file
            *listing_stream << std::left << std::setw(20) << line_data_stream.str() << current_line << std::endl;
        }
    }
    if (!if_stack.empty()) report_error("IF block not closed with ENDIF", lines.size());
}

// The recursive heart of the assembler. It expands macros, handles conditional assembly, and sends normal instructions to be parsed.
void Assembler::expand_and_process_line(const std::string& line, int original_lineno) {
    std::string temp_line = line;
    trim(temp_line);
    if(temp_line.empty() || temp_line[0] == ';') return;
    std::stringstream ss(temp_line);
    std::string first_word;
    ss >> first_word;
    std::string lower_first = first_word;
    to_lower(lower_first);

    // Handle conditional assembly directives.
    if (lower_first == "if") {
        bool is_active = !should_skip();
        std::string condition_expr;
        std::getline(ss, condition_expr);
        bool condition_result = is_active ? evaluate_conditional(condition_expr) : false;
        if_stack.push_back(condition_result);
        return;
    }
    if (lower_first == "endif") {
        if (if_stack.empty()) report_error("ENDIF without IF", original_lineno);
        if_stack.pop_back();
        return;
    }
    if (should_skip()) return; // If the program is in a false IF block, ignore this line.
    
    // Ignore directives that don't generate code.
    if (lower_first == "error" || lower_first == "local") return;

    // If the first word is a defined macro, expand it.
    if (macros.count(lower_first)) {
        const auto& macro_def = macros.at(lower_first);
        macro_expansion_counter++;
        std::string args_part;
        std::getline(ss, args_part);
        std::vector<std::string> args = split_args(args_part, ',');
        if (args.size() != macro_def.params.size()) { report_error("macro '" + macro_def.name + "' argument count mismatch", original_lineno); }

        // Find and prepare unique names for local labels.
        std::map<std::string, std::string> local_label_map;
        for (const auto& body_line : macro_def.body_lines) {
            std::string temp_body = body_line;
            trim(temp_body);
            std::string body_first_word;
            std::stringstream(temp_body) >> body_first_word;
            to_lower(body_first_word);
            if (body_first_word == "local") {
                std::string local_args_part;
                std::stringstream(temp_body) >> body_first_word >> local_args_part;
                std::vector<std::string> local_labels = split_args(local_args_part, ',');
                for (const auto& label_name : local_labels) {
                    local_label_map[label_name] = label_name + "_" + std::to_string(macro_expansion_counter);
                }
            }
        }

        // Process each line in the macro's body.
        for (std::string body_line : macro_def.body_lines) {
            // Substitute parameters with arguments.
            for (size_t i = 0; i < macro_def.params.size(); ++i) {
                size_t pos = 0;
                while ((pos = body_line.find(macro_def.params[i], pos)) != std::string::npos) {
                    body_line.replace(pos, macro_def.params[i].length(), args[i]);
                    pos += args[i].length();
                }
            }
            // Substitute local labels with their unique generated names.
            for (const auto& pair : local_label_map) {
                 size_t pos = 0;
                while ((pos = body_line.find(pair.first, pos)) != std::string::npos) {
                    body_line.replace(pos, pair.first.length(), pair.second);
                    pos += pair.second.length();
                }
            }
            // Recursively process the expanded line.
            expand_and_process_line(body_line, original_lineno);
        }
    } else {
        // If it's not a macro or directive, it's a normal instruction.
        this->lineno = original_lineno;
        parse(line);
        process_instruction();
    }
}

// Main parser to break a line into label, mnemonic, and operands.
void Assembler::parse(std::string line) {
    label = mnemonic = operand1 = operand2 = comment = "";
    std::replace(line.begin(), line.end(), '\t', ' ');
    size_t comment_pos = line.find(';');
    if (comment_pos != std::string::npos) { comment = line.substr(comment_pos + 1); line = line.substr(0, comment_pos); trim(comment); }
    trim(line);
    if (line.empty()) return;

    // Special handling for EQU directives without a colon.
    std::string temp_upper = line; to_lower(temp_upper);
    size_t equ_pos = temp_upper.find(" equ ");
    if (equ_pos != std::string::npos) { label = line.substr(0, equ_pos); mnemonic = "equ"; operand1 = line.substr(equ_pos + 5); trim(label); trim(operand1); to_lower(label); return; }

     // Standard parsing for lines with colon-terminated labels.
    size_t label_pos = line.find(':');
    if (label_pos != std::string::npos) { label = line.substr(0, label_pos); line = line.substr(label_pos + 1); trim(label); trim(line); }

    // Use stringstream to get the mnemonic and the rest of the operands.
    std::stringstream ss(line);
    ss >> mnemonic;
    std::string operands_part;
    std::getline(ss, operands_part);
    trim(operands_part);

    // Split operands by the first comma found outside of quotes/brackets.
    bool in_quotes = false; int bracket_level = 0; size_t comma_split_pos = std::string::npos;
    for (size_t i = 0; i < operands_part.length(); ++i) {
        if (operands_part[i] == '\'' || operands_part[i] == '"') in_quotes = !in_quotes;
        if (operands_part[i] == '<') bracket_level++; if (operands_part[i] == '>') bracket_level--;
        if (operands_part[i] == ',' && !in_quotes && bracket_level == 0) { comma_split_pos = i; break; }
    }
    if (comma_split_pos != std::string::npos) { operand1 = operands_part.substr(0, comma_split_pos); operand2 = operands_part.substr(comma_split_pos + 1); trim(operand1); trim(operand2); }
    else { operand1 = operands_part; }
    if (mnemonic.empty() && !operand1.empty()) { mnemonic = operand1; operand1 = ""; }
    to_lower(label); to_lower(mnemonic);
}

// Dispatches a parsed instruction to the correct handler function.
void Assembler::process_instruction() {
    if (mnemonic.empty() && label.empty()) return;
    if (mnemonic_handlers.count(mnemonic)) {
        (this->*mnemonic_handlers[mnemonic])();
    } else if (mnemonic.empty() && !label.empty()) {
        pass_action(0, {});
    } else if (!mnemonic.empty()) {
        report_error("unknown mnemonic \"" + mnemonic + "\"", this->lineno);
    }
}

// Handles the action for each line based on the current pass.
void Assembler::pass_action(int instruction_size, const std::vector<uint8_t>& output_bytes, bool should_add_label) {
    if (source_pass == 1) {
        if (!label.empty() && should_add_label) { add_label(); }
    } else {
        output.insert(output.end(), output_bytes.begin(), output_bytes.end());
    }
    address += instruction_size;
}

// Adds a label and its current address to the symbol table.
void Assembler::add_label() { if (symbol_table.count(label)) { report_error("duplicate label: \"" + label + "\"", this->lineno); } symbol_table[label] = address; cross_reference_data[label].push_back(-(this->lineno + 1));}

// Checks for the correct number of operands and reports an error if invalid.
void Assembler::check_operands(bool valid, const std::string& mnemonic_name) { if (!valid) { report_error("invalid operands for mnemonic \"" + mnemonic_name + "\"", this->lineno); } }

// Converts a string (hex, decimal, etc.) to a number.
int Assembler::get_number(std::string input) const { trim(input); if (input.empty()) return 0; if (input.front() == '-') { return std::stoi(input, nullptr, 10); } char last = tolower(input.back()); int base = 10; if (last == 'h') { base = 16; input.pop_back(); } else if (last == 'q') { base = 8; input.pop_back(); } else if (last == 'b') { base = 2; input.pop_back(); } try { return std::stoul(input, nullptr, base); } catch (const std::exception&) { report_error("invalid number format: " + input, this->lineno); } return 0; }

// *** Expression Evaluation Engine (Recursive Descent Parser) ***
int Assembler::register_offset8(std::string raw_register) { to_lower(raw_register); if (raw_register == "b") return 0; if (raw_register == "c") return 1; if (raw_register == "d") return 2; if (raw_register == "e") return 3; if (raw_register == "h") return 4; if (raw_register == "l") return 5; if (raw_register == "m") return 6; if (raw_register == "a") return 7; report_error("invalid 8-bit register \"" + raw_register + "\"", this->lineno); return -1; }
int Assembler::register_offset16() { std::string op = operand1; to_lower(op); if (op == "b" || op == "bc") return 0x00; if (op == "d" || op == "de") return 0x10; if (op == "h" || op == "hl") return 0x20; if (op == "psw") { if (mnemonic == "push" || mnemonic == "pop") return 0x30; report_error("\"psw\" cannot be used with instruction \"" + mnemonic + "\"", this->lineno); } if (op == "sp") { if (mnemonic != "push" && mnemonic != "pop") return 0x30; report_error("\"sp\" cannot be used with instruction \"" + mnemonic + "\"", this->lineno); } report_error("invalid 16-bit register \"" + operand1 + "\" for instruction \"" + mnemonic + "\"", this->lineno); return -1; }
void Assembler::immediate_operand(ImmediateType operand_type) { if (source_pass != 2) return; std::string operand = (mnemonic == "lxi" || mnemonic == "mvi") ? operand2 : operand1; int number = evaluate_expression(operand); if (operand_type == IMMEDIATE8) { output.push_back(number & 0xFF); } else { output.push_back(number & 0xFF); output.push_back((number >> 8) & 0xFF); } }
void Assembler::address16(const std::string& operand) { if (source_pass != 2) return; uint16_t number = evaluate_expression(operand); output.push_back(number & 0xFF); output.push_back((number >> 8) & 0xFF); }
bool Assembler::should_skip() const { for (bool condition : if_stack) { if (!condition) return true; } return false; }
bool Assembler::evaluate_conditional(const std::string& expr) { const std::vector<std::pair<std::string, std::string>> ops = { {"ne", "!="}, {"eq", "="}, {"ge", ">="}, {"le", "<="}, {"gt", ">"}, {"lt", "<"} }; std::string op_str; size_t op_pos = std::string::npos; for (const auto& op_pair : ops) { if ((op_pos = expr.find(op_pair.first)) != std::string::npos) { op_str = op_pair.first; break; } if ((op_pos = expr.find(op_pair.second)) != std::string::npos) { op_str = op_pair.second; break; } } if (op_pos != std::string::npos) { std::string lhs_str = expr.substr(0, op_pos); std::string rhs_str = expr.substr(op_pos + op_str.length()); int lhs_val = evaluate_expression(lhs_str); int rhs_val = evaluate_expression(rhs_str); if (op_str == "eq" || op_str == "=") return lhs_val == rhs_val; if (op_str == "ne" || op_str == "!=") return lhs_val != rhs_val; if (op_str == "gt" || op_str == ">") return lhs_val > rhs_val; if (op_str == "lt" || op_str == "<") return lhs_val < rhs_val; if (op_str == "ge" || op_str == ">=") return lhs_val >= rhs_val; if (op_str == "le" || op_str == "<=") return lhs_val <= rhs_val; } else { return evaluate_expression(expr) != 0; } return false; }

// --- Expression Evaluation Engine ---
std::string Assembler::get_token(std::string::const_iterator& it, std::string::const_iterator end) { while (it != end && isspace(*it)) ++it; if (it == end) return ""; std::string token; if (isalpha(*it) || *it == '$' || *it == '_') { while (it != end && (isalnum(*it) || *it == '$' || *it == '_')) token += *it++; } else if (isdigit(*it) || (*it == '-' && (it + 1 != end && isdigit(*(it+1))))) { token += *it++; while (it != end && isalnum(*it)) token += *it++; } else { token += *it++; } return token; }
int Assembler::parse_expr_factor(std::string::const_iterator& it, std::string::const_iterator end) { std::string token = get_token(it, end); if (token == "(") { int result = evaluate_expression(it, end); std::string closing_paren = get_token(it, end); if(closing_paren != ")") report_error("mismatched parentheses in expression", lineno); return result; } else { return evaluate_single_term(token); } }
int Assembler::parse_expr_term(std::string::const_iterator& it, std::string::const_iterator end) { int result = parse_expr_factor(it, end); while (true) { auto current_pos = it; std::string op = get_token(it, end); to_lower(op); if (op != "*" && op != "/" && op != "and") { it = current_pos; break; } int rhs = parse_expr_factor(it, end); if (op == "*") result *= rhs; else if (op == "/") result /= rhs; else if (op == "and") result &= rhs; } return result; }
int Assembler::evaluate_expression(std::string::const_iterator& it, std::string::const_iterator end) { int result = parse_expr_term(it, end); while (true) { auto current_pos = it; std::string op = get_token(it, end); to_lower(op); if (op != "+" && op != "-" && op != "or" && op != "xor") { it = current_pos; break; } int rhs = parse_expr_term(it, end); if (op == "+") result += rhs; else if (op == "-") result -= rhs; else if (op == "or") result |= rhs; else if (op == "xor") result ^= rhs; } return result; }
int Assembler::evaluate_expression(const std::string& expr) { auto it = expr.begin(); auto end = expr.end(); return evaluate_expression(it, end); }
int Assembler::evaluate_single_term(const std::string& term_str) { std::string term = term_str; trim(term); if (term.empty()) return 0; if (is_char_constant(term)) { return static_cast<uint8_t>(term[1]); } to_lower(term); if (term == "$") return this->address; if (term.rfind("low ", 0) == 0) { std::string label = term.substr(4); trim(label); if (symbol_table.count(label)) return symbol_table.at(label) & 0xFF; if (source_pass == 2) report_error("undefined label in LOW operator: " + label, this->lineno); return 0; } if (term.rfind("high ", 0) == 0) { std::string label = term.substr(5); trim(label); if (symbol_table.count(label)) return (symbol_table.at(label) >> 8) & 0xFF; if (source_pass == 2) report_error("undefined label in HIGH operator: " + label, this->lineno); return 0; } if (isdigit(term[0]) || (term.length() > 1 && term[0] == '-')) { return get_number(term); } if (symbol_table.count(term)) { cross_reference_data[term].push_back(this->lineno + 1); return symbol_table.at(term); } if (source_pass == 2) { report_error("undefined label in expression: " + term, this->lineno); } return 0; }
bool Assembler::is_quote_delimited(const std::string& s) const { if (s.length() < 2) return false; char first = s.front(); char last = s.back(); return (first == '"' && last == '"') || (first == '\'' && last == '\''); }
bool Assembler::is_char_constant(const std::string& s) const { return s.length() == 3 && s.front() == '\'' && s.back() == '\''; }

// --- Instruction & Directive Handlers ---
void Assembler::nop() { check_operands(operand1.empty() && operand2.empty(), "nop"); pass_action(1, {0x00}); }
void Assembler::lxi() { check_operands(!operand1.empty() && !operand2.empty(), "lxi"); uint8_t opcode = 0x01 + register_offset16(); pass_action(3, {opcode}); immediate_operand(IMMEDIATE16); }
void Assembler::stax() { check_operands(!operand1.empty() && operand2.empty(), "stax"); std::string op = operand1; to_lower(op); if (op == "b") pass_action(1, {0x02}); else if (op == "d") pass_action(1, {0x12}); else report_error("\"stax\" only takes \"b\" or \"d\"", this->lineno); }
void Assembler::inx() { check_operands(!operand1.empty() && operand2.empty(), "inx"); uint8_t opcode = 0x03 + register_offset16(); pass_action(1, {opcode}); }
void Assembler::inr() { check_operands(!operand1.empty() && operand2.empty(), "inr"); uint8_t opcode = 0x04 + (register_offset8(operand1) << 3); pass_action(1, {opcode}); }
void Assembler::dcr() { check_operands(!operand1.empty() && operand2.empty(), "dcr"); uint8_t opcode = 0x05 + (register_offset8(operand1) << 3); pass_action(1, {opcode}); }
void Assembler::mvi() { check_operands(!operand1.empty() && !operand2.empty(), "mvi"); uint8_t opcode = 0x06 + (register_offset8(operand1) << 3); pass_action(2, {opcode}); immediate_operand(IMMEDIATE8); }
void Assembler::rlc() { check_operands(operand1.empty() && operand2.empty(), "rlc"); pass_action(1, {0x07}); }
void Assembler::dad() { check_operands(!operand1.empty() && operand2.empty(), "dad"); uint8_t opcode = 0x09 + register_offset16(); pass_action(1, {opcode}); }
void Assembler::ldax() { check_operands(!operand1.empty() && operand2.empty(), "ldax"); std::string op = operand1; to_lower(op); if (op == "b") pass_action(1, {0x0A}); else if (op == "d") pass_action(1, {0x1A}); else report_error("\"ldax\" only takes \"b\" or \"d\"", this->lineno); }
void Assembler::dcx() { check_operands(!operand1.empty() && operand2.empty(), "dcx"); uint8_t opcode = 0x0B + register_offset16(); pass_action(1, {opcode}); }
void Assembler::rrc() { check_operands(operand1.empty() && operand2.empty(), "rrc"); pass_action(1, {0x0F}); }
void Assembler::ral() { check_operands(operand1.empty() && operand2.empty(), "ral"); pass_action(1, {0x17}); }
void Assembler::rar() { check_operands(operand1.empty() && operand2.empty(), "rar"); pass_action(1, {0x1F}); }
void Assembler::shld() { check_operands(!operand1.empty() && operand2.empty(), "shld"); pass_action(3, {0x22}); address16(operand1); }
void Assembler::daa() { check_operands(operand1.empty() && operand2.empty(), "daa"); pass_action(1, {0x27}); }
void Assembler::lhld() { check_operands(!operand1.empty() && operand2.empty(), "lhld"); pass_action(3, {0x2A}); address16(operand1); }
void Assembler::cma() { check_operands(operand1.empty() && operand2.empty(), "cma"); pass_action(1, {0x2F}); }
void Assembler::sta() { check_operands(!operand1.empty() && operand2.empty(), "sta"); pass_action(3, {0x32}); address16(operand1); }
void Assembler::stc() { check_operands(operand1.empty() && operand2.empty(), "stc"); pass_action(1, {0x37}); }
void Assembler::lda() { check_operands(!operand1.empty() && operand2.empty(), "lda"); pass_action(3, {0x3A}); address16(operand1); }
void Assembler::cmc() { check_operands(operand1.empty() && operand2.empty(), "cmc"); pass_action(1, {0x3F}); }
void Assembler::mov() { check_operands(!operand1.empty() && !operand2.empty(), "mov"); uint8_t opcode = 0x40 + (register_offset8(operand1) << 3) + register_offset8(operand2); pass_action(1, {opcode}); }
void Assembler::hlt() { check_operands(operand1.empty() && operand2.empty(), "hlt"); pass_action(1, {0x76}); }
void Assembler::add() { check_operands(!operand1.empty() && operand2.empty(), "add"); uint8_t opcode = 0x80 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::adc() { check_operands(!operand1.empty() && operand2.empty(), "adc"); uint8_t opcode = 0x88 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::sub() { check_operands(!operand1.empty() && operand2.empty(), "sub"); uint8_t opcode = 0x90 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::sbb() { check_operands(!operand1.empty() && operand2.empty(), "sbb"); uint8_t opcode = 0x98 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::ana() { check_operands(!operand1.empty() && operand2.empty(), "ana"); uint8_t opcode = 0xA0 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::xra() { check_operands(!operand1.empty() && operand2.empty(), "xra"); uint8_t opcode = 0xA8 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::ora() { check_operands(!operand1.empty() && operand2.empty(), "ora"); uint8_t opcode = 0xB0 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::cmp() { check_operands(!operand1.empty() && operand2.empty(), "cmp"); uint8_t opcode = 0xB8 + register_offset8(operand1); pass_action(1, {opcode}); }
void Assembler::rnz() { check_operands(operand1.empty() && operand2.empty(), "rnz"); pass_action(1, {0xC0}); }
void Assembler::pop() { check_operands(!operand1.empty() && operand2.empty(), "pop"); uint8_t opcode = 0xC1 + register_offset16(); pass_action(1, {opcode}); }
void Assembler::jnz() { check_operands(!operand1.empty() && operand2.empty(), "jnz"); pass_action(3, {0xC2}); address16(operand1); }
void Assembler::jmp() { check_operands(!operand1.empty() && operand2.empty(), "jmp"); pass_action(3, {0xC3}); address16(operand1); }
void Assembler::cnz() { check_operands(!operand1.empty() && operand2.empty(), "cnz"); pass_action(3, {0xC4}); address16(operand1); }
void Assembler::push() { check_operands(!operand1.empty() && operand2.empty(), "push"); uint8_t opcode = 0xC5 + register_offset16(); pass_action(1, {opcode}); }
void Assembler::adi() { check_operands(!operand1.empty() && operand2.empty(), "adi"); pass_action(2, {0xC6}); immediate_operand(); }
void Assembler::rst() { check_operands(!operand1.empty() && operand2.empty(), "rst"); int offset = get_number(operand1); if (offset >= 0 && offset <= 7) { uint8_t opcode = 0xC7 + (offset << 3); pass_action(1, {opcode}); } else { report_error("invalid restart vector", this->lineno); } }
void Assembler::rz() { check_operands(operand1.empty() && operand2.empty(), "rz"); pass_action(1, {0xC8}); }
void Assembler::ret() { check_operands(operand1.empty() && operand2.empty(), "ret"); pass_action(1, {0xC9}); }
void Assembler::jz() { check_operands(!operand1.empty() && operand2.empty(), "jz"); pass_action(3, {0xCA}); address16(operand1); }
void Assembler::cz() { check_operands(!operand1.empty() && operand2.empty(), "cz"); pass_action(3, {0xCC}); address16(operand1); }
void Assembler::call() { check_operands(!operand1.empty() && operand2.empty(), "call"); pass_action(3, {0xCD}); address16(operand1); }
void Assembler::aci() { check_operands(!operand1.empty() && operand2.empty(), "aci"); pass_action(2, {0xCE}); immediate_operand(); }
void Assembler::rnc() { check_operands(operand1.empty() && operand2.empty(), "rnc"); pass_action(1, {0xD0}); }
void Assembler::jnc() { check_operands(!operand1.empty() && operand2.empty(), "jnc"); pass_action(3, {0xD2}); address16(operand1); }
void Assembler::i80_out() { check_operands(!operand1.empty() && operand2.empty(), "out"); pass_action(2, {0xD3}); immediate_operand(); }
void Assembler::cnc() { check_operands(!operand1.empty() && operand2.empty(), "cnc"); pass_action(3, {0xD4}); address16(operand1); }
void Assembler::sui() { check_operands(!operand1.empty() && operand2.empty(), "sui"); pass_action(2, {0xD6}); immediate_operand(); }
void Assembler::rc() { check_operands(operand1.empty() && operand2.empty(), "rc"); pass_action(1, {0xD8}); }
void Assembler::jc() { check_operands(!operand1.empty() && operand2.empty(), "jc"); pass_action(3, {0xDA}); address16(operand1); }
void Assembler::i80_in() { check_operands(!operand1.empty() && operand2.empty(), "in"); pass_action(2, {0xDB}); immediate_operand(); }
void Assembler::cc() { check_operands(!operand1.empty() && operand2.empty(), "cc"); pass_action(3, {0xDC}); address16(operand1); }
void Assembler::sbi() { check_operands(!operand1.empty() && operand2.empty(), "sbi"); pass_action(2, {0xDE}); immediate_operand(); }
void Assembler::rpo() { check_operands(operand1.empty() && operand2.empty(), "rpo"); pass_action(1, {0xE0}); }
void Assembler::jpo() { check_operands(!operand1.empty() && operand2.empty(), "jpo"); pass_action(3, {0xE2}); address16(operand1); }
void Assembler::xthl() { check_operands(operand1.empty() && operand2.empty(), "xthl"); pass_action(1, {0xE3}); }
void Assembler::cpo() { check_operands(!operand1.empty() && operand2.empty(), "cpo"); pass_action(3, {0xE4}); address16(operand1); }
void Assembler::ani() { check_operands(!operand1.empty() && operand2.empty(), "ani"); pass_action(2, {0xE6}); immediate_operand(); }
void Assembler::rpe() { check_operands(operand1.empty() && operand2.empty(), "rpe"); pass_action(1, {0xE8}); }
void Assembler::pchl() { check_operands(operand1.empty() && operand2.empty(), "pchl"); pass_action(1, {0xE9}); }
void Assembler::jpe() { check_operands(!operand1.empty() && operand2.empty(), "jpe"); pass_action(3, {0xEA}); address16(operand1); }
void Assembler::xchg() { check_operands(operand1.empty() && operand2.empty(), "xchg"); pass_action(1, {0xEB}); }
void Assembler::cpe() { check_operands(!operand1.empty() && operand2.empty(), "cpe"); pass_action(3, {0xEC}); address16(operand1); }
void Assembler::xri() { check_operands(!operand1.empty() && operand2.empty(), "xri"); pass_action(2, {0xEE}); immediate_operand(); }
void Assembler::rp() { check_operands(operand1.empty() && operand2.empty(), "rp"); pass_action(1, {0xF0}); }
void Assembler::jp() { check_operands(!operand1.empty() && operand2.empty(), "jp"); pass_action(3, {0xF2}); address16(operand1); }
void Assembler::di() { check_operands(operand1.empty() && operand2.empty(), "di"); pass_action(1, {0xF3}); }
void Assembler::cp() { check_operands(!operand1.empty() && operand2.empty(), "cp"); pass_action(3, {0xF4}); address16(operand1); }
void Assembler::ori() { check_operands(!operand1.empty() && operand2.empty(), "ori"); pass_action(2, {0xF6}); immediate_operand(); }
void Assembler::rm() { check_operands(operand1.empty() && operand2.empty(), "rm"); pass_action(1, {0xF8}); }
void Assembler::sphl() { check_operands(operand1.empty() && operand2.empty(), "sphl"); pass_action(1, {0xF9}); }
void Assembler::jm() { check_operands(!operand1.empty() && operand2.empty(), "jm"); pass_action(3, {0xFA}); address16(operand1); }
void Assembler::ei() { check_operands(operand1.empty() && operand2.empty(), "ei"); pass_action(1, {0xFB}); }
void Assembler::cm() { check_operands(!operand1.empty() && operand2.empty(), "cm"); pass_action(3, {0xFC}); address16(operand1); }
void Assembler::cpi() { check_operands(!operand1.empty() && operand2.empty(), "cpi"); pass_action(2, {0xFE}); immediate_operand(); }
void Assembler::db() { std::string all_operands = operand1; if (!operand2.empty()) { all_operands += "," + operand2; } check_operands(!all_operands.empty(), "db"); bool should_add_label_flag = true; std::vector<std::string> arguments = split_args(all_operands, ','); for (const auto& arg : arguments) { std::string temp_arg = arg; trim(temp_arg); if (temp_arg.length() > 2 && temp_arg.front() == '<' && temp_arg.back() == '>') { std::string inner_content = temp_arg.substr(1, temp_arg.length() - 2); std::vector<std::string> byte_args = split_args(inner_content, ','); for (const auto& byte_str : byte_args) { pass_action(1, {}, !label.empty() && should_add_label_flag); if (source_pass == 2) { uint8_t val = evaluate_expression(byte_str) & 0xFF; output.push_back(val); } should_add_label_flag = false; } } else if (is_quote_delimited(temp_arg)) { std::string str = temp_arg.substr(1, temp_arg.length() - 2); pass_action(str.length(), {}, !label.empty() && should_add_label_flag); if (source_pass == 2) { for(char c : str) output.push_back(c); } } else if (is_char_constant(temp_arg)) { pass_action(1, {}, !label.empty() && should_add_label_flag); if (source_pass == 2) { output.push_back(static_cast<uint8_t>(temp_arg[1])); } } else { pass_action(1, {}, !label.empty() && should_add_label_flag); if (source_pass == 2) { uint8_t val = evaluate_expression(temp_arg) & 0xFF; output.push_back(val); } } should_add_label_flag = false; } }
void Assembler::dw() { std::string all_operands = operand1; if (!operand2.empty()) { all_operands += "," + operand2; } check_operands(!all_operands.empty(), "dw"); std::vector<std::string> arguments = split_args(all_operands, ','); for (const auto& arg : arguments) { std::string temp_arg = arg; trim(temp_arg); pass_action(2, {}); if (source_pass == 2) { address16(temp_arg); } } }
void Assembler::ds() { check_operands(!operand1.empty(), "ds"); int size = evaluate_expression(operand1); if (size < 0) { report_error("DS size cannot be negative", this->lineno); } uint8_t fill_value = 0; if (!operand2.empty()) { fill_value = evaluate_expression(operand2); } if (source_pass == 2) { output.insert(output.end(), size, fill_value); } pass_action(size, {}); }
void Assembler::end() { check_operands(label.empty() && operand1.empty() && operand2.empty(), "end"); assembly_finished = true; }
void Assembler::equ() { if (label.empty()) { report_error("missing 'equ' label", this->lineno); } check_operands(!operand1.empty() && operand2.empty(), "equ"); uint16_t value = evaluate_expression(operand1); if (source_pass == 1) { if (symbol_table.count(label)) { report_error("duplicate label: \"" + label + "\"", this->lineno); } symbol_table[label] = value; } }
void Assembler::org() { check_operands(!operand1.empty() && label.empty() && operand2.empty(), "org"); uint16_t new_address = evaluate_expression(operand1); if (source_pass == 2) { if (new_address > address) { output.insert(output.end(), new_address - address, 0); } } address = new_address; }
void Assembler::name() {} void Assembler::title() {} void Assembler::sim() { check_operands(operand1.empty() && operand2.empty(), "sim"); pass_action(1, {0x30}); } void Assembler::rim() { check_operands(operand1.empty() && operand2.empty(), "rim"); pass_action(1, {0x20}); }

// Initializes the map that connects mnemonic strings to their handler functions.
void Assembler::initialize_mnemonic_handlers() {
    mnemonic_handlers = {
        {"nop", &Assembler::nop}, {"lxi", &Assembler::lxi}, {"stax", &Assembler::stax}, {"inx", &Assembler::inx}, {"inr", &Assembler::inr}, {"dcr", &Assembler::dcr},
        {"mvi", &Assembler::mvi}, {"rlc", &Assembler::rlc}, {"dad", &Assembler::dad}, {"ldax", &Assembler::ldax},{"dcx", &Assembler::dcx}, {"rrc", &Assembler::rrc},
        {"ral", &Assembler::ral}, {"rar", &Assembler::rar}, {"shld", &Assembler::shld}, {"daa", &Assembler::daa}, {"lhld", &Assembler::lhld},{"cma", &Assembler::cma},
        {"sta", &Assembler::sta}, {"stc", &Assembler::stc}, {"lda", &Assembler::lda}, {"cmc", &Assembler::cmc}, {"mov", &Assembler::mov}, {"hlt", &Assembler::hlt},
        {"add", &Assembler::add}, {"adc", &Assembler::adc}, {"sub", &Assembler::sub}, {"sbb", &Assembler::sbb}, {"ana", &Assembler::ana}, {"xra", &Assembler::xra},
        {"ora", &Assembler::ora}, {"cmp", &Assembler::cmp}, {"rnz", &Assembler::rnz}, {"pop", &Assembler::pop}, {"jnz", &Assembler::jnz}, {"jmp", &Assembler::jmp},
        {"cnz", &Assembler::cnz}, {"push", &Assembler::push},{"adi", &Assembler::adi}, {"rst", &Assembler::rst}, {"rz", &Assembler::rz},   {"ret", &Assembler::ret},
        {"jz", &Assembler::jz},   {"cz", &Assembler::cz},   {"call", &Assembler::call}, {"aci", &Assembler::aci}, {"rnc", &Assembler::rnc}, {"jnc", &Assembler::jnc},
        {"out", &Assembler::i80_out},{"cnc", &Assembler::cnc}, {"sui", &Assembler::sui}, {"rc", &Assembler::rc},   {"jc", &Assembler::jc},   {"in", &Assembler::i80_in},
        {"cc", &Assembler::cc},   {"sbi", &Assembler::sbi}, {"jpe", &Assembler::jpe}, {"rpo", &Assembler::rpo}, {"jpo", &Assembler::jpo}, {"xthl", &Assembler::xthl},
        {"cpo", &Assembler::cpo}, {"ani", &Assembler::ani}, {"rpe", &Assembler::rpe}, {"pchl", &Assembler::pchl},{"xchg", &Assembler::xchg},{"cpe", &Assembler::cpe},
        {"xri", &Assembler::xri}, {"rp", &Assembler::rp},   {"jp", &Assembler::jp}, {"di", &Assembler::di},   {"cp", &Assembler::cp},   {"ori", &Assembler::ori},
        {"rm", &Assembler::rm},   {"sphl", &Assembler::sphl},{"jm", &Assembler::jm}, {"ei", &Assembler::ei},   {"cm", &Assembler::cm},   {"cpi", &Assembler::cpi},
        {"db", &Assembler::db},   {"ds", &Assembler::ds},   {"dw", &Assembler::dw}, {"end", &Assembler::end}, {"equ", &Assembler::equ}, {"name", &Assembler::name},
        {"org", &Assembler::org}, {"title", &Assembler::title}, {"sim", &Assembler::sim}, {"rim", &Assembler::rim}
    };
}