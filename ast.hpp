#ifndef _BSCP_AST_H
#define _BSCP_AST_H

#include "lex.hpp"

#include <map>
#include <limits>
#include <string>
#include <vector>
#include <initializer_list>
#include <memory>

namespace bscp
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
        null(std::shared_ptr<obj> parent): value(parent){}
    };
    class num: public value
    {
    public:
        long double value;

        // positive value only
        num(std::shared_ptr<obj> parent, std::string& raw)
            : bscp::value(parent), value(std::strtold(raw.c_str(), nullptr)){}

        num(std::shared_ptr<obj> parent, long double value)
            : bscp::value(parent), value(value){}

        num(std::shared_ptr<obj> parent)
            : bscp::value(parent), value(0){}
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
            OP_INCREMENT,
            OP_DECREMENT,
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
        std::string name;
        bool is_tmp;
        field(std::shared_ptr<bscp::value> value = nullptr, bool is_tmp = true, std::shared_ptr<obj> parent = nullptr): bscp::value(parent), value(value), is_tmp(is_tmp){}
    };

    class obj: public value
    {
    public:
        std::vector<std::shared_ptr<value>> lines;
        std::map<std::string, std::shared_ptr<value>> static_fields;

        obj(std::shared_ptr<obj> parent): value(parent){}
        inline std::shared_ptr<value>& operator[](size_t id){
            return this->static_fields[std::to_string(id)];
        }
        inline std::shared_ptr<value>& operator[](std::string& name){
            return this->static_fields[name];
        }
    };

    extern bool value2bool(std::shared_ptr<value> &value);
}

extern struct lex_token token;

#endif// _BSCP_AST_H
