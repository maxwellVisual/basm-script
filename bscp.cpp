#include "bscp.hpp"

#include <argp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <FlexLexer.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stack>
#include <codecvt>
#include <csetjmp>

#include "lex.hpp"

std::shared_ptr<bscp::script::value> output;
extern yyFlexLexer* lexer;
std::stack<const char*> file_stack;

namespace bscp::script
{
    class stack_frame: public std::map<std::wstring, std::shared_ptr<value>> {
        public:
        std::jmp_buf return_point;
        std::shared_ptr<value> return_value;
    };
    static std::shared_ptr<value> calc(std::shared_ptr<value>& val, std::shared_ptr<obj> &parent, std::stack<stack_frame> &stack);
    
    static int call_debug(std::shared_ptr<value>& value, size_t indent = 0);

    bool value2bool(std::shared_ptr<value>& value) {
        if (std::dynamic_pointer_cast<num>(value)) {
            return std::static_pointer_cast<num>(value)->value != 0;
        }
        return !std::dynamic_pointer_cast<null>(value);
    }

    static std::shared_ptr<value> calc_list(
        std::vector<std::shared_ptr<value>>& lines, 
        std::shared_ptr<obj> &parent, 
        std::stack<stack_frame> &stack
    ) {//todo: return chain via longjmp
        std::shared_ptr<value> ret = nullptr;
        for(std::shared_ptr<value> &line : lines) {
            ret = calc(line, parent, stack);
        }
        if(ret == nullptr){
            return std::shared_ptr<value>(new null());
        }
        return ret;
    }
    static inline std::shared_ptr<value> call_obj(std::shared_ptr<obj>& obj, std::stack<stack_frame> &stack){
        stack_frame& stack_frame = stack.emplace();
        stack_frame.return_value = obj;
        //todo
        // for(auto i=oper->operands.begin()+1; i != oper->operands.end(); i++){
        //     stack_frame[(*i)->name] = *i;
        // }
        if(!setjmp(stack_frame.return_point)){
            std::shared_ptr<value> lines = std::static_pointer_cast<value>(std::make_shared<list>(obj->lines));
            (void)calc(lines, obj, stack);
        }
        std::shared_ptr<value> ans = stack_frame.return_value;
        stack.pop();
        return ans;
    }
}

