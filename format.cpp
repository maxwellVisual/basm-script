#include "bscp.hpp"

#include <memory>
#include <queue>



namespace bscp::script 
{
template<typename _CharT>
escape_stream<_CharT>::escape_stream(const std::basic_string<_CharT> &input): 
    state(state_t::S_NORMAL)
{
    put(input);
}

template<typename _CharT>
escape_stream<_CharT>::escape_stream(): 
        state(state_t::S_NORMAL)
{
}

template<typename _CharT>
void escape_stream<_CharT>::put(const _CharT* str, std::streamsize n){
    for(std::streamsize i = 0; i < n; i++) {
        obuf.push(str[i]);
    }
}
template<typename _CharT>
void escape_stream<_CharT>::put(const _CharT& c){
    obuf.push(c);
}
template<typename _CharT>
void escape_stream<_CharT>::put(const std::basic_string<_CharT> str){
    for(auto& c : str) {
        obuf.push(c);
    }
}
template<typename _CharT>
void escape_stream<_CharT>::step(_CharT &c){
    // 处理结束时的状态
    if(c == EOF){
        state_t old_state = state;
        state = state_t::S_NORMAL;
        
        switch (old_state) {
            case state_t::S_ESCAPE:
                // 以反斜杠结尾，输出反斜杠
                ibuf.push(L'\\');
                break;
            case state_t::S_HEX_FIRST:
                // 以\x结尾，输出x
                ibuf.push(L'x');
                break;
            case state_t::S_HEX_CONT:
                // 解析并输出十六进制值
                if (!escape_str.empty()) {
                    wchar_t hex_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(hex_val);
                }
                break;
            case state_t::S_UNICODE_U:
                // 输出u和已解析的部分
                ibuf.push(L'u');
                if (!escape_str.empty()) {
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(unicode_val);
                }
                break;
            case state_t::S_UNICODE_UU:
                // 输出U和已解析的部分
                ibuf.push(L'U');
                if (!escape_str.empty()) {
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(unicode_val);
                }
                break;
            case state_t::S_OCTAL:
                // 解析并输出八进制值
                if (!escape_str.empty()) {
                    wchar_t oct_val = std::wcstoul(escape_str.c_str(), nullptr, 8);
                    ibuf.push(oct_val);
                }
                break;
            default:
                break;
        }
    }

    switch (state)
    {
    case state_t::S_NORMAL:
        if (c == L'\\') {
            state = state_t::S_ESCAPE;
        } else {
            ibuf.push(c);
        }
        break;

    case state_t::S_ESCAPE:
        switch (c) {
            case L'\'':
                ibuf.push(L'\'');
                state = state_t::S_NORMAL;
                break;
            case L'\"':
                ibuf.push(L'\"');
                state = state_t::S_NORMAL;
                break;
            case L'\?':
                ibuf.push(L'?');
                state = state_t::S_NORMAL;
                break;
            case L'\\':
                ibuf.push(L'\\');
                state = state_t::S_NORMAL;
                break;
            case L'a':
                ibuf.push(L'\a');
                state = state_t::S_NORMAL;
                break;
            case L'b':
                ibuf.push(L'\b');
                state = state_t::S_NORMAL;
                break;
            case L'f':
                ibuf.push(L'\f');
                state = state_t::S_NORMAL;
                break;
            case L'n':
                ibuf.push(L'\n');
                state = state_t::S_NORMAL;
                break;
            case L'r':
                ibuf.push(L'\r');
                state = state_t::S_NORMAL;
                break;
            case L't':
                ibuf.push(L'\t');
                state = state_t::S_NORMAL;
                break;
            case L'v':
                ibuf.push(L'\v');
                state = state_t::S_NORMAL;
                break;
            case L'x':
                state = state_t::S_HEX_FIRST;
                escape_str.clear();
                break;
            case L'u':
                state = state_t::S_UNICODE_U;
                escape_str.clear();
                break;
            case L'U':
                state = state_t::S_UNICODE_UU;
                escape_str.clear();
                break;
            case L'0': case L'1': case L'2': case L'3': case L'4':
            case L'5': case L'6': case L'7':
                state = state_t::S_OCTAL;
                escape_str.clear();
                escape_str += c;
                break;
            default:
                ibuf.push(c);
                state = state_t::S_NORMAL;
                break;
            }
            break;

        case state_t::S_HEX_FIRST:
            if (std::isxdigit(c)) {
                escape_str += c;
                state = state_t::S_HEX_CONT;
            } else {
                // 如果不是十六进制字符，就打印'x'并处理当前字符
                for(auto ch : L"x") ibuf.push(ch);
                if (c == L'\\') {
                    state = state_t::S_ESCAPE;
                } else {
                    ibuf.push(c);
                    state = state_t::S_NORMAL;
                }
            }
            break;

        case state_t::S_HEX_CONT:
            if (std::isxdigit(c) && escape_str.length() < 8) {
                escape_str += c;
                // 继续读取更多十六进制字符
            } else {
                // 遇到非十六进制字符，解析并输出十六进制值
                if (!escape_str.empty()) {
                    wchar_t hex_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(hex_val);
                }
                if (c == L'\\') {
                    state = state_t::S_ESCAPE;
                } else {
                    ibuf.push(c);
                    state = state_t::S_NORMAL;
                }
            }
            break;

        case state_t::S_UNICODE_U:
            if (std::isxdigit(c)) {
                escape_str += c;
                if (escape_str.length() >= 4) {
                    // 已经读取了4个十六进制字符
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(unicode_val);
                    state = state_t::S_NORMAL;
                }
            } else {
                // 如果不是有效的十六进制字符，就打印'u'和已解析的字符
                for(auto ch : L"u") ibuf.push(ch);
                if (!escape_str.empty()) {
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(unicode_val);
                }
                if (c == L'\\') {
                    state = state_t::S_ESCAPE;
                } else {
                    ibuf.push(c);
                    state = state_t::S_NORMAL;
                }
            }
            break;

        case state_t::S_UNICODE_UU:
            if (std::isxdigit(c)) {
                escape_str += c;
                if (escape_str.length() >= 8) {
                    // 已经读取了8个十六进制字符
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(unicode_val);
                    state = state_t::S_NORMAL;
                }
            } else {
                // 如果不是有效的十六进制字符，就打印'U'和已解析的字符
                ibuf.push(L'U');
                if (!escape_str.empty()) {
                    wchar_t unicode_val = std::wcstoul(escape_str.c_str(), nullptr, 16);
                    ibuf.push(unicode_val);
                }
                if (c == L'\\') {
                    state = state_t::S_ESCAPE;
                } else {
                    ibuf.push(c);
                    state = state_t::S_NORMAL;
                }
            }
            break;

        case state_t::S_OCTAL:
            if (c >= L'0' && c <= L'7' && escape_str.length() < 11) {
                escape_str += c;
                // 继续读取更多八进制字符
            } else {
                // 遇到非八进制字符，解析并输出八进制值
                if (!escape_str.empty()) {
                    wchar_t oct_val = std::wcstoul(escape_str.c_str(), nullptr, 8);
                    ibuf.push(oct_val);
                }
                if (c == L'\\') {
                    state = state_t::S_ESCAPE;
                } else {
                    ibuf.push(c);
                    state = state_t::S_NORMAL;
                }
            }
            break;
    }
}

template<typename _CharT>
_CharT escape_stream<_CharT>::next(){
    // 先处理输出缓冲
    if(!ibuf.empty()){
        _CharT i = ibuf.front();
        ibuf.pop();
        return i;
    }

    if(obuf.empty()){
        return EOF;
    }
    _CharT c;
    
    do{
        c = obuf.front();
        obuf.pop();
        step(c);
        if(!ibuf.empty()){
            c = ibuf.front();
            ibuf.pop();
            return c;
        }
    }while(!obuf.empty());

    return _CharT(EOF);
}


template<typename _CharT>
void escape_stream<_CharT>::get(std::basic_string<_CharT> &buf){
    _CharT c;
    while((c = next()) != EOF){
        buf.push_back(c);
    }
}
template<typename _CharT>
void escape_stream<_CharT>::get(wchar_t &buf){
    buf = next();
}

std::shared_ptr<obj> format_str_const(const std::wstring &str){
    escape_stream<wchar_t> s(str);
    std::wstring out;
    s.get(out);

    auto obj = std::make_shared<bscp::script::obj>(nullptr);
    for(size_t i = 0; i < out.length(); i++) {
        std::wstring name = std::to_wstring(i);
        obj->static_fields.emplace(name, std::shared_ptr<bscp::script::value>(new bscp::script::num(out[i], obj)));
    }
    return std::shared_ptr<bscp::script::obj>(obj);
}
std::shared_ptr<num> format_chr_const(const std::wstring &str){
    escape_stream<wchar_t> s(str);
    wchar_t out;
    s.get(out);
    return std::make_shared<num>((long double)out);
}
std::shared_ptr<value> call_print(std::shared_ptr<value> &msg_val, std::wostream& wout){
    escape_stream<wchar_t> s;

    if(auto num = std::dynamic_pointer_cast<bscp::script::num>(msg_val)){
        wout<<num->value;
        return std::shared_ptr<value>(new null());
    }

    std::shared_ptr<obj> msg_obj = std::dynamic_pointer_cast<obj>(msg_val);
    if(!msg_obj){
        return std::shared_ptr<value>(new num(0));
    }
    for (size_t i = 0; msg_obj->static_fields.find(std::to_wstring(i)) != msg_obj->static_fields.end(); i++){
        std::shared_ptr<bscp::script::num> num = std::dynamic_pointer_cast<bscp::script::num>((*msg_obj)[i]);
        if(!num || num->value < 0){
            continue;
        }
        wout<<wchar_t(num->value);
    }
    return std::shared_ptr<value>(new null());
}


} // namespace bscp::script
