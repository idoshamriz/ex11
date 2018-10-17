CC=gcc
AR_COM=ar rcs
LIB_FLAGS = -Wall -Werror -c
LIBS=libbinsem.a libut.a
PH=ph
OUTPUT_FLAGS = -Wall -Werror -L./
CLEAN=clean

all: $(LIBS) $(PH) clean

$(PH): $(PH).o $(LIBS)
	@echo "Creating ${PH}"
	@$(CC) ${OUTPUT_FLAGS} $^ -o $@

%.o: %.c
	@$(CC) $(LIB_FLAGS) $^ -o $@

lib%.a: %.o
	@echo "Creating Library $@"
	@$(AR_COM) $@ $^
	@ranlib $@

clean:
	rm -f $(LIBS) *.o
