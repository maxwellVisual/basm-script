# Basm Script
Basm 语言的预处理脚本语言，设计来从字符流构建抽象语法树，但也可以作为通用编程语言使用。

# 使用
## 运行依赖
- libc

## 编译依赖
- make
- g++
- flex
- bison

## 编译
```shell
git clone https://github.com/maxwellVisual/basm-script
cd basm-script
make -j$(nproc)
```
## 运行代码文件
```shell
./bscp <path/to/code.bs>
```
## 运行CLI
```shell
./bscp
```

# 语法示例
### Hello World
```basm-script
print("hello world!");
```
### 注释
```basm-script
# unix风格的单行注释
// c风格的单行注释
/* c风格的多行注释 */
/** c风格的多行注释 */
```
### 基本数据类型
| 类型 | 例子 |
| --------- | ------- |
| 数字 | 0, -1.5, 0xf1, 0b1, 076 |
| 对象 | (){}, "a", \[1,2,3\] |
| 属性 | foo, .0, .bar |

### 伪数据类型
| 类型 | 定义 | 例子 |
| -- | -- | -- |
| 数组 | object<i: v> | \[1,2,3\] -> {.1=1;.2=2;.3=3;} |
| 字符串 | list<v> | "hello" -> \[104,101,108,108,111\] -> {.0=104;.1=101;.2=108;.3=108;.4=111;} |
| 字符 | unicode num | 'a' = 97 |
| 方法 | object w/out attributes | {printf();return 0;}

# Development
## Notice
### char width (char vs. wchar_t)
| domain | type | reason |
| -- | -- | -- |
| stdout | wchar_t | unicode compatibility |
| stderr | char | debug simplicity |
| stdin | char | flex lexer compatibility |
| bscp::script | wchar_t | unicode compatibility |
| yy::parser | wchar_t | unicode compatibility |
| FlexLexer & yyFlexLexer | char | wchar_t not supported by flex |
| args & main | char | standard c++ compatibility |

\***don't** mix different char types!
### Smart Pointers in AST
- 向下引用shared_ptr (如obj.static_values)
- 向上（反向）引用weak_ptr (如value.parent)