namespace bscp::script
{    
    static std::shared_ptr<value> calc(std::shared_ptr<value> &val, std::shared_ptr<obj>& parent, std::stack<stack_frame> &stack) {
        if(std::shared_ptr<obj> obj = std::dynamic_pointer_cast<bscp::script::obj>(val)){
            return call_obj(obj, stack);
        }
        if(std::shared_ptr<list> list = std::dynamic_pointer_cast<bscp::script::list>(val)){
            return calc_list(list->values, parent, stack);
        }
        if(std::shared_ptr<field> field = std::dynamic_pointer_cast<bscp::script::field>(val)){
            if(field->value != nullptr){
                return field->value;
            }
            if(field->is_tmp){
                auto node = stack.top().find(field->name);
                if(node == stack.top().end()){
                    return std::shared_ptr<value>(new null());
                }else{
                    return node->second;
                }
            }else{
                auto node = parent->static_fields.find(field->name);
                if(node == parent->static_fields.end()){
                    return std::shared_ptr<value>(new null());
                }else{
                    return node->second;
                }
            }
        }
        std::shared_ptr<oper> oper = std::dynamic_pointer_cast<bscp::script::oper>(val);
        if(!oper){
            return val;
        }
        switch (oper->op) {
            case oper::OP_INDEX:
            {
                std::shared_ptr<obj> obj = std::dynamic_pointer_cast<bscp::script::obj>(calc(oper->operands[0], parent, stack));
                if(!obj){
                    return std::shared_ptr<value>(new null());
                }
                std::shared_ptr<value> arg1 = calc(oper->operands[1], parent, stack);

                if(auto num = std::dynamic_pointer_cast<bscp::script::num>(arg1)){
                    auto node = obj->static_fields.find(std::to_wstring((unsigned long long int)num->value));
                    if(node == obj->static_fields.end()){
                        goto err_null;
                    }
                    return node->second;
                }else if(std::dynamic_pointer_cast<bscp::script::obj>(arg1)){
                    // capture wstring
                    std::wstringstream ind;
                    std::wstring wstr;
                    call_print(oper->operands[1], ind);
                    ind >> wstr;

                    // find target
                    auto node = obj->static_fields.find(wstr);
                    if(node == obj->static_fields.end()){
                        goto err_null;
                    }
                    return node->second;
                }else{
                    std::cerr<<"invalid operand 1"<<std::endl;
                    goto err_null;
                }
            }
            case oper::OP_PLUS:
            {
                auto a = std::dynamic_pointer_cast<num>(calc(oper->operands[0], parent, stack));
                auto b = std::dynamic_pointer_cast<num>(calc(oper->operands[1], parent, stack));
                if(!a) {
                    std::cerr<<"a must be a numeric expression"<<std::endl;
                    goto err_null;
                }
                if(!b) {
                    std::cerr<<"b must be a numeric expression"<<std::endl;
                    goto err_null;
                }
                return std::shared_ptr<value>(new num(oper->parent, a->value + b->value));
            }
            case oper::OP_MINUS:
            {
                auto a = std::dynamic_pointer_cast<num>(calc(oper->operands[0], parent, stack));
                auto b = std::dynamic_pointer_cast<num>(calc(oper->operands[1], parent, stack));
                if(!a) {
                    std::cerr<<"a must be a numeric expression"<<std::endl;
                    goto err_null;
                }
                if(!b) {
                    std::cerr<<"b must be a numeric expression"<<std::endl;
                    goto err_null;
                }
                return std::shared_ptr<value>(new num(oper->parent, a->value - b->value));
            }
            case oper::OP_MULTIPLY:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value * static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_DIVIDE:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value / static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_MODULO:
                return std::shared_ptr<value>(new num(oper->parent, modfl(static_cast<num*>(oper->operands[0].get())->value, &static_cast<num*>(oper->operands[1].get())->value)));
            case oper::OP_SHIFT_LEFT:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) << static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value)));
            case oper::OP_SHIFT_RIGHT:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) >> static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value)));
            case oper::OP_BITWISE_AND:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) & static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value)));
            case oper::OP_BITWISE_OR:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) | static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value)));
            case oper::OP_BITWISE_XOR:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) ^ static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value)));
            case oper::OP_LOGICAL_AND:
                return std::shared_ptr<value>(new num(oper->parent, bscp::script::value2bool(oper->operands[0]) && bscp::script::value2bool(oper->operands[1])));
            case oper::OP_LOGICAL_OR:
                return std::shared_ptr<value>(new num(oper->parent, bscp::script::value2bool(oper->operands[0]) || bscp::script::value2bool(oper->operands[1])));
            case oper::OP_LESS_THAN:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value < static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_LESS_THAN_OR_EQUAL:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value <= static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_GREATER_THAN:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value > static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_GREATER_THAN_OR_EQUAL:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value >= static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_EQUAL_TO:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value == static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_NOT_EQUAL_TO:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value != static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_UNARY_PLUS:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value));
            case oper::OP_UNARY_MINUS:
                return std::shared_ptr<value>(new num(oper->parent, -static_cast<num*>(oper->operands[0].get())->value));
            case oper::OP_BITWISE_NOT:
                return std::shared_ptr<value>(new num(oper->parent, ~static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value)));
            case oper::OP_INCREMENT_GET:
                static_cast<num*>(oper->operands[0].get())->value += 1;
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value));
            case oper::OP_DECREMENT_GET:
                static_cast<num*>(oper->operands[0].get())->value -= 1;
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value));
            case oper::OP_ASSIGN:
            {
                std::shared_ptr<field> field = std::dynamic_pointer_cast<bscp::script::field>(oper->operands[0]);
                if (!field) {
                    std::cerr<<"invalid field\n";
                    return std::shared_ptr<value>(new null());
                }

                std::shared_ptr<value> value = calc(oper->operands[1], parent, stack);
                value->parent = parent;
                if(field->is_tmp){
                    stack.top().insert_or_assign(field->name, value);
                }else{
                    parent->static_fields.insert_or_assign(field->name, value);
                }

                return std::shared_ptr<bscp::script::value>(field);
            }
            case oper::OP_ADD_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value += static_cast<num*>(oper->operands[1].get())->value;
                }
                return oper->operands[0];
            case oper::OP_SUBTRACT_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value -= static_cast<num*>(oper->operands[1].get())->value;
                }
                return oper->operands[0];
            case oper::OP_MULTIPLY_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value *= static_cast<num*>(oper->operands[1].get())->value;
                }
                return oper->operands[0];
            case oper::OP_DIVIDE_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value /= static_cast<num*>(oper->operands[1].get())->value;
                }
                return oper->operands[0];
            case oper::OP_MODULO_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value = fmod(static_cast<num*>(oper->operands[0].get())->value, static_cast<num*>(oper->operands[1].get())->value);
                }
                return oper->operands[0];
            case oper::OP_SHIFT_LEFT_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value = static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) << static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value);
                }
                return oper->operands[0];
            case oper::OP_SHIFT_RIGHT_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value = static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) >> static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value);
                }
                return oper->operands[0];
            case oper::OP_BITWISE_AND_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value = static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) & static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value);
                }
                return oper->operands[0];
            case oper::OP_BITWISE_OR_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value = static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) | static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value);
                }
                return oper->operands[0];
            case oper::OP_BITWISE_XOR_ASSIGN:
                if (dynamic_cast<num*>(oper->operands[0].get())) {
                    static_cast<num*>(oper->operands[0].get())->value = static_cast<long long>(static_cast<num*>(oper->operands[0].get())->value) ^ static_cast<long long>(static_cast<num*>(oper->operands[1].get())->value);
                }
                return oper->operands[0];
            case oper::OP_TERNARY_CONDITIONAL:
            {
                std::shared_ptr<value> cond = calc(oper->operands[0], parent, stack);
                return value2bool(cond) ? calc(oper->operands[1], parent, stack) : calc(oper->operands[2], parent, stack);
            }
            case oper::OP_CALL_OBJ:
            {
                // special references
                if(oper->operands.size() <= 0){
                    std::cerr<<"invalid object call\n";
                    return std::shared_ptr<value>(new null());
                }
                std::shared_ptr<bscp::script::field> field = std::dynamic_pointer_cast<bscp::script::field>(oper->operands[0]);
                if(__glibc_unlikely(field != nullptr)){
                    if(field->name == L"print"){
                        std::shared_ptr<value> msg = calc(oper->operands[1], parent, stack);
                        return call_print(msg, std::wcout);
                    }
                    if(field->name == L"debug"){
                        std::shared_ptr<value> msg = calc(oper->operands[1], parent, stack);
                        return std::static_pointer_cast<value>(std::make_shared<num>(nullptr, call_debug(msg, 0)));
                    }
                }

                // dereference
                std::shared_ptr<bscp::script::obj> obj = std::dynamic_pointer_cast<bscp::script::obj>(oper->operands[0]);
                if(!obj){
                    std::shared_ptr<bscp::script::field> id = std::dynamic_pointer_cast<bscp::script::field>(oper->operands[0]);
                    if(id->is_tmp){
                        auto node = stack.top().find(id->name);
                        if(node != stack.top().end()){
                            obj = std::dynamic_pointer_cast<bscp::script::obj>(node->second);
                        }
                    }else{
                        auto node = parent->static_fields.find(id->name);
                        if(node != parent->static_fields.end()){
                            obj = std::dynamic_pointer_cast<bscp::script::obj>(node->second);
                        }
                    }
                }
                if(!obj){
                    std::cerr<<"invalid object call: invalid id\n";
                    return std::shared_ptr<value>(new null());
                }
                return call_obj(obj, stack);
            }
            default:
                std::wcout<<oper->op<<std::endl;
                fwprintf(stderr, L"Unknown operation\n");
                return std::shared_ptr<value>(new null());
        }
    err_null:
        return std::shared_ptr<value>(new null());
    }
    static void wprintln_indent(size_t indent = 0){
        wprintf(L"\n");
        for (size_t i = 0; i < indent; i++)
        {
            wprintf(L"+   ");
        }
    }
    int call_debug(std::shared_ptr<value> &value){
        return call_debug(value, 0);
    }
    static int call_debug(std::shared_ptr<value> &value, size_t indent){
        if(std::dynamic_pointer_cast<num>(value)){
            wprintf(L"%Lf", std::static_pointer_cast<num>(value)->value);
            return 0;
        }
        if(std::dynamic_pointer_cast<oper>(value)){
            std::shared_ptr<oper> oper = std::static_pointer_cast<bscp::script::oper>(value);
            wprintf(L"OPER(%d)[", oper->op);
            if(oper->operands.size() > 0){
                for(auto arg : oper->operands){
                    wprintln_indent(indent + 1);
                    call_debug(arg, indent + 1);
                    wprintf(L",");
                }
                wprintln_indent(indent);
            }
            wprintf(L"]");
            return 0;
        }
        if(std::dynamic_pointer_cast<null>(value)){
            wprintf(L"(bscp::script::null)");
            return 0;
        }
        if(std::shared_ptr<obj> obj = std::dynamic_pointer_cast<bscp::script::obj>(value)){
            wprintf(L"OBJ[");
            if(obj->static_fields.size() > 0){
                for(auto [name, field]: obj->static_fields){
                    wprintln_indent(indent + 1);
                    wprintf(L".%ls = ", name.c_str());
                    call_debug(field, indent + 1);
                    wprintf(L",");
                }
                wprintln_indent(indent);
            }
            wprintf(L"]{");
            if(obj->lines.size() > 0){
                for(auto i=obj->lines.begin(); i+1 != obj->lines.end(); i++){
                    wprintln_indent(indent + 1);
                    call_debug(*i, indent + 1);
                    wprintf(L";");
                }
                wprintln_indent(indent + 1);
                call_debug(obj->lines.back(), indent + 1);
            }
            wprintf(L"}");

            return 0;
        }
        if(std::shared_ptr<field> field = std::dynamic_pointer_cast<bscp::script::field>(value)){
            std::wcout<<"ID("<<field->name<<")";
            return 0;
        }
        if(value == nullptr){
            std::wcout<<"(c_null)";
            return 0;
        }
        if(std::shared_ptr<list> list = std::dynamic_pointer_cast<bscp::script::list>(value)){
            for(auto i=list->values.begin(); i+1 != list->values.end(); i++){
                call_debug(*i, indent + 1);
                wprintf(L";");
            }
            call_debug(list->values.back(), indent + 1);
            return 0;
        }
        std::cerr<<"invalid value\n";
        return 1;
    }
}

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
    
    std::stack<bscp::script::stack_frame> stack;
    stack.emplace();

    std::shared_ptr<bscp::script::obj> root(new bscp::script::obj(nullptr));
    root->parent = root;
    root->lines = std::dynamic_pointer_cast<bscp::script::obj>(output)->lines;
    (void)bscp::script::calc(output, root, stack);

    output = nullptr;
    file_stack.pop();

    file.close();
    delete lexer;
    lexer = nullptr;

    return result;
}
int repl_mode() {
    wprintf(L"bscp REPL (press Ctrl+D to exit)\n");

    std::string line;
    if(lexer != nullptr){
        delete lexer;
    }
    while (wprintf(L"> ") && (std::cin>>line)){
        if (line.length() == 1 && line[0] == '\n') continue;
        
        std::istringstream yyin(line, std::ios::in);

        lexer = new yyFlexLexer(&yyin);

        yy::parser parser;
        parser.parse();
        int result = yy::parser()();
        
        if (result != 0) {
            fwprintf(stderr, L"Error parsing input\n");
        }
        delete lexer;
    }
    lexer = nullptr;
    
    wprintf(L"\n");
    return 0;
}



const char *argp_program_version = "bscp 1.0";
const char *argp_program_bug_address = "<bug@example.com>";
static char doc[] = "bscp - A simple expression language interpreter";
static char args_doc[] = "[FILE]";

static struct argp_option options[] = {
    {"debug", 'd', 0, 0, "Enable debug output", 0},
    {NULL, 0, 0, 0, NULL, 0}
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
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {
    .options = options,
    .parser = parse_opt,
    .args_doc = args_doc,
    .doc = doc,
    .children = NULL,
    .help_filter = NULL,
    .argp_domain = NULL,
};

__attribute__((weak)) 
int main(int argc, char *argv[]) {
    if (setlocale(LC_ALL, "") == NULL) {
        fwprintf(stderr, L"警告：无法设置 locale。宽字符输出可能不正常。\n");
    }

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
