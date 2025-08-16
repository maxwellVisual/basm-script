#include "lex.hpp"

#include <FlexLexer.h>

/* Flex相关类型定义 */
struct yy_buffer_state;
typedef struct yy_buffer_state* YY_BUFFER_STATE;

/* Flex生成的函数前向声明 */
extern int yylex(void);
extern YY_BUFFER_STATE yy_scan_bytes(const char* bytes, int len);
extern void yy_switch_to_buffer(YY_BUFFER_STATE buffer);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern char* yytext;
extern int yyleng;

yyFlexLexer* lexer = nullptr;

/* 全局变量，用于存储当前识别的token */
struct lex_token current_token = { lex_unknown, 0, NULL };

// std::istream yyin;

/**
 * 获取下一个词法单元
 * 
 * @param buf 用于存储词法单元的缓冲区
 * @return 1表示成功，0表示遇到错误或文件结束
 */
int lex_next(struct lex_token* buf) {
    int ret = lexer->yylex();
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

