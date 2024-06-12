BIN = parser
OBJECTS = main.o
CFLAGS = -O2 -Wall -Wextra -pedantic -std=c99

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(BIN)
	for F in samples/*.idl; do ./$(BIN) < "$$F" | diff - "$${F%.*}.txt"; done

$(BIN): $(OBJECTS) idlparser.h
	$(CC) -o $@ $(OBJECTS)

clean:
	rm -f *.o $(BIN)
