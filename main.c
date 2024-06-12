#define IDL_PARSER_IMPLEMENTATION
#include "idlparser.h"

#include <stdio.h>

int main(void) {
	char buf[4096];
	char *p = buf;
	for (size_t left = sizeof(buf), read = 0; left > 0; ) {
		read = fread(p, sizeof(char), left, stdin);
		left -= read;
		p += read;
		if (read != left) {
			break;
		}
	}
	IDL idl;
	if (!parse_idl(&idl, buf, p - buf)) {
		fprintf(stderr, "ERROR!\n");
		free_idl(&idl);
		return -1;
	}
	printf("IIN:[%s]\n", idl.iin);
	for (unsigned int i = 0; i < idl.count; ++i) {
		IDLElement *e = idl.elements + i;
		printf("%s:[%s]\n", e->key, e->value);
	}
	free_idl(&idl);
	return 0;
}
