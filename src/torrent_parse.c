#define _CRT_SECURE_NO_WARNINGS // If needed for fopen, strcpy etc.
#include "torrent_parse.h"
#include "debug.h" // Assuming DebugOut is defined elsewhere as provided
#include <string.h>
#include <stdlib.h> // For malloc/free (potentially useful here if needed, though not strictly required by funcs below)
#include <stdio.h>  // For sprintf if needed for debugging

// --- Global for Bounds Checking ---
const unsigned char* g_parse_end_ptr = NULL;

void set_parse_end_ptr(const unsigned char* end)
{
    g_parse_end_ptr = end;
}

// --- Helper ---
// Check bounds before reading. Need g_parse_end_ptr to be set.
int is_safe_to_read(const unsigned char* p, size_t n)
{
    // Ensure g_parse_end_ptr is set before calling this!
    if(!g_parse_end_ptr)
    {
        DebugOut("Error: g_parse_end_ptr not set for bounds checking!\n");
        return 0; // Indicate unsafe / error
    }
    // Check for null pointer AND potential overflow when calculating end read point
    if(!p || (size_t)(g_parse_end_ptr - p) < n)
    {
        return 0; // Not safe
    }
    // Consider adding check if p itself is >= g_parse_end_ptr already?
    if(p >= g_parse_end_ptr) return 0;

    return 1; // Safe
}


// --- Implementations ---

// Parse string length: <len>:<data>
const unsigned char* parse_bstr_len(const unsigned char* p, size_t* out_len)
{
    if(!is_safe_to_read(p, 1) || !(*p >= '0' && *p <= '9')) return NULL;

    *out_len = 0;
    const unsigned char* start_len = p;
    while(is_safe_to_read(p, 1) && *p >= '0' && *p <= '9')
    {
        *out_len = *out_len * 10 + (*p - '0');
        p++;
    }
    if(!is_safe_to_read(p, 1) || *p != ':') return NULL;
    return p + 1; // Return pointer *after* the colon
}

// Parse string: <len>:<data>
const unsigned char* parse_bstr(const unsigned char* p, char* out, size_t max_len)
{
    size_t len;
    const unsigned char* data_start = parse_bstr_len(p, &len);
    if(!data_start) return NULL;

    // Check if the *entire* string data is within bounds before proceeding
    if(!is_safe_to_read(data_start, len)) return NULL;

    if(max_len > 0)
    {
        size_t copy_len = (len >= max_len) ? max_len - 1 : len;
        memcpy(out, data_start, copy_len);
        out[copy_len] = '\0';
    }

    return data_start + len; // Return pointer after the actual string data
}

// Parse integer: i<integer>e
const unsigned char* parse_int(const unsigned char* p, long long* out)
{
    if(!is_safe_to_read(p, 1) || *p != 'i') return NULL;
    p++; // Skip 'i'

    *out = 0;
    int sign = 1;

    if(is_safe_to_read(p, 1) && *p == '-')
    {
        sign = -1;
        p++;
    }

    // Need at least one digit
    if(!is_safe_to_read(p, 1) || !(*p >= '0' && *p <= '9')) return NULL;

    while(is_safe_to_read(p, 1) && *p >= '0' && *p <= '9')
    {
        *out = *out * 10 + (*p - '0');
        p++;
    }

    if(!is_safe_to_read(p, 1) || *p != 'e') return NULL;

    *out *= sign;
    return p + 1; // Skip 'e'
}

// Skip any bencoded value
const unsigned char* skip_bencoded(const unsigned char* p)
{
    if(!is_safe_to_read(p, 1)) return NULL;

    // Integer: i...e
    if(*p == 'i')
    {
        p++; // Skip 'i'
        // Allow optional '-' sign
        if(is_safe_to_read(p, 1) && *p == '-') p++;
        // Need at least one digit
        if(!is_safe_to_read(p, 1) || !(*p >= '0' && *p <= '9')) return NULL;
        while(is_safe_to_read(p, 1) && *p >= '0' && *p <= '9')
        {
            p++;
        }
        if(!is_safe_to_read(p, 1) || *p != 'e') return NULL;
        return p + 1; // Skip 'e'
    }
    // String: <len>:<data>
    else if(*p >= '0' && *p <= '9')
    {
        size_t len;
        const unsigned char* data_start = parse_bstr_len(p, &len);
        if(!data_start) return NULL;
        // Check if string data is within bounds before skipping past it
        if(!is_safe_to_read(data_start, len)) return NULL;
        return data_start + len;
    }
    // List: l...e or Dictionary: d...e
    else if(*p == 'l' || *p == 'd')
    {
        unsigned char terminator = (*p == 'l') ? 'l' : 'd';
        p++; // Skip 'l' or 'd'
        while(is_safe_to_read(p, 1) && *p != 'e')
        {
            // For dictionaries, skip key (string) then value
            if(terminator == 'd')
            {
                p = skip_bencoded(p); // Skip key (must be a string)
                if(!p) return NULL;
                if(!is_safe_to_read(p, 1)) return NULL; // Need space for value or 'e'
            }
            p = skip_bencoded(p); // Skip value (list item or dict value)
            if(!p) return NULL;
        }
        if(!is_safe_to_read(p, 1) || *p != 'e') return NULL;
        return p + 1; // Skip 'e'
    }

    // Unknown type or end of buffer reached unexpectedly
    return NULL;
}


// Find key in dictionary. *p_ptr points to the first key string *inside* the dict.
// Updates *p_ptr to point to the start of the *value* if found.
// Assumes dictionary structure is valid (alternating string keys and values).
int bencode_find_key(const unsigned char** p_ptr, const char* key)
{
    const unsigned char* p = *p_ptr;
    char current_key[256]; // Buffer for reading keys

    if(!is_safe_to_read(p, 1)) return -1; // Need at least 'e'

    while(is_safe_to_read(p, 1) && *p != 'e')
    {
        // 1. Parse the key (must be a string)
        const unsigned char* value_ptr = parse_bstr(p, current_key, sizeof(current_key));
        if(!value_ptr)
        {
            DebugOut("bencode_find_key: Failed to parse key string.\n");
            return -1; // Error parsing key
        }

        // 2. Compare with target key
        if(strcmp(current_key, key) == 0)
        {
            *p_ptr = value_ptr; // Found! Update caller's pointer to the start of the value
            return 0;           // Success
        }

        // 3. Not a match, skip the corresponding value
        p = skip_bencoded(value_ptr);
        if(!p)
        {
            DebugOut("bencode_find_key: Failed to skip value for key '%s'.\n", current_key);
            return -1; // Error skipping value
        }
    }

    // Reached 'e' or end of buffer without finding the key
    if(is_safe_to_read(p, 1) && *p == 'e')
    {
        // Normal end of dictionary, key not found
    }
    else if(!is_safe_to_read(p, 1))
    {
        DebugOut("bencode_find_key: Unexpected end of buffer searching for key '%s'.\n", key);
    }
    else
    {
        DebugOut("bencode_find_key: Malformed dictionary structure searching for key '%s'. Unexpected token '%c'.\n", key, *p);
    }
    return -1; // Not found or error
}