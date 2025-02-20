/**
 * Original author: Serge Zaitsev <zaitsev.serge@gmail.com>
 * Event based stream parsing rewrite: Sakari Kapanen <sakari.m.kapanen@gmail.com>
 */
#ifndef __JSMN_STREAM_H_
#define __JSMN_STREAM_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Determines the maximal nesting level of the JSON */
#define JSMN_STREAM_MAX_DEPTH 32
/* Determines the maximal length a primitive or a string can have */
#define JSMN_STREAM_BUFFER_SIZE 256

typedef enum {
	JSMN_STREAM_TOKEN_START_OBJECT,
	JSMN_STREAM_TOKEN_END_OBJECT,
	JSMN_STREAM_TOKEN_START_ARRAY,
	JSMN_STREAM_TOKEN_END_ARRAY,
	JSMN_STREAM_TOKEN_KEY,
	JSMN_STREAM_TOKEN_STRING,
	JSMN_STREAM_TOKEN_PRIMITIVE,
} jsmn_stream_token_t;

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
	JSMN_STREAM_UNDEFINED = 0,
	JSMN_STREAM_OBJECT = 1,
	JSMN_STREAM_ARRAY = 2,
	JSMN_STREAM_STRING = 3,
	JSMN_STREAM_PRIMITIVE = 4,
	JSMN_STREAM_KEY = 5
} jsmn_streamtype_t;

enum jsmn_streamerr {
	/* Buffer not large enough */
	JSMN_STREAM_ERROR_NOMEM = -1,
	/* Invalid character inside JSON string */
	JSMN_STREAM_ERROR_INVAL = -2,
	/* The string is not a full JSON packet, more bytes expected */
	JSMN_STREAM_ERROR_PART = -3,
	/* Reached maximal stack depth (too deep nesting) */
	JSMN_STREAM_ERROR_MAX_DEPTH = -4
};

typedef enum {
    JSMN_STREAM_PARSING = 0,
    JSMN_STREAM_PARSING_STRING = 1,
    JSMN_STREAM_PARSING_PRIMITIVE = 2
} jsmn_streamstate_t;

typedef void (* jsmn_stream_callback_t)(
	jsmn_stream_token_t token,
	char const * buf,
	size_t len,
	void * user
);

/**
 * JSON parser. Stores the internal state of the parser and a necessary buffer
 * for parsing primitives.
 */
typedef struct {
    jsmn_streamstate_t state;
	jsmn_stream_callback_t callback; /* callbacks for parse events */
	jsmn_streamtype_t type_stack[JSMN_STREAM_MAX_DEPTH]; /* Stack for storing the type structure */
	size_t stack_height;
	char buffer[JSMN_STREAM_BUFFER_SIZE];
	size_t buffer_size;
	void *user_arg;
} jsmn_stream_parser;

/**
 * Create JSON parser given an array of event callbacks and an optional user
 * argument that is passed to the callbacks.
 */
void jsmn_stream_init(jsmn_stream_parser *parser,
	jsmn_stream_callback_t callback, void *user_arg);

/**
 * Run JSON parser. It incrementally parses a JSON string character by
 * character (so call this function repeatedly), calling the corresponding
 * callback whenever a parse event happens.
 */
int jsmn_stream_parse(jsmn_stream_parser *parser, char c);

#ifdef __cplusplus
}
#endif

#endif /* __JSMN_STREAM_H_ */
