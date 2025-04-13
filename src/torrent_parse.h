#pragma once
#include <stddef.h> // For size_t
#include <stdint.h> // For int64_t / long long

// --- Forward Declaration ---
// Pointer to the end of the buffer being parsed. Used for bounds checking.
extern const unsigned char* g_parse_end_ptr;

// --- Helper Functions ---

// Sets the global end pointer for bounds checking during parsing.
void set_parse_end_ptr(const unsigned char* end);

// Check if n bytes can be safely read from pointer p.
int is_safe_to_read(const unsigned char* p, size_t n);


// --- Bencode Parsing Functions ---

// Parse a bencoded string length without copying the string.
// Returns pointer after the ':', updates out_len. NULL on error or EOF.
const unsigned char* parse_bstr_len(const unsigned char* p, size_t* out_len);

// Parse a bencoded string into 'out' buffer. Max 'max_len' chars copied (incl null terminator).
// Returns pointer after the string data. NULL on error or EOF.
const unsigned char* parse_bstr(const unsigned char* p, char* out, size_t max_len);

// Parse a bencoded integer into 'out'.
// Returns pointer after the terminating 'e'. NULL on error or EOF.
const unsigned char* parse_int(const unsigned char* p, long long* out);

// Skip the next bencoded element (string, int, list, dict).
// Returns pointer after the skipped element. NULL on error or EOF.
const unsigned char* skip_bencoded(const unsigned char* p);

// Finds a key (null-terminated string) within a bencoded dictionary.
// Starts searching from *p. *p must point to the first key (string) after 'd'.
// If found, updates *p to point to the start of the *value* associated with the key.
// Returns 0 if found, -1 if not found or error.
int bencode_find_key(const unsigned char** p_ptr, const char* key);