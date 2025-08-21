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
#include <csetjmp>

#include "lex.hpp"

std::shared_ptr<bscp::value> output;
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

namespace bscp
{
    class stack_frame: public std::map<std::string, std::shared_ptr<value>> {
        public:
        std::jmp_buf return_point;
        std::shared_ptr<value> return_value;
    };
    static std::shared_ptr<value> calc(std::shared_ptr<value>& val, std::shared_ptr<obj> &parent, std::stack<stack_frame> &stack);
    
    // int eval(std::shared_ptr<value>& value);
    int eval(std::shared_ptr<value>& value, size_t indent = 0);

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
            return std::shared_ptr<value>(new null(nullptr));
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

    static inline std::shared_ptr<value> call_print(std::shared_ptr<value> &msg_val){
        std::shared_ptr<obj> msg_obj = std::dynamic_pointer_cast<obj>(msg_val);
        std::shared_ptr<num> num = nullptr;

        if(!msg_obj){
            return std::shared_ptr<value>(new bscp::num(nullptr, 0));
        }

        // 状态机变量
        enum EscapeState {
            STATE_NORMAL,           // 正常字符状态
            STATE_ESCAPE,           // 转义符开始状态
            STATE_HEX_FIRST,        // 十六进制转义符第一个字符状态
            STATE_HEX_CONT,         // 十六进制转义符后续字符状态
            STATE_UNICODE_U,        // \u Unicode转义符状态
            STATE_UNICODE_UU,       // \U Unicode转义符状态
            STATE_OCTAL            // 八进制转义符状态
        };

        EscapeState state = STATE_NORMAL;
        wchar_t c = 0;
        std::wstring escape_str;    // 复用字符序列缓存

        for (size_t i = 0; msg_obj->static_fields.find(std::to_string(i)) != msg_obj->static_fields.end(); i++)
        {
            std::shared_ptr<bscp::num> num = std::dynamic_pointer_cast<bscp::num>((*msg_obj)[i]);
            if(!num || num->value < 0){
                continue;
            }
            c = (wchar_t)num->value;

            switch (state) {
                case STATE_NORMAL:
                    if (c == L'\\') {
                        state = STATE_ESCAPE;
                    } else {
                        std::wcout<<c;
                    }
                    break;

                case STATE_ESCAPE:
                    switch (c) {
                        case L'\'':
                            std::wcout<<L'\'';
                            state = STATE_NORMAL;
                            break;
                        case L'\"':
                            std::wcout<<L'\"';
                            state = STATE_NORMAL;
                            break;
                        case L'\?':
                            std::wcout<<L'\?';
                            state = STATE_NORMAL;
                            break;
                        case L'\\':
                            std::wcout<<L'\\';
                            state = STATE_NORMAL;
                            break;
                        case L'a':
                            std::wcout<<L'\a';
                            state = STATE_NORMAL;
                            break;
                        case L'b':
                            std::wcout<<L'\b';
                            state = STATE_NORMAL;
                            break;
                        case L'f':
                            std::wcout<<L'\f';
                            state = STATE_NORMAL;
                            break;
                        case L'n':
                            std::wcout<<L'\n';
                            state = STATE_NORMAL;
                            break;
                        case L'r':
                            std::wcout<<L'\r';
                            state = STATE_NORMAL;
                            break;
                        case L't':
                            std::wcout<<L'\t';
                            state = STATE_NORMAL;
                            break;
                        case L'v':
                            std::wcout<<L'\v';
                            state = STATE_NORMAL;
                            break;
                        case L'x':
                            state = STATE_HEX_FIRST;
                            escape_str.clear();
                            break;
                        case L'u':
                            state = STATE_UNICODE_U;
                            escape_str.clear();
                            break;
                        case L'U':
                            state = STATE_UNICODE_UU;
                            escape_str.clear();
                            break;
                        case L'0': case L'1': case L'2': case L'3': case L'4':
                        case L'5': case L'6': case L'7':
                            state = STATE_OCTAL;
                            escape_str.clear();
                            escape_str += c;
                            break;
                        default:
                            std::wcout<<c;
                            state = STATE_NORMAL;
                            break;
                    }
                    break;

                case STATE_HEX_FIRST:
                    if (std::isxdigit(c)) {
                        escape_str += c;
                        state = STATE_HEX_CONT;
                    } else {
                        // 如果不是十六进制字符，就打印'x'并处理当前字符
                        std::wcout<<L'x';
                        if (c == L'\\') {
                            state = STATE_ESCAPE;
                        } else {
                            std::wcout<<c;
                            state = STATE_NORMAL;
                        }
                    }
                    break;

                case STATE_HEX_CONT:
                    if (std::isxdigit(c) && escape_str.length() < 8) {
                        escape_str += c;
                        // 继续读取更多十六进制字符
                    } else {
                        // 遇到非十六进制字符，解析并输出十六进制值
                        if (!escape_str.empty()) {
                            wchar_t hex_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                            std::wcout<<hex_val;
                        }
                        if (c == L'\\') {
                            state = STATE_ESCAPE;
                        } else {
                            std::wcout<<c;
                            state = STATE_NORMAL;
                        }
                    }
                    break;

                case STATE_UNICODE_U:
                    if (std::isxdigit(c)) {
                        escape_str += c;
                        if (escape_str.length() >= 4) {
                            // 已经读取了4个十六进制字符
                            wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                            std::wcout<<unicode_val;
                            state = STATE_NORMAL;
                        }
                    } else {
                        // 如果不是有效的十六进制字符，就打印'u'和已解析的字符
                        std::wcout<<L'u';
                        if (!escape_str.empty()) {
                            wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                            std::wcout<<unicode_val;
                        }
                        if (c == L'\\') {
                            state = STATE_ESCAPE;
                        } else {
                            std::wcout<<c;
                            state = STATE_NORMAL;
                        }
                    }
                    break;

                case STATE_UNICODE_UU:
                    if (std::isxdigit(c)) {
                        escape_str += c;
                        if (escape_str.length() >= 8) {
                            // 已经读取了8个十六进制字符
                            wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                            std::wcout<<unicode_val;
                            state = STATE_NORMAL;
                        }
                    } else {
                        // 如果不是有效的十六进制字符，就打印'U'和已解析的字符
                        std::wcout<<L'U';
                        if (!escape_str.empty()) {
                            wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                            std::wcout<<unicode_val;
                        }
                        if (c == L'\\') {
                            state = STATE_ESCAPE;
                        } else {
                            std::wcout<<c;
                            state = STATE_NORMAL;
                        }
                    }
                    break;

                case STATE_OCTAL:
                    if (c >= L'0' && c <= L'7' && escape_str.length() < 11) {
                        escape_str += c;
                        // 继续读取更多八进制字符
                    } else {
                        // 遇到非八进制字符，解析并输出八进制值
                        if (!escape_str.empty()) {
                            wchar_t oct_val = std::wcstoul(escape_str.c_str(), nullptr, 8);
                            std::wcout<<oct_val;
                        }
                        if (c == L'\\') {
                            state = STATE_ESCAPE;
                        } else {
                            std::wcout<<c;
                            state = STATE_NORMAL;
                        }
                    }
                    break;
            }
        }

        // 处理结束时的状态
        switch (state) {
            case STATE_ESCAPE:
                // 以反斜杠结尾，输出反斜杠
                std::wcout<<L'\\';
                break;
            case STATE_HEX_FIRST:
                // 以\x结尾，输出x
                std::wcout<<L'x';
                break;
            case STATE_HEX_CONT:
                // 解析并输出十六进制值
                if (!escape_str.empty()) {
                    wchar_t hex_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    std::wcout<<hex_val;
                }
                break;
            case STATE_UNICODE_U:
                // 输出u和已解析的部分
                std::wcout<<L'u';
                if (!escape_str.empty()) {
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    std::wcout<<unicode_val;
                }
                break;
            case STATE_UNICODE_UU:
                // 输出U和已解析的部分
                std::wcout<<L'U';
                if (!escape_str.empty()) {
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    std::wcout<<unicode_val;
                }
                break;
            case STATE_OCTAL:
                // 解析并输出八进制值
                if (!escape_str.empty()) {
                    wchar_t oct_val = std::wcstoul(escape_str.c_str(), nullptr, 8);
                    std::wcout<<oct_val;
                }
                break;
            default:
                break;
        }

        return std::shared_ptr<value>(new null(nullptr));
    }
    static std::shared_ptr<value> calc(std::shared_ptr<value> &val, std::shared_ptr<obj>& parent, std::stack<stack_frame> &stack) {
        if(std::shared_ptr<obj> obj = std::dynamic_pointer_cast<bscp::obj>(val)){
            return call_obj(obj, stack);
        }
        if(std::shared_ptr<list> list = std::dynamic_pointer_cast<bscp::list>(val)){
            return calc_list(list->values, parent, stack);
        }
        if(std::shared_ptr<field> field = std::dynamic_pointer_cast<bscp::field>(val)){
            if(field->value != nullptr){
                return field->value;
            }
            if(field->is_tmp){
                auto node = stack.top().find(field->name);
                if(node == stack.top().end()){
                    return std::shared_ptr<value>(new null(nullptr));
                }else{
                    return node->second;
                }
            }else{
                auto node = parent->static_fields.find(field->name);
                if(node == parent->static_fields.end()){
                    return std::shared_ptr<value>(new null(nullptr));
                }else{
                    return node->second;
                }
            }
        }
        std::shared_ptr<oper> oper = std::dynamic_pointer_cast<bscp::oper>(val);
        if(!oper){
            return val;
        }
        switch (oper->op) {
            case oper::OP_PLUS:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value + static_cast<num*>(oper->operands[1].get())->value));
            case oper::OP_MINUS:
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value - static_cast<num*>(oper->operands[1].get())->value));
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
                return std::shared_ptr<value>(new num(oper->parent, bscp::value2bool(oper->operands[0]) && bscp::value2bool(oper->operands[1])));
            case oper::OP_LOGICAL_OR:
                return std::shared_ptr<value>(new num(oper->parent, bscp::value2bool(oper->operands[0]) || bscp::value2bool(oper->operands[1])));
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
            case oper::OP_INCREMENT:
                static_cast<num*>(oper->operands[0].get())->value += 1;
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value));
            case oper::OP_DECREMENT:
                static_cast<num*>(oper->operands[0].get())->value -= 1;
                return std::shared_ptr<value>(new num(oper->parent, static_cast<num*>(oper->operands[0].get())->value));
            case oper::OP_ASSIGN:
            {
                std::shared_ptr<field> field = std::dynamic_pointer_cast<bscp::field>(oper->operands[0]);
                if (!field) {
                    std::cerr<<"invalid field\n";
                    return std::shared_ptr<value>(new null(nullptr));
                }

                std::shared_ptr<value> value = calc(oper->operands[1], parent, stack);
                value->parent = parent;
                if(field->is_tmp){
                    stack.top().insert_or_assign(field->name, value);
                }else{
                    parent->static_fields.insert_or_assign(field->name, value);
                }

                for(auto i : stack.top()){
                    std::shared_ptr<bscp::value> &v = dynamic_cast<bscp::field*>(i.second.get())->value;
                    std::cout<<i.first<<": ";
                    eval(v);
                    std::cout<<"\n";
                }
                return std::shared_ptr<bscp::value>(field);
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
                    return std::shared_ptr<value>(new null(oper->parent));
                }
                std::shared_ptr<bscp::field> field = std::dynamic_pointer_cast<bscp::field>(oper->operands[0]);
                if(__glibc_unlikely(field != nullptr)){
                    if(field->name == "print"){
                        std::shared_ptr<bscp::value> msg = calc(oper->operands[1], parent, stack);
                        return call_print(msg);
                    }
                    if(field->name == "debug"){
                        std::shared_ptr<bscp::value> msg = calc(oper->operands[1], parent, stack);
                        return std::static_pointer_cast<value>(std::make_shared<num>(nullptr, eval(msg)));
                    }
                }

                // dereference
                std::shared_ptr<bscp::obj> obj = std::dynamic_pointer_cast<bscp::obj>(oper->operands[0]);
                if(!obj){
                    std::shared_ptr<bscp::field> id = std::dynamic_pointer_cast<bscp::field>(oper->operands[0]);
                    if(id->is_tmp){
                        auto node = stack.top().find(id->name);
                        if(node != stack.top().end()){
                            obj = std::dynamic_pointer_cast<bscp::obj>(node->second);
                        }
                    }else{
                        auto node = parent->static_fields.find(id->name);
                        if(node != parent->static_fields.end()){
                            obj = std::dynamic_pointer_cast<bscp::obj>(node->second);
                        }
                    }
                }
                if(!obj){
                    std::cerr<<"invalid object call: invalid id\n";
                    return std::shared_ptr<value>(new null(oper->parent));
                }
                return call_obj(obj, stack);
            }
            default:
                fwprintf(stderr, L"Unknown operation\n");
                return std::shared_ptr<value>(new null(oper->parent));
        }
    }
    static void println_indent(size_t indent = 0){
        wprintf(L"\n");
        for (size_t i = 0; i < indent; i++)
        {
            wprintf(L"+---");
        }
    }
    int eval(std::shared_ptr<value> &value, size_t indent){
        if(std::dynamic_pointer_cast<num>(value)){
            wprintf(L"%Lf", std::static_pointer_cast<num>(value)->value);
            return 0;
        }
        if(std::dynamic_pointer_cast<oper>(value)){
            std::shared_ptr<oper> oper = std::static_pointer_cast<bscp::oper>(value);
            wprintf(L"OPER(%d)[", oper->op);
            for(auto arg : oper->operands){
                eval(arg, indent + 1);
                wprintf(L", ");
            }
            wprintf(L"]");
            return 0;
        }
        if(std::dynamic_pointer_cast<null>(value)){
            wprintf(L"(bscp::null)");
            return 0;
        }
        if(std::shared_ptr<obj> obj = std::dynamic_pointer_cast<bscp::obj>(value)){
            wprintf(L"OBJ[");
            for(auto [name, field]: obj->static_fields){
                wprintf(L"%s: ", name.c_str());
                eval(field, indent + 1);
                wprintf(L", ");
            }
            wprintf(L"]{");
            if(obj->lines.size() > 0){
                println_indent(indent);
                for(auto i=obj->lines.begin(); i+1 != obj->lines.end(); i++){
                    eval(*i, indent + 1);
                    wprintf(L";");
                    println_indent(indent);
                }
                eval(obj->lines.back(), indent + 1);
            }
            wprintf(L"}");

            return 0;
        }
        if(std::shared_ptr<field> field = std::dynamic_pointer_cast<bscp::field>(value)){
            std::cout<<"ID("<<field->name<<")";
            return 0;
        }
        if(value == nullptr){
            std::cout<<"(c_null)";
            return 0;
        }
        if(std::shared_ptr<list> list = std::dynamic_pointer_cast<bscp::list>(value)){
            for(auto i=list->values.begin(); i+1 != list->values.end(); i++){
                eval(*i, indent + 1);
                wprintf(L";");
            }
            eval(list->values.back(), indent + 1);
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
    
    std::stack<bscp::stack_frame> stack;
    stack.emplace();

    std::shared_ptr<bscp::obj> root(new bscp::obj(nullptr));
    root->parent = root;
    root->lines = std::dynamic_pointer_cast<bscp::obj>(output)->lines;
    // auto a = std::static_pointer_cast<bscp::value>(root);
    // eval(a);
    (void)bscp::calc(output, root, stack);

    output = nullptr;
    file_stack.pop();

    file.close();
    delete lexer;
    lexer = nullptr;

    return result;
}

int repl_mode() {
    wprintf(L"bscp REPL (press Ctrl+D to exit)\n");

    char line[1024];
    if(lexer != nullptr){
        delete lexer;
    }
    while (wprintf(L"> ") && fgets(line, sizeof(line), stdin)) {
        if (strlen(line) == 1 && line[0] == '\n') continue;
        
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

__attribute__((weak)) 
int main(int argc, char *argv[]) {
    if (setlocale(LC_ALL, "") == NULL) {
        fwprintf(stderr, L"警告：无法设置 locale。宽字符输出可能不正常。\n");
    }
    return execute_file("demo.bs");
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
