CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = src/lexer.c src/shell.c
OBJ = $(SRC:.c=.o)
TARGET = shell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
