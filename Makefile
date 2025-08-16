CXX ?= g++
CXXFLAGS ?= -Wall -g
YACC = bison
YACCFLAGS += -Lc++
LEX = flex
LEXFLAGS += -c++
# 目标文件
OBJS = bscp.tab.o bscp.yy.o bscp.o lex.o

# 默认目标
all: libbscp.a bscp
bscp.tab.cpp bscp.hpp: bscp.ypp
	$(YACC) $(YACCFLAGS) $< -o bscp.tab.cpp -Hbscp.hpp

# 编译目标文件
%.o: %.cpp bscp.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# 生成词法分析器
bscp.yy.cpp bscp.yy.hpp: bscp.lex
	$(LEX) $(LEXFLAGS) --header-file=bscp.yy.hpp -o bscp.yy.cpp $<

# 构建预处理器库
libbscp.a: $(OBJS)
	ar rcs $@ $(OBJS)

bscp: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

# 清理生成的文件
clean:
	rm -f $(OBJS) bscp.hpp bscp.tab.cpp libbscp.a stack.hh bscp.yy.cc bscp bscp.yy.cpp bscp.yy.hpp

.PHONY: all clean
