
CC = @g++
LD = @g++
CFLAGS = -c -Wall -Wextra -Wwrite-strings
IFLAGS = -I/StdFuncs
LIBS = -lStdFuncs

ifdef DEBUG
	OBJ = Debug
	CFLAGS += -ggdb -D_DEBUG
else
	OBJ = Release
	CFLAGS += -O2
endif

LFLAGS = -L/StdFuncs/$(OBJ)

EXECUTABLE = $(OBJ)/BUBYFU

OBJECTS = $(OBJ)/BUBYFU.o $(OBJ)/Scanner.o

All: $(OBJ) $(EXECUTABLE)

$(OBJ):
	@MakeDir $(OBJ)

$(EXECUTABLE): $(OBJECTS) /StdFuncs/$(OBJ)/libStdFuncs.a
	@echo Linking $@...
	$(LD) $(OBJECTS) $(LFLAGS) $(LIBS) -lauto -o $@.debug
	@strip -R.comment $@.debug -o $@

$(OBJ)/%.o: %.cpp
	@echo Compiling $<...
	$(CC) $(CFLAGS) $(IFLAGS) -o $(OBJ)/$*.o $<

clean:
	@Delete $(OBJ) all quiet