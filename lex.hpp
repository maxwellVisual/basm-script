#ifndef __LEX_H__
#define __LEX_H__

#include <cstddef>
#include <cstdlib>
#include <stack>

extern size_t line_count;
extern std::stack<const char*> file_stack;

/* 公共常量定义 */
#define LEX_TOKEN_STREAM_BUFSIZE BUFSIZ
#define INITIAL_BUFFER_SIZE 64
#define BUFFER_GROWTH_FACTOR 2

/* 词法标记结构 */
struct lex_token {
    int type;
    size_t raw_size;
    wchar_t* raw;
    
    void clear(){
        if(__glibc_likely(raw != nullptr)){
            type = 0;
            raw_size = 0;
            free(raw);
            raw = nullptr;
        }
    }
};

/* 全局变量声明 */
extern struct lex_token current_token;


/* 公共接口函数 */
/**
 * 获取下一个词法单元
 *
 * @param buf 用于存储词法单元的缓冲区
 * @return 1表示成功，0表示遇到错误或文件结束
 */
extern int lex_next(struct lex_token* buf);

#endif
