TARGET=generate
SRC_FILES=main.c parse.c generate.c database.c
SRC_DIR=Source
OBJ_DIR=.build
OBJ_FILES=$(SRC_FILES:.c=.o)
DEP_FILES := $(OBJ_FILES:.o=.d)
INCLUDE_DIRS=Source /usr/lib/llvm-14/include
LIBS=clang
LIB_DIRS=/usr/lib/llvm-14/lib .
CC=gcc
C_FLAGS=-g -O0 -Wextra

all: $(TARGET)

$(TARGET): $(addprefix $(OBJ_DIR)/,$(OBJ_FILES))
	$(CC) $(C_FLAGS) $(addprefix -L,$(LIB_DIRS)) $^ $(addprefix -l,$(LIBS)) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c Makefile
	@ mkdir -p $(dir @@)
	$(CC) $(C_FLAGS) $(addprefix -I,$(INCLUDE_DIRS)) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

-include $(addprefix $(OBJ_DIR)/,$(DEP_FILES))

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(TARGET)

re: fclean all

.PHONY: all clean fclean re


