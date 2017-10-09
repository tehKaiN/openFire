# Windows version of VBCC requires absolute path in all .h files
# e.g. timer manager has to refer to timer.h by absolute path

# TODO: refactor so 'make' without args won't recompile whole ACE

# Multi-platform
ifdef ComSpec
	# Windows
	RM = del
	CP = copy
	SLASH = \\
	CURR_DIR=$(shell chdir)
else
	# Linux/Amiga
	RM = rm
	CP = cp
	SLASH = /
	CURR_DIR = $(shell pwd)
endif
SL= $(strip $(SLASH))
SRC_DIR = $(CURR_DIR)$(SL)src

# Directories
TMP_DIR = $(SRC_DIR)$(SL)tmp
ACE_DIR = ..$(SL)ace$(SL)ace
ACE_PARENT = $(ACE_DIR)$(SL)..

# Compiler stuff
CC = vc
CC_FLAGS = +kick13 -c99 -O1 -I$(SRC_DIR) -I$(ACE_PARENT)

# File list
OF_MAIN_FILES = $(wildcard $(SRC_DIR)/*.c)
OF_MAIN_OBJS = $(addprefix $(TMP_DIR)$(SL), $(notdir $(OF_MAIN_FILES:.c=.o)))

OF_GS_GAME_FILES = $(wildcard $(SRC_DIR)/gamestates/game/*.c)
OF_GS_GAME_OBJS = $(addprefix $(TMP_DIR)$(SL)gsgame_, $(notdir $(OF_GS_GAME_FILES:.c=.o)))

OF_GS_MENU_FILES = $(wildcard $(SRC_DIR)/gamestates/menu/*.c)
OF_GS_MENU_OBJS = $(addprefix $(TMP_DIR)$(SL)gsmenu_, $(notdir $(OF_GS_MENU_FILES:.c=.o)))

OF_GS_PRECALC_FILES = $(wildcard $(SRC_DIR)/gamestates/precalc/*.c)
OF_GS_PRECALC_OBJS = $(addprefix $(TMP_DIR)$(SL)gsprecalc_, $(notdir $(OF_GS_PRECALC_FILES:.c=.o)))

OF_FILES = $(OF_MAIN_FILES) $(OF_GS_GAME_FILES) $(OF_GS_MENU_FILES) $(OF_GS_PRECALC_FILES)
OF_OBJS = $(OF_MAIN_OBJS) $(OF_GS_GAME_OBJS) $(OF_GS_MENU_OBJS) $(OF_GS_PRECALC_OBJS)
ACE_OBJS = $(wildcard $(ACE_DIR)/tmp/*.o)

#
ace: $(ACE_OBJS)
	make -C $(ACE_DIR) all
	@echo.
	@echo Copying ACE objs:
	$(CP) $(ACE_DIR)$(SL)tmp$(SL)*.o $(TMP_DIR)
	@echo.

of: $(OF_OBJS)
	@echo.
	@echo Linking...
	$(CC) $(CC_FLAGS) -lamiga -o $@ $^ $(ACE_OBJS)

# Main files
$(TMP_DIR)$(SL)%.o: $(SRC_DIR)/%.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

# Game
$(TMP_DIR)$(SL)gsgame_%.o: $(SRC_DIR)/gamestates/game/%.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

# Menu
$(TMP_DIR)$(SL)gsmenu_%.o: $(SRC_DIR)/gamestates/menu/%.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

# Precalc
$(TMP_DIR)$(SL)gsprecalc_%.o: $(SRC_DIR)/gamestates/precalc/%.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

all: clear ace of

clear:
	$(RM) $(TMP_DIR)$(SL)*.o
