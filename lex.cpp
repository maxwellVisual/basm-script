#include "lex.hpp"

#include <FlexLexer.h>
#include "bscp.hpp"

yyFlexLexer* lexer = nullptr;

/* 全局变量，用于存储当前识别的token */
struct lex_token current_token = { yy::parser::token_kind_type::lex_unknown, 0, NULL };

/**
 * 获取下一个词法单元，调用者需要释放 current_token
 * 
 * @param buf 用于存储词法单元的缓冲区
 * @return 1表示成功，0表示遇到错误或文件结束
 */
int lex_next(struct lex_token* buf) {
    if(lexer == nullptr){
        return 0;
    }
    int ret = lexer->yylex();
    // printf("token: %d '%s'\n", current_token.type, current_token.raw);
    if(buf == NULL){
        return ret;
    }
    if (ret) {
        /* 复制当前token到输出buffer */
        buf->type = current_token.type;
        buf->raw_size = current_token.raw_size;
        
        if (current_token.raw) {
            /* 直接传递raw指针的所有权给调用者，调用者负责释放内存 */
            buf->raw = current_token.raw;
            current_token.raw = NULL;
        } else {
            buf->raw = NULL;
        }
        
        return 1;
    }
    
    return 0;
}

