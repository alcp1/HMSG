# My makefile

# Name of the project - HAPCAN, MQTT, and Socket Gateway
PROJ_NAME=HMSG

# .c files
C_SOURCE=$(wildcard ./source/*.c)

# .h files
H_SOURCE=$(wildcard ./source/*.h)

# Object files
OBJ=$(subst .c,.o,$(subst source,objects,$(C_SOURCE)))

# Compiler and linker
CC=gcc

# Netbeans original compiler options
# gcc -lpaho-mqtt3c -pthread   -c -g -MMD -MP -MF "build/Debug/GNU-Linux/manager.o.d" -o build/Debug/GNU-Linux/manager.o manager.c

# Flags for compiler
CC_FLAGS=-c               \
         -lpaho-mqtt3c    \
         -pthread         \
         -Wall            \
         -ljson-c         \
         -g               \

LDFLAGS = -lpaho-mqtt3c -pthread -ljson-c -g

# Command used at clean target
RM = rm -rf

#
# Compilation and linking
#
all: objFolder $(PROJ_NAME)

$(PROJ_NAME): $(OBJ)
	@ echo 'Building binary using GCC linker: $@'
	$(CC) $^ $(LDFLAGS) -o $@
	@ echo 'Finished building binary: $@'
	@ echo ' '

./objects/%.o: ./source/%.c ./source/%.h
	@ echo 'Building target using GCC compiler: $<'
	$(CC) $< $(CC_FLAGS) -o $@
	@ echo ' '

./objects/main.o: ./source/main.c $(H_SOURCE)
	@ echo 'Building target using GCC compiler: $<'
	$(CC) $< $(CC_FLAGS) -o $@
	@ echo ' '

objFolder:
	@ mkdir -p objects

clean:
	@ $(RM) ./objects/*.o $(PROJ_NAME) *~
	@ rmdir objects

.PHONY: all clean
