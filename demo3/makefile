#==============================================================================
# Makefile for 2D Diffusion-Convection Solver
#==============================================================================

# 目录设置
SRCDIR   = src
INCDIR   = include

# 编译器设置
CC       = g++
CXXFLAGS = -Wall -std=c++17 -pedantic -O3 -march=native -fopenmp -I$(INCDIR)

# 目标可执行文件
TARGET   = df2d

# 源文件列表
SRCS     = $(SRCDIR)/util.cpp \
           $(SRCDIR)/solver.cpp \
           $(SRCDIR)/backend.cpp \
           $(SRCDIR)/cpu_backend.cpp \
           $(SRCDIR)/runtime.cpp \
           $(SRCDIR)/checkpoint.cpp \
           $(SRCDIR)/io.cpp \
           $(SRCDIR)/main.cpp

# 目标文件列表
OBJS     = $(SRCS:.cpp=.o)

# 头文件依赖
HEADERS  = $(wildcard $(INCDIR)/*.h)

#------------------------------------------------------------------------------
# 默认目标
#------------------------------------------------------------------------------
all: $(TARGET)
	@echo "Build complete: $(TARGET)"

#------------------------------------------------------------------------------
# 链接
#------------------------------------------------------------------------------
$(TARGET): $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) -lm

#------------------------------------------------------------------------------
# 编译规则
#------------------------------------------------------------------------------
$(SRCDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	$(CC) $(CXXFLAGS) -c $< -o $@

#------------------------------------------------------------------------------
# 清理
#------------------------------------------------------------------------------
clean:
	rm -f $(OBJS) *~

distclean: clean
	rm -f $(TARGET)

rebuild: distclean all

#------------------------------------------------------------------------------
# 格式化代码
#------------------------------------------------------------------------------
format:
	clang-format -i $(SRCDIR)/*.cpp $(INCDIR)/*.h

#------------------------------------------------------------------------------
# 清理运行结果
#------------------------------------------------------------------------------
clean-output:
	rm -rf data_* eta_ave_*.m remarks_*.m checkpoint_*.bin

#------------------------------------------------------------------------------
# 帮助
#------------------------------------------------------------------------------
help:
	@echo "Targets:"
	@echo "  all          - Build executable"
	@echo "  clean        - Remove object files"
	@echo "  distclean    - Remove objects and executable"
	@echo "  rebuild      - Clean and rebuild"
	@echo "  format       - Format source code"
	@echo "  clean-output - Remove simulation output files"

.PHONY: all clean distclean rebuild format clean-output help
