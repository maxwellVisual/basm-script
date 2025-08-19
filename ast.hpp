#ifndef _BSCP_AST_H
#define _BSCP_AST_H

#include "lex.hpp"

#include <map>
#include <limits>
#include <string>
#include <vector>
#include <initializer_list>

class bscp_obj;

class bscp_value
{
public:
    std::string name;
    bscp_obj* parent;
    bscp_value* last;
    virtual ~bscp_value() {} // Needed for dynamic_cast
protected:
    bscp_value(bscp_obj* parent): parent(parent), last(nullptr){}
};

class bscp_null: public bscp_value
{
public:
    bscp_null(bscp_obj* parent): bscp_value(parent){}
};

class bscp_num: public bscp_value
{
public:
    long double value;

    // positive value only
    bscp_num(bscp_obj* parent, std::string& raw)
        : bscp_value(parent), value(std::strtold(raw.c_str(), nullptr)){}

    bscp_num(bscp_obj* parent, long double value)
        : bscp_value(parent), value(value){}

    bscp_num(bscp_obj* parent)
        : bscp_value(parent), value(0){}
};

class bscp_list: public bscp_value
{
public:
    std::vector<bscp_value*> values;

    bscp_list(std::initializer_list<bscp_value*> values)
        : bscp_value(nullptr), values(values){}
};

class bscp_oper: public bscp_value
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
    std::vector<bscp_value*> operands;

    bscp_oper(operation_type op, std::initializer_list<bscp_value*> operands)
        : bscp_value(nullptr), op(op), operands(operands) {}
    bscp_oper(operation_type op, bscp_list* operands)
        : bscp_value(nullptr), op(op), operands(operands->values){}
};


class Field
{
public:
    bscp_value* parent;
    std::string name;
    bscp_value* value;
    bool is_tmp;
    Field(bscp_value* parent = nullptr): parent(parent), value(nullptr), is_tmp(false) {}
    Field(bscp_value& value, bool is_tmp = true, bscp_value* parent = nullptr): parent(parent), value(&value), is_tmp(is_tmp){}
};

class bscp_obj: public bscp_value
{
public:
    std::vector<bscp_value*> lines;
    std::map<std::string, Field> static_fields;
    bscp_obj(bscp_obj* parent): bscp_value(parent){}
    inline Field& operator[](size_t id){
        return this->static_fields[std::to_string(id)];
    }
    inline Field& operator[](std::string& name){
        return this->static_fields[name];
    }
};

extern bool value2bool(bscp_value* value);

extern struct lex_token token;

#endif// _BSCP_AST_H
