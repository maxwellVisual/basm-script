#ifndef _BSCP_AST_H
#define _BSCP_AST_H

#include "lex.hpp"

#include <map>
#include <limits>
#include <string>
#include <vector>
#include <initializer_list>
#include <memory>
#include <queue>

namespace bscp::script
{
    class obj;

    class value
    {
    public:
        std::shared_ptr<obj> parent;
        virtual ~value() {} // Needed for dynamic_cast
    protected:
        value(std::shared_ptr<obj> parent): parent(parent){}
    };

    class null: public value
    {
    public:
        null(std::shared_ptr<obj> parent = nullptr): value(parent){}
    };
    class num: public value
    {
    public:
        long double value;

        // positive value only
        num(std::shared_ptr<obj> parent, std::wstring& raw)
            : bscp::script::value(parent), value(std::wcstold(raw.c_str(), nullptr)){}

        num(std::shared_ptr<obj> parent, long double value)
            : bscp::script::value(parent), value(value){}

        num(std::shared_ptr<obj> parent)
            : bscp::script::value(parent), value(0){}
    };

    class list: public value
    {
    public:
        std::vector<std::shared_ptr<value>> values;

        list(std::initializer_list<std::shared_ptr<value>> values)
            : value(nullptr), values(values){}
        list(std::vector<std::shared_ptr<value>> values)
            : value(nullptr), values(values){}
    };

    class oper: public value
    {
    public:
        enum operation_type {
            OP_DOT,
            OP_INDEX,
            OP_DOT_WORD,
            OP_UNARY_NOT,
            OP_PLUS,
            OP_MINUS,
            OP_MULTIPLY,
            OP_DIVIDE,
            OP_MODULO,
            OP_SHIFT_LEFT,
            OP_SHIFT_RIGHT,
            OP_BITWISE_AND,
            OP_BITWISE_OR,
            OP_BITWISE_XOR,
            OP_LOGICAL_AND,
            OP_LOGICAL_OR,
            OP_LESS_THAN,
            OP_LESS_THAN_OR_EQUAL,
            OP_GREATER_THAN,
            OP_GREATER_THAN_OR_EQUAL,
            OP_EQUAL_TO,
            OP_NOT_EQUAL_TO,
            OP_UNARY_PLUS,
            OP_UNARY_MINUS,
            OP_BITWISE_NOT,
            OP_GET_INCREMENT,
            OP_GET_DECREMENT,
            OP_INCREMENT_GET,
            OP_DECREMENT_GET,
            OP_ASSIGN,
            OP_ADD_ASSIGN,
            OP_SUBTRACT_ASSIGN,
            OP_MULTIPLY_ASSIGN,
            OP_DIVIDE_ASSIGN,
            OP_MODULO_ASSIGN,
            OP_SHIFT_LEFT_ASSIGN,
            OP_SHIFT_RIGHT_ASSIGN,
            OP_BITWISE_AND_ASSIGN,
            OP_BITWISE_OR_ASSIGN,
            OP_BITWISE_XOR_ASSIGN,
            OP_TERNARY_CONDITIONAL,
            OP_CALL_OBJ,
        };

        operation_type op;
        std::vector<std::shared_ptr<value>> operands;

        oper(operation_type op, std::initializer_list<std::shared_ptr<value>> operands)
            : value(nullptr), op(op), operands(operands) {}
        oper(operation_type op, std::shared_ptr<list>& operands)
            : value(nullptr), op(op), operands(operands->values){}
    };


    class field: public value
    {
    public:
        std::shared_ptr<value> value;
        std::wstring name;
        bool is_tmp;
        field(std::shared_ptr<bscp::script::value> value = nullptr, bool is_tmp = true, std::shared_ptr<obj> parent = nullptr): bscp::script::value(parent), value(value), is_tmp(is_tmp){}
    };

    class obj: public value
    {
    public:
        std::vector<std::shared_ptr<value>> lines;
        std::map<std::wstring, std::shared_ptr<value>> static_fields;

        obj(std::shared_ptr<obj> parent = nullptr): value(parent){}
        inline std::shared_ptr<value>& operator[](size_t id){
            return this->static_fields[std::to_wstring(id)];
        }
        inline std::shared_ptr<value>& operator[](std::wstring& name){
            return this->static_fields[name];
        }
    };

    extern bool value2bool(std::shared_ptr<value> &value);
}




namespace bscp::script
{

template<typename _CharT>
class escape_stream
{
private:
    // 状态机变量
    enum class state_t {
        S_NORMAL,           // 正常字符状态
        S_ESCAPE,           // 转义符开始状态
        S_HEX_FIRST,        // 十六进制转义符第一个字符状态
        S_HEX_CONT,         // 十六进制转义符后续字符状态
        S_UNICODE_U,        // \u Unicode转义符状态
        S_UNICODE_UU,       // \U Unicode转义符状态
        S_OCTAL             // 八进制转义符状态
    };

    // 调用者视角的输入和输出
    std::queue<_CharT> obuf;
    std::queue<_CharT> ibuf;

    std::basic_string<_CharT> escape_str;
    state_t state;
public:
    escape_stream(const std::basic_string<_CharT> &input);

    escape_stream();

    void put(const _CharT* str, std::streamsize n);
    void put(const _CharT& c);
    void put(const std::basic_string<_CharT> str);

private:
    void step(_CharT &c);
    _CharT next();
public:
    void get(std::basic_string<_CharT> &buf);
    void get(wchar_t &buf);
};

extern std::shared_ptr<obj> format_str_const(const std::wstring &str);
extern std::shared_ptr<num> format_chr_const(const std::wstring &str);
extern std::shared_ptr<value> call_print(std::shared_ptr<value> &msg_val, std::wostream& wout);


} // namespace bscp::script

#endif// _BSCP_AST_H
