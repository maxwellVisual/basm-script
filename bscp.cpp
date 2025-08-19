#include "bscp.hpp"

#include <argp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <FlexLexer.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <istream>
#include <stack>

#include "lex.hpp"

bscp_value* output;
extern yyFlexLexer* lexer;
std::stack<const char*> file_stack;

const char *argp_program_version = "bscp 1.0";
const char *argp_program_bug_address = "<bug@example.com>";
static char doc[] = "bscp - A simple expression language interpreter";
static char args_doc[] = "[FILE]";

static struct argp_option options[] = {
    {"debug", 'd', 0, 0, "Enable debug output"},
    {0}
};

struct arguments {
    char *file;
    int debug;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = (struct arguments *)state->input;

    switch (key) {
    case 'd':
        arguments->debug = 1;
        break;
    case ARGP_KEY_ARG:
        if (state->arg_num >= 1)
            argp_usage(state);
        arguments->file = arg;
        break;
    case ARGP_KEY_END:
        if (state->arg_num < 0)
            argp_usage(state);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};
static bscp_value* bscp_calc(bscp_value* val, std::stack<std::map<std::string, bscp_value*>> &stack);

int execute_file(const char *filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        perror("Error opening file");
        return 1;
    }


    if(lexer != nullptr){
        delete lexer;
    }
    file_stack.push(filename);
    lexer = new yyFlexLexer(&file);
    
    int result = yy::parser()();
    std::stack<std::map<std::string, bscp_value*>> stack;
    (void)bscp_calc(output, stack);
    delete output;
    output = nullptr;
    file_stack.pop();

    file.close();
    delete lexer;
    lexer = nullptr;

    return result;
}

int repl_mode() {
    printf("bscp REPL (press Ctrl+D to exit)\n");

    char line[1024];
    if(lexer != nullptr){
        delete lexer;
    }
    while (printf("> ") && fgets(line, sizeof(line), stdin)) {
        if (strlen(line) == 1 && line[0] == '\n') continue;
        
        std::istringstream yyin(line, std::ios::in);

        lexer = new yyFlexLexer(&yyin);

        // yyin = fmemopen(line, strlen(line), "r");
        yy::parser parser;
        parser.parse();
        int result = yy::parser()();
        
        if (result != 0) {
            fprintf(stderr, "Error parsing input\n");
        }
        delete lexer;
    }
    lexer = nullptr;
    
    printf("\n");
    return 0;
}
static int bscp_eval(bscp_value* value);

