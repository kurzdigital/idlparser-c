#ifndef __idlparser_h__
#define __idlparser_h__

struct IDLElement {
	const char *key;
	const char *value;
};
typedef struct IDLElement IDLElement;

struct IDL {
	const char *iin;
	IDLElement *elements;
	unsigned int count;
};
typedef struct IDL IDL;

void free_idl(struct IDL *);
int parse_idl(struct IDL *, const char *, unsigned int);

#ifdef IDL_PARSER_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

#define IDL_WHITE_SPACE "\t\r\n "
#define IDL_DIGITS "0123456789"
#define IDL_LETTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"

void free_idl(struct IDL *idl) {
	if (!idl) {
		return;
	}
	if (idl->iin) {
		free((void *) idl->iin);
		idl->iin = NULL;
	}
	if (idl->elements) {
		for (unsigned int i = 0; i < idl->count; ++i) {
			IDLElement *e = idl->elements + i;
			free((void *) e->key);
			free((void *) e->value);
		}
		free((void *) idl->elements);
		idl->elements = NULL;
		idl->count = 0;
	}
}

static char *idl_dup_trim(const char *s) {
	const char *trimmed = s + strspn(s, IDL_WHITE_SPACE);
	const char *last = trimmed;
	for (const char *p = trimmed; *p; ++p) {
		if (*p != ' ') {
			last = p + 1;
		}
	}
	return strndup(trimmed, last - trimmed);
}

static void idl_add(IDL *idl, const char *key, const char *value) {
	IDLElement *e = idl->elements + idl->count;
	e->key = strdup(key);
	e->value = idl_dup_trim(value);
	++idl->count;
}

static void idl_resolve_sex(char *value) {
	char letter = 0;
	switch (*value) {
	default: return;
	case '1': letter = 'M'; break;
	case '2': letter = 'F'; break;
	case '9': letter = 'X'; break;
	}
	*value = letter;
	*(++value) = 0;
}

static const char *idl_skip(const char *p, const char *end, const char *skip) {
	for (; p < end && strchr(skip, *p); ++p);
	return p;
}

static const char *idl_subtype(const char *s, const char *end,
		char (*code)[3]) {
	for (const char *p = s; p < end; ++p) {
		// Find a digit.
		if (!strchr(IDL_DIGITS, *p)) {
			continue;
		}
		const char *d = idl_skip(p, end, IDL_DIGITS);
		// Check if there are 8 or more consecutive digits.
		if (d - p < 8 || end - d < 3 ||
				// Check if this is followed by "DL" or "ID".
				(strncmp(d, "DL", 2) && strncmp(d, "ID", 2))) {
			continue;
		}
		strncpy(*code, d, 2);
		d += 2;
		while (end - d > 2) {
			// Skip over everything that is not a "D" or "I".
			for (; d < end && !strchr("DI", *d); ++d);
			if (end - d > 2 &&
					// Check if it is a "DL" or "ID".
					(!strncmp(d, "DL", 2) || !strncmp(d, "ID", 2))) {
				// Return pointer after second "DL|ID".
				return d + 2;
			}
		}
		break;
	}
	return s;
}

static const char *idl_find_iin(const char *s, const char *end,
		unsigned int *size) {
	const char *ansi = (char *) memmem(s, (end - s), "ANSI", 4);
	if (!ansi) {
		return NULL;
	}
	ansi += 4;
	const char *p = ansi;
	// Can't use strcspn() because s may not be NULL-terminated.
	p = idl_skip(p, end, IDL_WHITE_SPACE);
	if (p == ansi) {
		return NULL; // There need to be some white space after ANSI.
	}
	const char *iin = p;
	// The Issuer Identification Number (IIN) is at most 6 digits long.
	//for (int n = 6; n > 0 && p < end && strchr(IDL_DIGITS, *p); --n, ++p);
	p = idl_skip(p, end, IDL_DIGITS);
	*size = p - iin;
	if (*size > 6) {
		*size = 6;
	}
	return *size > 0 ? iin : NULL;
}

// Unfortunately, the format specified in
// http://www.aamva.org/DL-ID-Card-Design-Standard/
// does _not_ match the data I-Nigma (or ZXing) returns.
//
// The spec says, there must be a header of five bytes
// ('@', 0x0a, 0x1e, 0x0d, "ANSI ", ...) but the string
// we get does _not_ contain the characters 0x1e and 0x0d.
//
// Because of this inconsistency and because there seem
// to be deviating standards anyway (between states, and
// between USA and Canada, just have a look at
// https://github.com/googlesamples/android-vision/issues/77)
// the most simplest way to parse this is to use the
// field separator (LF) to split the string.
//
// Data elements come in the form "[ID][DATA]LF".
// Where [ID] is a three digit element identifier (like DCF, DCS...)
// and [DATA] is just a string.
//
// Each element is terminated by a line feed (LF = 0x10).
// Because the elements directly follow each other, each element
// is also prepended by a LF - except for the very first element
// what directly follows the subfile header (either DL for driver
// license or ID).
int parse_idl(IDL *idl, const char *s, unsigned int len) {
	if (!idl) {
		return 0;
	}
	memset(idl, 0, sizeof(IDL));
	if (!s || len < 1) {
		return 0;
	}
	const char *end = s + len;

	// Try to find the Issuer Identification Number (IIN).
	{
		unsigned int size = 0;
		const char *iin = idl_find_iin(s, end, &size);
		if (iin) {
			idl->iin = strndup(iin, size);
		}
	}

	idl->elements = (IDLElement *) calloc(len / 5, sizeof(IDLElement));
	if (!idl->elements) {
		free_idl(idl);
		return 0;
	}

	// Check for sub file pattern.
	{
		const char *p = idl_skip(s, end, IDL_WHITE_SPACE);
		if (*p == '@') {
			char code[] = {0,0,0};
			s = idl_subtype(p, end, &code);
			if (*code) {
				idl_add(idl, "DL", code);
			}
		}
	}

	// Split and collect key/value pairs.
	{
		const char *p = s;
		for (; p <= end; ++p) {
			// Wait until control character (LF).
			if (p < end && *p > 0x1f) {
				continue;
			}
			int vlen = (p - s) - 3;
			if (vlen > 0 &&
					// Check label name.
					strchr("DZ", *s) &&
					strchr(IDL_LETTERS, *(s + 1)) &&
					strchr(IDL_LETTERS, *(s + 2))) {
				char key[4];
				memset(&key, 0, sizeof(key));
				strncpy(key, s, 3);
				char value[vlen + 1];
				memset(&value, 0, sizeof(value));
				strncpy(value, s + 3, vlen);
				if (!strncmp(key, "DBC", 3)) {
					idl_resolve_sex((char *) &value);
				}
				idl_add(idl, key, value);
			}
			s = p + 1;
		}
	}

	if (!idl->count) {
		free_idl(idl);
		return 0;
	}

	return idl->count;
}
#endif // IDL_PARSER_IMPLEMENTATION

#endif
