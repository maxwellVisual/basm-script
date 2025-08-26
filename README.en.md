# Basm Script
A preprocessing scripting language for Basm, designed to build abstract syntax trees from character streams, but can also be used as a general-purpose programming language.

# Usage
## Runtime Dependencies
- libc

## Compilation Dependencies
- make  
- g++
- flex
- bison

## Compilation
```shell
git clone https://github.com/maxwellVisual/basm-script
cd basm-script
make -j$(nproc)
```

## Run Code File
```shell
./bscp <path/to/code.bs>
```

## Run CLI
```shell 
./bscp
```

# Examples
### Hello World
```basm-script
print("hello world!");
```

### Comments
```basm-script
# Unix-style single line comment
// C-style single line comment  
/* C-style multi-line comment */
/** C-style multi-line comment */
```

### Basic Data Types
| Type | Examples |
| --------- | ------- |
| number | 0, -1.5, 0xf1, 0b1, 076 |
| object | (){}, "a", \[1,2,3\] |
| field | foo, .0, .bar |

### Pseudo Data Types  
| Type | Definition | Example |
| -- | -- | -- |
| array | object<i: v> | \[1,2,3\] -> {.1=1;.2=2;.3=3;} |
| string | list<v> | "hello" -> \[104,101,108,108,111\] -> {.0=104;.1=101;.2=108;.3=108;.4=111;} |
| char | unicode num | 'a' = 97 |
| function | object w/out attributes | {printf();return 0;}

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
- Downward references use shared_ptr (e.g. obj.static_values)  
- Upward (reverse) references use weak_ptr (e.g. value.parent)