bool value2bool(bscp_value* value) {
    if (dynamic_cast<bscp_num*>(value)) {
        return static_cast<bscp_num*>(value)->value != 0;
    }
    return !dynamic_cast<bscp_null*>(value);
}
static bscp_value* bscp_calc(bscp_value* val, std::stack<std::map<std::string, bscp_value*>> &stack) {
    bscp_oper* oper = dynamic_cast<bscp_oper*>(val);
    if(!oper){
        return val;
    }
    switch (oper->op) {
        case bscp_oper::OP_PLUS:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value + static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_MINUS:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value - static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_MULTIPLY:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value * static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_DIVIDE:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value / static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_MODULO:
            return new bscp_num(oper->parent, modfl(static_cast<bscp_num*>(oper->operands[0])->value, &static_cast<bscp_num*>(oper->operands[1])->value));
        case bscp_oper::OP_SHIFT_LEFT:
            return new bscp_num(oper->parent, static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) << static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value));
        case bscp_oper::OP_SHIFT_RIGHT:
            return new bscp_num(oper->parent, static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) >> static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value));
        case bscp_oper::OP_BITWISE_AND:
            return new bscp_num(oper->parent, static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) & static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value));
        case bscp_oper::OP_BITWISE_OR:
            return new bscp_num(oper->parent, static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) | static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value));
        case bscp_oper::OP_BITWISE_XOR:
            return new bscp_num(oper->parent, static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) ^ static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value));
        case bscp_oper::OP_LOGICAL_AND:
            return new bscp_num(oper->parent, value2bool(oper->operands[0]) && value2bool(oper->operands[1]));
        case bscp_oper::OP_LOGICAL_OR:
            return new bscp_num(oper->parent, value2bool(oper->operands[0]) || value2bool(oper->operands[1]));
        case bscp_oper::OP_LESS_THAN:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value < static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_LESS_THAN_OR_EQUAL:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value <= static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_GREATER_THAN:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value > static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_GREATER_THAN_OR_EQUAL:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value >= static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_EQUAL_TO:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value == static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_NOT_EQUAL_TO:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value != static_cast<bscp_num*>(oper->operands[1])->value);
        case bscp_oper::OP_UNARY_PLUS:
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value);
        case bscp_oper::OP_UNARY_MINUS:
            return new bscp_num(oper->parent, -static_cast<bscp_num*>(oper->operands[0])->value);
        case bscp_oper::OP_BITWISE_NOT:
            return new bscp_num(oper->parent, ~static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value));
        case bscp_oper::OP_INCREMENT:
            static_cast<bscp_num*>(oper->operands[0])->value += 1;
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value);
        case bscp_oper::OP_DECREMENT:
            static_cast<bscp_num*>(oper->operands[0])->value -= 1;
            return new bscp_num(oper->parent, static_cast<bscp_num*>(oper->operands[0])->value);
        case bscp_oper::OP_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = static_cast<bscp_num*>(oper->operands[1])->value;
            }
            return oper->operands[0];
        case bscp_oper::OP_ADD_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value += static_cast<bscp_num*>(oper->operands[1])->value;
            }
            return oper->operands[0];
        case bscp_oper::OP_SUBTRACT_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value -= static_cast<bscp_num*>(oper->operands[1])->value;
            }
            return oper->operands[0];
        case bscp_oper::OP_MULTIPLY_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value *= static_cast<bscp_num*>(oper->operands[1])->value;
            }
            return oper->operands[0];
        case bscp_oper::OP_DIVIDE_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value /= static_cast<bscp_num*>(oper->operands[1])->value;
            }
            return oper->operands[0];
        case bscp_oper::OP_MODULO_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = fmod(static_cast<bscp_num*>(oper->operands[0])->value, static_cast<bscp_num*>(oper->operands[1])->value);
            }
            return oper->operands[0];
        case bscp_oper::OP_SHIFT_LEFT_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) << static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value);
            }
            return oper->operands[0];
        case bscp_oper::OP_SHIFT_RIGHT_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) >> static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value);
            }
            return oper->operands[0];
        case bscp_oper::OP_BITWISE_AND_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) & static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value);
            }
            return oper->operands[0];
        case bscp_oper::OP_BITWISE_OR_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) | static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value);
            }
            return oper->operands[0];
        case bscp_oper::OP_BITWISE_XOR_ASSIGN:
            if (dynamic_cast<bscp_num*>(oper->operands[0])) {
                static_cast<bscp_num*>(oper->operands[0])->value = static_cast<long long>(static_cast<bscp_num*>(oper->operands[0])->value) ^ static_cast<long long>(static_cast<bscp_num*>(oper->operands[1])->value);
            }
            return oper->operands[0];
        case bscp_oper::OP_TERNARY_CONDITIONAL:
            return value2bool(oper->operands[0]) ? bscp_calc(static_cast<bscp_oper*>(oper->operands[1]), stack) : bscp_calc(static_cast<bscp_oper*>(oper->operands[2]), stack);
        case bscp_oper::OP_CALL_OBJ:
            if(oper->operands.size() <= 0){
                std::cerr<<"invalid object call\n";
                return new bscp_null(oper->parent);
            }
            if(oper->operands[0]->name == "debug"){
                bscp_num* ans = new bscp_num(nullptr, bscp_eval(oper->operands[1]));
                return ans;
            }else if(oper->operands[0]->name == "print"){
                bscp_obj* msg = dynamic_cast<bscp_obj*>(oper->operands[1]);
                if(!msg){
                    return new bscp_num(nullptr, 0);
                }
                for (size_t i = 0; msg->static_fields.find(std::to_string(i)) != msg->static_fields.end(); i++)
                {
                    bscp_num* num = dynamic_cast<bscp_num*>((*msg)[i].value);
                    if(!num || num->value < 0){
                        continue;
                    }
                    putwchar((wchar_t)num->value);
                }
                return new bscp_null(nullptr);
            }else{
                std::map<std::string, bscp_value*>& stack_frame = stack.emplace();
                for(auto i=oper->operands.begin()+1; i != oper->operands.end(); i++){
                    stack_frame[(*i)->name] = *i;
                }
                bscp_value* ans = bscp_calc(oper->operands[0], stack);
                stack.pop();
                return ans;
            }
        default:
            fprintf(stderr, "Unknown operation\n");
            return new bscp_null(oper->parent);
    }
}

static int bscp_eval(bscp_value* value){
    if(dynamic_cast<bscp_num*>(value)){
        printf("%Lf", static_cast<bscp_num*>(value)->value);
        return 0;
    }
    if(dynamic_cast<bscp_oper*>(value)){
        bscp_oper* oper = static_cast<bscp_oper*>(value);
        printf("OPER(%d)[", oper->op);
        for(auto arg : oper->operands){
            bscp_eval(arg);
            printf(",");
        }
        printf("]");
        return 0;
    }
    if(!dynamic_cast<bscp_obj*>(value)){
        printf("(null)");
        return 0;
    }
    printf("{");
    bscp_obj* obj = static_cast<bscp_obj*>(value);
    for(auto [name, field]: obj->static_fields){
        if(!field.is_tmp){
            printf("%s:", name.c_str());
            bscp_eval(field.value);
            printf(",");
        }
    }
    printf("}");
    return 0;
}

__attribute__((weak)) 
int main(int argc, char *argv[]) {
    struct arguments arguments;
    arguments.file = NULL;
    arguments.debug = 0;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    if (arguments.file) {
        return execute_file(arguments.file);
    } else {
        return repl_mode();
    }
}
