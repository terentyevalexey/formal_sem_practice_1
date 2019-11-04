#include <stack>
#include <string>

typedef struct {
    const char* name;
    void* pointer;
} symbol_t;

enum { NOTHING = 0, CONSTANT = 1, EXTERNAL = 2 };

void enter_arm(void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    int_buffer[offset++] = 0xe52de004u; // push {lr}
    int_buffer[offset++] = 0xe52d4004u; // push {r4}
}

void exit_arm(void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    int_buffer[offset++] = 0xe49d0004u; // pop {r0}
    int_buffer[offset++] = 0xe49d4004u; // pop {r4}
    int_buffer[offset++] = 0xe49de004u; // pop {lr}
    int_buffer[offset++] = 0xe12fff1eu; // bx lr
}

void plus(void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    int_buffer[offset++] = 0xe49d1004u; // pop {r1}
    int_buffer[offset++] = 0xe49d0004u; // pop {r0}
    int_buffer[offset++] = 0xe0800001u; // add r0, r0, r1
    int_buffer[offset++] = 0xe52d0004u; // push {r0}
}

void minus(void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    int_buffer[offset++] = 0xe49d1004u; // pop {r1}
    int_buffer[offset++] = 0xe49d0004u; // pop {r0}
    int_buffer[offset++] = 0xe0400001u; // sub r0, r0, r1
    int_buffer[offset++] = 0xe52d0004u; // push {r0}
}

void multiply(void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    int_buffer[offset++] = 0xe49d1004u; // pop {r1}
    int_buffer[offset++] = 0xe49d0004u; // pop {r0}
    int_buffer[offset++] = 0xe0000091u; // mul r0, r1, r0
    int_buffer[offset++] = 0xe52d0004u; // push {r0}
}

