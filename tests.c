#include <gtest/gtest.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

typedef struct {
    const char * name;
    void       * pointer;
} symbol_t;

extern void
jit_compile_expression_to_arm(const char * expression,
    const symbol_t * externs,
    void * out_buffer);

// available functions to be used within JIT-compiled code
static int my_div(int a, int b) { return a / b; }
static int my_mod(int a, int b) { return a % b; }
static int my_inc(int a) { return ++a; }
static int my_dec(int a) { return --a; }

enum {
    SYMTABLE_SIZE = 100,  // symtable size in units
    EXPR_SIZE = 100,      // max expression size in chars
    CODE_SIZE = 4096      // code segment size in bytes
};

static symbol_t symbols[SYMTABLE_SIZE + 1];
char expression_to_parse[EXPR_SIZE + 1];

static size_t
init_symbols() {
    memset(symbols, 0, sizeof(symbols));
    static const char * func_names[] = {
            "div", "mod", "inc", "dec"
    };
    size_t offset = 0;

    symbols[offset].name = func_names[offset];
    symbols[offset].pointer = &my_div;
    ++offset;

    symbols[offset].name = func_names[offset];
    symbols[offset].pointer = &my_mod;
    ++offset;

    symbols[offset].name = func_names[offset];
    symbols[offset].pointer = &my_inc;
    ++offset;

    symbols[offset].name = func_names[offset];
    symbols[offset].pointer = &my_dec;
    ++offset;

    return offset;
}

static symbol_t
parse_variable(const char * token) {
    const char * delim = strchr(token, '=');
    if (!delim || delim == token) {
        fprintf(stderr, "Wrong token in input: %s\n", token);
        exit(1);
    }
    char left[128], right[128];
    memset(left, 0, sizeof(left));
    memset(right, 0, sizeof(right));
    strncpy(left, token, delim - token);
    strncpy(right, delim + 1, sizeof(right));
    symbol_t result;
    result.name = calloc(1 + strnlen(left, sizeof(left)), sizeof(char));
    result.pointer = calloc(1, sizeof(int));
    sscanf(left, "%s", result.name); // eliminate whitespaces
    sscanf(right, "%d", result.pointer); // parse int value
    return result;
}

static void
free_symbols(size_t offset) {
    while (NULL != symbols[offset].name) {
        free((char *)(symbols[offset].name));
        free(symbols[offset].pointer);
        offset++;
    }
}

static void *
init_program_code_buffer() {
    void * result = mmap(0,
        CODE_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE,
        0,
        0);
    if (!result) {
        perror("Can't mmap: ");
        exit(2);
    }
    return result;
}

static void
free_program_code_buffer(void * addr) {
    munmap(addr, CODE_SIZE);
}

class TestCompile : public ::testing::Test {
protected:
    size_t symbols_count;
    void * code_buffer;

    void SetUp() {
        symbols_count = init_symbols();
        code_buffer = init_program_code_buffer();
    }

    void TearDown() {
        free_symbols(symbols_count);
        free_program_code_buffer(code_buffer);
    }
};

typedef int(*jited_function_t)();

TEST_F(TestCompile, bracers) {
    jit_compile_expression_to_arm("(3)",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(3, function());
}

TEST_F(TestCompile, summation) {
    jit_compile_expression_to_arm("3+4",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(7, function());
}

TEST_F(TestCompile, subtraction) {
    jit_compile_expression_to_arm("5-3",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(2, function());
}

TEST_F(TestCompile, multiplication) {
    jit_compile_expression_to_arm("5*5",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(25, function());
}

TEST_F(TestCompile, order_of_operators) {
    jit_compile_expression_to_arm("5+4*3-2+7*2-4*2*1-3",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(18, function());
}

TEST_F(TestCompile, bracers_priority) {
    jit_compile_expression_to_arm("7*3+7*(3+2)*4+2*(7-2)-2",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(169, function());
}

TEST_F(TestCompile, functions) {
    jit_compile_expression_to_arm("div(100, 3) + mod(5, 4) + inc(3) + dec(4)",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(41, function());
}

TEST_F(TestCompile, variables) {
    symbols[symbols_count++] = parse_variable("a=7");
    jit_compile_expression_to_arm("a+3",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(10, function());
}

TEST_F(TestCompile, general_case) {
    symbols[symbols_count++] = parse_variable("a=1");
    symbols[symbols_count++] = parse_variable("b=2");
    symbols[symbols_count++] = parse_variable("c=3");
    jit_compile_expression_to_arm("(1+a)*c + div(2+4,2)",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(9, function());
}

TEST_F(TestCompile, minus_test) {
    jit_compile_expression_to_arm("-3 - 4 + (-3)",
        symbols,
        code_buffer);
    jited_function_t function = code_buffer;
    EXPECT_EQ(-10, function());
}


int main(int argc, char* argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}