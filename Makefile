CC = cc
CFLAGS = -Wall -Wextra -Werror -pedantic
LFLAGS = -ldumb -lportaudio
OBJS = modplay.c
OBJ_NAME = modplay

INSTALL_DIR = /usr/local

all: $(OBJS)
	$(CC) $(OBJS) $(CFLAGS) $(LFLAGS) -o $(OBJ_NAME)

clean:
	rm -f $(OBJ_NAME)

install: $(OBJS)
	install -Dm 755 $(OBJ_NAME) $(INSTALL_DIR)/bin
#install -Dm 644 modplay.1 $(INSTALL_DIR)/man/man1

uninstall:
	rm -f $(INSTALL_DIR)/bin/$(OBJ_NAME) #$(INSTALL_DIR)/man/man1/modplay.1