void func(void* func_ptr, int argc, void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    if (argc == 4)
        int_buffer[offset++] = 0xe49d3004u; // pop {r3}
    if (argc >= 3)
        int_buffer[offset++] = 0xe49d2004u; // pop {r2}
    if (argc >= 2)
        int_buffer[offset++] = 0xe49d1004u; // pop {r1}
    if (argc >= 1)
        int_buffer[offset++] = 0xe49d0004u; // pop {r0}

    unsigned* ptr =
        int_buffer + offset + 5; // 5 because we wrote 5 instructions to buffer
    unsigned nxt_instruction =
        *reinterpret_cast<unsigned*>(&ptr); // nxt_instruction pointer
    unsigned right_16 = nxt_instruction & 0xFFFFu;
    unsigned mask = (right_16 & 0xFFFu) | ((right_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe300e000u | mask; // movw lr, nxt_instruction::lower_16
    unsigned left_16 = nxt_instruction >> 16u;
    mask = (left_16 & 0xFFFu) | ((left_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe340e000u | mask; // movt lr, nxt_instruction::upper_16

    unsigned func_pointer = *reinterpret_cast<unsigned*>(&func_ptr);
    right_16 = func_pointer & 0xFFFFu;
    mask = (right_16 & 0xFFFu) | ((right_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe3004000u | mask; // movw r4, func_pointer::lower_16
    left_16 = func_pointer >> 16u;
    mask = (left_16 & 0xFFFu) | ((left_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe3404000u | mask; // movt r4, func_pointer::upper_16

    int_buffer[offset++] = 0xe12fff14u; // bx r4
    int_buffer[offset++] = 0xe52d0004u; // push {r0}
}

void constant(const std::string& str, void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    int int_constant = std::stoi(str);
    unsigned unsigned_constant = *reinterpret_cast<unsigned*>(&int_constant);
    unsigned right_16 = unsigned_constant & 0xFFFFu;
    unsigned mask = (right_16 & 0xFFFu) | ((right_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe3000000u | mask; // movw r0, int_constant::lower_16
    unsigned left_16 = unsigned_constant >> 16u;
    mask = (left_16 & 0xFFFu) | ((left_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe3400000u | mask;             // movt r0, int_constant::upper_16
    int_buffer[offset++] = 0xe52d0004u; // push {r0}
}

void external_int(void* ptr, void* out_buffer, int& offset)
{
    auto int_buffer = reinterpret_cast<unsigned*>(out_buffer);
    unsigned external_ptr = *reinterpret_cast<unsigned*>(&ptr);
    unsigned right_16 = external_ptr & 0xFFFFu;
    unsigned mask = (right_16 & 0xFFFu) | ((right_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe3000000u | mask; // movw r0, external_ptr::lower_16
    unsigned left_16 = external_ptr >> 16u;
    mask = (left_16 & 0xFFFu) | ((left_16 << 4u) & 0xF0000u);
    int_buffer[offset++] =
        0xe3400000u | mask;             // movt r0, external_ptr::upper_16
    int_buffer[offset++] = 0xe5900000u; // ldr r0, [r0]
    int_buffer[offset++] = 0xe52d0004u; // push {r0}
}

bool is_digit(const char cur_char)
{
    return ('0' <= cur_char && cur_char <= '9');
}

bool is_operator(const char cur_char)
{
    return (cur_char == '+' || cur_char == '-' || cur_char == '*');
}

bool is_plus(const char cur_char)
{
    return cur_char == '+';
}

bool is_minus(const char cur_char)
{
    return cur_char == '-';
}

bool is_multiply(const char cur_char)
{
    return cur_char == '*';
}

bool is_open_bracket(const char cur_char)
{
    return cur_char == '(';
}

bool is_close_bracket(const char cur_char)
{
    return cur_char == ')';
}

bool is_comma(const char cur_char)
{
    return cur_char == ',';
}

void pop_all_operators(
    std::stack<std::pair<char, void*>>& sym_stack,
    void* out_buffer,
    int& offset)
{
    while (!sym_stack.empty() && is_operator(sym_stack.top().first)) {
        if (is_minus(sym_stack.top().first))
            minus(out_buffer, offset);
        else if (is_plus(sym_stack.top().first))
            plus(out_buffer, offset);
        else
            multiply(out_buffer, offset);
        sym_stack.pop();
    }
}

void pop_all_multiplies(
    std::stack<std::pair<char, void*>>& sym_stack,
    void* out_buffer,
    int& offset)
{
    while (!sym_stack.empty() && is_multiply(sym_stack.top().first)) {
        multiply(out_buffer, offset);
        sym_stack.pop();
    }
}

extern "C" void jit_compile_expression_to_arm(
    const char* expression,
    const symbol_t* externs,
    void* out_buffer)
{
    char cur_char;
    int cur_token_t = NOTHING;
    std::string cur_token;
    std::stack<std::pair<char, void*>> sym_stack;
    std::stack<int> args_cnt;
    int offset = 0;
    enter_arm(out_buffer, offset);
    int cur_char_num = 0;

    while ((cur_char = expression[cur_char_num++]) != '\0') {
        if (cur_char == ' ') // ignore spaces
            continue;
        if (!is_close_bracket(cur_char) && !args_cnt.empty() &&
            args_cnt.top() == 0) // COUNT FIRST ARG
            ++args_cnt.top();
        if (is_digit(cur_char)) { // EXTERNAL CONTINUATION OR CONSTANT PART
            cur_token.push_back(cur_char);
            if (cur_token_t == NOTHING) // starting from a digit => constant
                cur_token_t = CONSTANT;
        } else if (is_operator(cur_char)) { // OPERATORS
            if ((cur_char_num == 1 ||
                 is_open_bracket(expression[cur_char_num - 2])) &&
                is_digit(expression[cur_char_num])) {
                cur_token_t = CONSTANT;
                cur_token.push_back(cur_char);
            } else {
                if (cur_token_t == CONSTANT) // before operator was constant
                    constant(cur_token, out_buffer, offset);
                else if (cur_token_t == EXTERNAL) { // before oper was ext int
                    symbol_t cur_symbol = *externs;
                    int i = 0;
                    while (cur_symbol.name != cur_token) {
                        cur_symbol = *(++i + externs); // finding name by token
                    }
                    external_int(cur_symbol.pointer, out_buffer, offset);
                }
                cur_token.clear();
                if (is_plus(cur_char) ||
                    is_minus(
                        cur_char)) // minus or plus (least priority operators)
                    pop_all_operators(sym_stack, out_buffer, offset);
                else if (is_multiply(
                             cur_char)) // multiply (most priority operator)
                    pop_all_multiplies(sym_stack, out_buffer, offset);
                sym_stack.push({cur_char, nullptr});
                cur_token_t = NOTHING;
            }
        } else if (is_open_bracket(cur_char)) { // OPEN BRACKET
            if (cur_token_t == EXTERNAL) { // before open bracket was ext func
                symbol_t cur_symbol = *externs;
                int i = 0;
                while (cur_symbol.name != cur_token) // finding name by token
                    cur_symbol = *(++i + externs);
                sym_stack.push({'\0', cur_symbol.pointer});
                args_cnt.push(0);
                cur_token.clear();
            }
            sym_stack.push({cur_char, nullptr});
            cur_token_t = NOTHING;
        } else if (is_close_bracket(cur_char)) { // CLOSE BRACKET
            if (cur_token_t == CONSTANT) // before close bracket was constant
                constant(cur_token, out_buffer, offset);
            else if (cur_token_t == EXTERNAL) { // before close was ext int
                symbol_t cur_symbol = *externs;
                int i = 0;
                while (cur_symbol.name != cur_token) // finding name by token
                    cur_symbol = *(++i + externs);
                external_int(cur_symbol.pointer, out_buffer, offset);
            }
            cur_token.clear();
            pop_all_operators(sym_stack, out_buffer, offset);
            sym_stack.pop();
            if (!sym_stack.empty() && sym_stack.top().second) { // func before
                func(
                    sym_stack.top().second, args_cnt.top(), out_buffer, offset);
                args_cnt.pop();
                sym_stack.pop();
            }
            cur_token_t = NOTHING;
        } else if (is_comma(cur_char)) { // COMMA TO SEPARATE FUNC ARGS
            if (cur_token_t == CONSTANT) // before comma was constant
                constant(cur_token, out_buffer, offset);
            else if (cur_token_t == EXTERNAL) { // before comma was ext int
                symbol_t cur_symbol = *externs;
                int i = 0;
                while (cur_symbol.name != cur_token) // finding name by token
                    cur_symbol = *(++i + externs);
                external_int(cur_symbol.pointer, out_buffer, offset);
            }
            cur_token.clear();
            pop_all_operators(sym_stack, out_buffer, offset);
            ++args_cnt.top();
            cur_token_t = NOTHING;
        } else { // SYMBOL => PART OF AN EXTERNAL (INT OR FUNC)
            cur_token.push_back(cur_char);
            cur_token_t = EXTERNAL;
        }
    }
    if (cur_token_t == CONSTANT) // last was constant
        constant(cur_token, out_buffer, offset);
    else if (cur_token_t == EXTERNAL) { // last was external (int)
        symbol_t cur_symbol = *externs;
        int i = 0;
        while (cur_symbol.name != cur_token) // finding name by token
            cur_symbol = *(++i + externs);
        external_int(cur_symbol.pointer, out_buffer, offset);
    }
    pop_all_operators(sym_stack, out_buffer, offset);
    exit_arm(out_buffer, offset);
}
