/**
 * Original author: Serge Zaitsev <zaitsev.serge@gmail.com>
 * Event based stream parsing rewrite: Sakari Kapanen <sakari.m.kapanen@gmail.com>
 */

#include "jsmn_stream.h"

#include <stdbool.h>

#define JSMN_STREAM_CALLBACK(f, ...) if ((f) != NULL) { (f)(__VA_ARGS__); }
#define JSMN_STREAM_CALLBACK_NULL(f, ty) if ((f) != NULL) { (f)(ty, NULL, 0, parser->user_arg); }

static bool jsmn_stream_stack_push(jsmn_stream_parser *parser, jsmn_streamtype_t type) {
	if (parser->stack_height >= JSMN_STREAM_MAX_DEPTH) {
		return false;
	}
	parser->type_stack[parser->stack_height++] = type;
	return true;
}

static void jsmn_stream_stack_pop(jsmn_stream_parser *parser) {
	// We don't need to return the popped item, so just decrement height.
	if (parser->stack_height > 0) parser->stack_height--;
}

static jsmn_streamtype_t jsmn_stream_stack_top(jsmn_stream_parser *parser) {
	if (parser->stack_height == 0) {
		return JSMN_STREAM_UNDEFINED;
	}
	return parser->type_stack[parser->stack_height - 1];
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_stream_parse_primitive(jsmn_stream_parser *parser, char cin) {
	/* Leave space for the terminating null character */
	if (parser->buffer_size == JSMN_STREAM_BUFFER_SIZE - 1) {
		return JSMN_STREAM_ERROR_NOMEM;
	}
	parser->buffer[parser->buffer_size++] = cin;
	size_t len = parser->buffer_size;
	const char *js = parser->buffer;
	for (int pos = 0; pos < len && js[pos] != '\0'; pos++) {
		switch (js[pos]) {
			case '\t' : case '\r' : case '\n' : case ' ' :
			case ','  : case ']'  : case '}' :
				goto found;
		}
		if (js[pos] < 32 || js[pos] >= 127) {
			return JSMN_STREAM_ERROR_INVAL;
		}
	}
	/* In strict mode primitive must be followed by a comma/object/array */
	return JSMN_STREAM_ERROR_PART;

found:
	parser->buffer[len - 1] = '\0';
	JSMN_STREAM_CALLBACK(parser->callback,
		JSMN_STREAM_TOKEN_PRIMITIVE, js, len - 1, parser->user_arg);
	parser->buffer_size = 0;
	parser->state = JSMN_STREAM_PARSING;
	return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_stream_parse_string(jsmn_stream_parser *parser, char cin) {
	/* Leave space for the terminating null character */
	if (parser->buffer_size == JSMN_STREAM_BUFFER_SIZE - 1) {
		return JSMN_STREAM_ERROR_NOMEM;
	}
	parser->buffer[parser->buffer_size++] = cin;
	size_t len = parser->buffer_size;
	const char *js = parser->buffer;
	for (int pos = 0; pos < len; pos++) {
		char c = js[pos];

		/* Quote: end of string */
		if (c == '\"') {
			parser->buffer[len - 1] = '\0';
			JSMN_STREAM_CALLBACK(parser->callback,
				jsmn_stream_stack_top(parser) == JSMN_STREAM_KEY ?
				JSMN_STREAM_TOKEN_STRING : JSMN_STREAM_TOKEN_KEY,
				js, len - 1, parser->user_arg);
			parser->buffer_size = 0;
			parser->state = JSMN_STREAM_PARSING;
			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if (c == '\\' && pos + 1 < len) {
			int i;
			pos++;
			switch (js[pos]) {
				/* Allowed escaped symbols */
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				/* Allows escaped symbol \uXXXX */
				case 'u':
					pos++;
					for(i = 0; i < 4 && pos < len && js[pos] != '\0'; i++) {
						/* If it isn't a hex character we have an error */
						if(!((js[pos] >= 48 && js[pos] <= 57) || /* 0-9 */
									(js[pos] >= 65 && js[pos] <= 70) || /* A-F */
									(js[pos] >= 97 && js[pos] <= 102))) { /* a-f */
							return JSMN_STREAM_ERROR_INVAL;
						}
						pos++;
					}
					pos--;
					break;
				/* Unexpected symbol */
				default:
					return JSMN_STREAM_ERROR_INVAL;
			}
		}
	}
	return JSMN_STREAM_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
int jsmn_stream_parse(jsmn_stream_parser *parser, char c) {
	jsmn_streamtype_t type;
	int r;

	switch (parser->state) {
		case JSMN_STREAM_PARSING:
			switch (c) {
				case '{': case '[':
					if (c == '{') {
						type = JSMN_STREAM_OBJECT;
						JSMN_STREAM_CALLBACK_NULL(parser->callback, JSMN_STREAM_TOKEN_START_OBJECT);
					} else {
						type = JSMN_STREAM_ARRAY;
						JSMN_STREAM_CALLBACK_NULL(parser->callback, JSMN_STREAM_TOKEN_START_ARRAY);
					}
					if (!jsmn_stream_stack_push(parser, type)) {
						return JSMN_STREAM_ERROR_MAX_DEPTH;
					}
					break;
				case '}': case ']':
					if (c == '}') {
						JSMN_STREAM_CALLBACK_NULL(parser->callback, JSMN_STREAM_TOKEN_END_OBJECT);
					} else {
						JSMN_STREAM_CALLBACK_NULL(parser->callback, JSMN_STREAM_TOKEN_END_ARRAY);
					}
					jsmn_stream_stack_pop(parser);
					if (jsmn_stream_stack_top(parser) == JSMN_STREAM_KEY) {
						jsmn_stream_stack_pop(parser);
					}
					break;
				case '\"':
					parser->state = JSMN_STREAM_PARSING_STRING;
					break;
				case '\t' : case '\r' : case '\n' : case ' ' : case ',':
					break;
				case ':':
					if (jsmn_stream_stack_top(parser) == JSMN_STREAM_OBJECT &&
						!jsmn_stream_stack_push(parser, JSMN_STREAM_KEY)) {
						return JSMN_STREAM_ERROR_MAX_DEPTH;
					}
					break;
				/* In strict mode primitives are: numbers and booleans */
				case '-': case '0': case '1' : case '2': case '3' : case '4':
				case '5': case '6': case '7' : case '8': case '9':
				case 't': case 'f': case 'n' :
					if (jsmn_stream_stack_top(parser) == JSMN_STREAM_OBJECT) {
						return JSMN_STREAM_ERROR_INVAL;
					}
					parser->state = JSMN_STREAM_PARSING_PRIMITIVE;
					jsmn_stream_parse(parser, c);
					break;

				/* Unexpected char in strict mode */
				default:
					return JSMN_STREAM_ERROR_INVAL;
			}
			break;

		case JSMN_STREAM_PARSING_STRING:
			r = jsmn_stream_parse_string(parser, c);
			if (r < 0) return r;
			if (jsmn_stream_stack_top(parser) == JSMN_STREAM_KEY) {
				jsmn_stream_stack_pop(parser);
			}
			break;

		case JSMN_STREAM_PARSING_PRIMITIVE:
			r = jsmn_stream_parse_primitive(parser, c);
			if (r < 0) return r;
			else if (r == 0) {
				if (jsmn_stream_stack_top(parser) == JSMN_STREAM_KEY) {
					jsmn_stream_stack_pop(parser);
				}
				return jsmn_stream_parse(parser, c);
			}
			break;
	}

	return 0;
}

/**
 * Creates a new parser based over a given  buffer with an array of tokens
 * available.
 */
void jsmn_stream_init(jsmn_stream_parser *parser,
	jsmn_stream_callback_t callback, void *user_arg) {
	parser->state = JSMN_STREAM_PARSING;
	parser->stack_height = 0;
	parser->buffer_size = 0;
	parser->callback = *callback;
	parser->user_arg = user_arg;
}

