#define _CRT_SECURE_NO_WARNINGS
#include "torrent.h"
#include "torrent_parse.h"
#include "debug.h"
#include <stdio.h>
#include <stdlib.h> // For malloc, free
#include <string.h> // For memcpy, memset, strcmp
#include <stdint.h> // For standard types

// Function to free dynamically allocated memory within a Torrent struct
void torrent_free(Torrent* t)
{
    if(t)
    {
        free(t->pieces); // Free the piece hashes buffer
        t->pieces = NULL; // Avoid double free
        t->piece_count = 0;
        // Files array is part of the struct, no separate free needed here
    }
}

// Helper to read entire file into a dynamically allocated buffer
// Returns pointer to buffer, sets 'out_size'. Returns NULL on error.
// Caller must free the returned buffer.
static unsigned char* read_entire_file(const char* path, size_t* out_size)
{
    FILE* f = fopen(path, "rb");
    if(!f)
    {
        DebugOut("read_entire_file: Failed to open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size_long = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Check for error from ftell or zero size (which can be valid, but often not for torrents)
    if(file_size_long < 0)
    {
        DebugOut("read_entire_file: Failed to get file size (ftell error) for %s\n", path);
        fclose(f);
        return NULL;
    }
    if(file_size_long == 0)
    {
        DebugOut("read_entire_file: Warning: File size is 0 for %s\n", path);
        // Allow proceeding with empty buffer, caller might handle it or fail later.
        // Alternative: return NULL here if 0-byte files are always errors.
    }

    size_t file_size = (size_t)file_size_long;
    unsigned char* buffer = NULL;

    // Avoid malloc(0) which has implementation-defined behavior
    if(file_size > 0)
    {
        buffer = (unsigned char*)malloc(file_size);
        if(!buffer)
        {
            DebugOut("read_entire_file: Failed to allocate %zu bytes for %s\n", file_size, path);
            fclose(f);
            return NULL;
        }

        size_t bytes_read = fread(buffer, 1, file_size, f);
        if(bytes_read != file_size)
        {
            DebugOut("read_entire_file: Failed to read %zu bytes from %s (read %zu)\n", file_size, path, bytes_read);
            free(buffer); // Free partially allocated buffer on read error
            fclose(f);
            return NULL;
        }
        DebugOut("read_entire_file: Read %zu bytes from %s\n", bytes_read, path);
    }
    else
    {
        // If file_size is 0, we don't need to allocate or read. Return a NULL or placeholder if necessary?
        // Let's return NULL consistently with allocation failure for zero size, as torrents usually aren't empty.
        // Or we could malloc a zero-byte buffer if the caller expects non-NULL? Sticking to NULL for simplicity.
        buffer = NULL; // Explicitly NULL for 0 size
        DebugOut("read_entire_file: Returning NULL for 0-byte file %s\n", path);
        fclose(f); // Still need to close the file
        // Set out_size to 0 before returning NULL due to 0 file size.
        if(out_size) *out_size = 0;
        return NULL;
    }


    fclose(f); // Close file on success too!

    *out_size = file_size;
    return buffer;
}


int torrent_load(const char* path, Torrent* t)
{
    size_t size = 0; // Initialize size
    unsigned char* file_buffer = read_entire_file(path, &size);
    if(!file_buffer)
    {
        // read_entire_file already printed the error
        // If size was 0, this branch is also taken.
        return -1; // Indicate file read error or empty file
    }

    // --- Crucial Check: Ensure we don't parse if size is 0, even if buffer somehow wasn't NULL ---
    if(size == 0)
    {
        DebugOut("torrent_load: Cannot parse empty file content for %s.\n", path);
        free(file_buffer); // Free the (likely NULL) buffer just in case.
        return -1;
    }

    // Set the global end pointer for bounds checking in parse functions
    set_parse_end_ptr(file_buffer + size);

    // Initialize Torrent struct
    memset(t, 0, sizeof(*t));
    t->pieces = NULL; // Ensure pieces pointer is NULL initially

    const unsigned char* p = file_buffer;
    const unsigned char* end = file_buffer + size; // Keep 'end' for local checks if needed. Rely on is_safe_to_read.
    int result = -1; // Default to error


    // --- Sanity check: Must start with 'd' (dictionary) ---
    if(!is_safe_to_read(p, 1) || *p != 'd')
    {
        DebugOut("torrent_load: Torrent file does not start with dictionary 'd'.\n");
        goto cleanup;
    }
    p++; // Skip 'd'


    // --- Find 'announce' Key ---
    // IMPORTANT: bencode_find_key modifies the pointer it's passed.
    // Make a copy if you need to restart the search from the beginning later.
    const unsigned char* search_ptr = p;
    if(bencode_find_key(&search_ptr, "announce") != 0) // Modifies search_ptr
    {
        DebugOut("torrent_load: Optional key 'announce' not found.\n");
        // It's often optional if 'announce-list' exists. Don't make it fatal.
    }
    else
    {
        // search_ptr now points to the *start* of the announce value.
        const unsigned char* next_p = parse_bstr(search_ptr, t->announce, sizeof(t->announce));
        if(!next_p)
        {
            DebugOut("torrent_load: Failed to parse 'announce' value.\n");
            goto cleanup;
        }
        DebugOut("torrent_load: announce = %s\n", t->announce);
    }
    // We reset the search pointer below for 'info', so modifying search_ptr here is okay.


    // --- Find 'info' Dictionary ---
    search_ptr = p; // Reset search pointer to the beginning of the main dictionary content
    if(bencode_find_key(&search_ptr, "info") != 0)
    {
        DebugOut("torrent_load: Required key 'info' not found.\n");
        goto cleanup;
    }

    // search_ptr now points to the start of the 'info' value (should be 'd')
    if(!is_safe_to_read(search_ptr, 1) || *search_ptr != 'd')
    {
        DebugOut("torrent_load: 'info' value is not a dictionary.\n");
        goto cleanup;
    }
    // Save the start of the info dictionary's *value* for hashing later if needed
    const unsigned char* info_dict_value_start = search_ptr;
    p = search_ptr + 1; // Move main parsing pointer 'p' inside the info dictionary


    // --- Parse 'info' Dictionary Keys ---
    int name_found = 0, piece_length_found = 0, pieces_found = 0;
    long long single_file_length = -1; // Track if single file 'length' key is found
    int files_list_found = 0;          // Track if 'files' list is found

    while(is_safe_to_read(p, 1) && *p != 'e')
    {
        // MAX_FILENAME_LEN should be from torrent.h, ensure it's defined correctly
        char key[MAX_FILENAME_LEN]; // No need to init {0} if parse_bstr guarantees null-term
        memset(key, 0, sizeof(key)); // Explicit clear is safer though
        const unsigned char* value_p;

        // Parse key
        value_p = parse_bstr(p, key, sizeof(key)); // parse_bstr must null-terminate
        if(!value_p)
        {
            DebugOut("torrent_load: Failed to parse info dictionary key.\n");
            goto cleanup;
        }
        p = value_p; // Advance p past the key string

        if(!is_safe_to_read(p, 1))
        { // Need space for value or dictionary end 'e'
            DebugOut("torrent_load: Unexpected end of buffer after reading key '%s'.\n", key);
            goto cleanup;
        }

        // Process known keys
        if(strcmp(key, "name") == 0)
        {
            p = parse_bstr(p, t->name, sizeof(t->name)); // parse_bstr must null-terminate
            if(!p) { DebugOut("torrent_load: Failed to parse 'name'.\n"); goto cleanup; }
            DebugOut("torrent_load: name = %s\n", t->name);
            name_found = 1;
        }
        else if(strcmp(key, "piece length") == 0)
        {
            long long val = 0; // Initialize just in case parse_int fails early
            p = parse_int(p, &val);
            if(!p || val <= 0) { DebugOut("torrent_load: Failed to parse 'piece length' or invalid value <= 0.\n"); goto cleanup; }
            t->piece_length = (int)val;
            if(t->piece_length != val) { DebugOut("WARN: Piece length %lld truncated to %d\n", val, t->piece_length); }
            DebugOut("torrent_load: piece length = %d\n", t->piece_length);
            piece_length_found = 1;
        }
        else if(strcmp(key, "pieces") == 0)
        {
            size_t len = 0; // <<< Initialize len here
            const unsigned char* pieces_data_ptr = parse_bstr_len(p, &len);
            if(!pieces_data_ptr) { DebugOut("torrent_load: Failed to parse length header of 'pieces'.\n"); goto cleanup; }

            // Check validity of pieces length
            if(len == 0)
            {
                // Technically allowed? Might mean 0 pieces / 0 size torrent.
                DebugOut("torrent_load: Warning: 'pieces' length is zero.\n");
                pieces_found = 1; // Mark as found, but pieces count will be 0
                t->piece_count = 0;
                t->pieces = NULL; // Ensure pieces is NULL
                p = pieces_data_ptr; // Advance p past the zero-length string header
            }
            else if(len % 20 != 0)
            {
                DebugOut("torrent_load: Invalid 'pieces' length %zu (not multiple of 20).\n", len);
                goto cleanup;
            }
            else
            {
                // Ensure we can read the piece data itself (this checks if data_ptr + len is <= end_ptr)
                if(!is_safe_to_read(pieces_data_ptr, len))
                {
                    DebugOut("torrent_load: Unexpected end of buffer reading pieces data (%zu bytes expected).\n", len);
                    goto cleanup;
                }

                // Allocate memory for pieces
                t->piece_count = (int)(len / 20);
                t->pieces = (unsigned char*)malloc(len);
                if(!t->pieces)
                {
                    DebugOut("torrent_load: Failed to allocate %zu bytes for piece hashes.\n", len);
                    goto cleanup;
                }
                memcpy(t->pieces, pieces_data_ptr, len);
                p = pieces_data_ptr + len; // Advance p past piece data

                DebugOut("torrent_load: piece count = %d (total %zu bytes)\n", t->piece_count, len);
                pieces_found = 1;
            }
        }
        else if(strcmp(key, "length") == 0)
        { // Single-file torrent indicator
            p = parse_int(p, &single_file_length);
            if(!p || single_file_length < 0) { DebugOut("torrent_load: Failed to parse single-file 'length' or negative value.\n"); goto cleanup; }
            DebugOut("torrent_load: Found single-file length = %lld\n", single_file_length);
        }
        else if(strcmp(key, "files") == 0)
        { // Multi-file torrent indicator
            if(!is_safe_to_read(p, 1) || *p != 'l') { DebugOut("torrent_load: 'files' key does not contain a list 'l'.\n"); goto cleanup; }
            p++; // Skip 'l'
            files_list_found = 1;
            t->total_size = 0; // Reset total size for multi-file

            while(is_safe_to_read(p, 1) && *p != 'e')
            { // Loop through file entries (dictionaries)
                if(t->file_count >= MAX_FILES_PER_TORRENT)
                {
                    DebugOut("torrent_load: Warning: Exceeded MAX_FILES_PER_TORRENT (%d), skipping remaining files.\n", MAX_FILES_PER_TORRENT);
                    p = skip_bencoded(p); // Skip the rest of the file entry dict/list/etc.
                    if(!p) { DebugOut("torrent_load: Failed to skip excess file entry.\n"); goto cleanup; }
                    continue; // Go to next item check ('e' or next file entry)
                }
                if(!is_safe_to_read(p, 1) || *p != 'd') { DebugOut("torrent_load: Expected dictionary 'd' for file entry.\n"); goto cleanup; }
                p++; // Skip 'd'

                // Ensure TorrentFile struct is defined correctly in torrent.h
                TorrentFile* f = &t->files[t->file_count];
                memset(f, 0, sizeof(*f)); // Clear the file struct
                int current_file_len_found = 0;
                int current_file_path_found = 0;

                // Parse file dictionary
                while(is_safe_to_read(p, 1) && *p != 'e')
                {
                    char file_key[64];
                    memset(file_key, 0, sizeof(file_key)); // Clear key buffer
                    value_p = parse_bstr(p, file_key, sizeof(file_key)); // parse_bstr must null-terminate
                    if(!value_p) { DebugOut("torrent_load: Failed to parse file dictionary key.\n"); goto cleanup; }
                    p = value_p; // Advance past key

                    if(!is_safe_to_read(p, 1)) { DebugOut("torrent_load: Unexpected EOF after file key '%s'.\n", file_key); goto cleanup; }

                    if(strcmp(file_key, "length") == 0)
                    {
                        long long file_len = 0;
                        p = parse_int(p, &file_len);
                        if(!p || file_len < 0) { DebugOut("torrent_load: Failed to parse file length or negative value.\n"); goto cleanup; }
                        f->length = file_len;
                        t->total_size += f->length; // Accumulate total size
                        current_file_len_found = 1;
                    }
                    else if(strcmp(file_key, "path") == 0)
                    {
                        if(!is_safe_to_read(p, 1) || *p != 'l') { DebugOut("torrent_load: Malformed file path list, expected 'l'.\n"); goto cleanup; }
                        p++; // Skip 'l' for path list
                        f->path_depth = 0; // Reset path depth for this file
                        while(is_safe_to_read(p, 1) && *p != 'e')
                        {
                            if(f->path_depth >= MAX_PATH_COMPONENTS)
                            {
                                DebugOut("torrent_load: Warning: Exceeded MAX_PATH_COMPONENTS (%d) for file %d path, skipping remaining components.\n", MAX_PATH_COMPONENTS, t->file_count);
                                p = skip_bencoded(p); // Skip the path component string
                                if(!p) { DebugOut("torrent_load: Failed to skip excess path component.\n"); goto cleanup; }
                                continue;
                            }
                            // Ensure path buffer sizes in TorrentFile are correct
                            // parse_bstr must null-terminate
                            p = parse_bstr(p, f->path[f->path_depth], sizeof(f->path[0]));
                            if(!p) { DebugOut("torrent_load: Failed parsing file path component %d.\n", f->path_depth); goto cleanup; }
                            f->path_depth++;
                        }
                        if(!is_safe_to_read(p, 1) || *p != 'e') { DebugOut("torrent_load: Path list not terminated by 'e'.\n"); goto cleanup; }
                        p++; // Skip 'e' for path list
                        current_file_path_found = 1;
                    }
                    // Could add handling for 'path.utf-8' key here if needed
                    else
                    {
                        DebugOut("torrent_load: Skipping unknown file key '%s'.\n", file_key);
                        p = skip_bencoded(p); // Skip the value for this key
                        if(!p) { DebugOut("torrent_load: Failed to skip unknown file key value.\n"); goto cleanup; }
                    }
                } // End of file dictionary parsing loop

                if(!is_safe_to_read(p, 1) || *p != 'e') { DebugOut("torrent_load: File dictionary not terminated by 'e'.\n"); goto cleanup; }
                p++; // Skip 'e' for file dict

                if(!current_file_len_found || !current_file_path_found)
                {
                    // Handle missing path/len for file entry? Some torrents might have 0-byte files.
                    // If path is missing, it's definitely an error. If len is missing? Error too by spec.
                    DebugOut("torrent_load: File entry %d missing required 'length' or 'path'.\n", t->file_count);
                    goto cleanup;
                }

                DebugOut("torrent_load: Added file %d, length %lld, path[0] '%s' (%d components)\n",
                         t->file_count, f->length, (f->path_depth > 0 ? f->path[0] : "N/A"), f->path_depth);
                t->file_count++;

            } // End of file list loop ('l')

            if(!is_safe_to_read(p, 1) || *p != 'e') { DebugOut("torrent_load: Files list 'l' not terminated by 'e'.\n"); goto cleanup; }
            p++; // Skip 'e' for files list

        }
        else
        { // Skip unknown keys in the info dictionary
            DebugOut("torrent_load: Skipping unknown info key '%s'\n", key);
            p = skip_bencoded(p); // Skip the value associated with the unknown key
            if(!p) { DebugOut("torrent_load: Failed to skip value for unknown info key '%s'.\n", key); goto cleanup; }
        }

    } // End of info dictionary parsing loop


    // --- Final Checks and Single File Handling ---
    // We should now be pointed right after the info dictionary's content, expecting its 'e'
    if(!is_safe_to_read(p, 1) || *p != 'e')
    {
        // If loop exited because is_safe_to_read(p, 1) failed, this also catches it
        DebugOut("torrent_load: Info dictionary structure error or buffer ended unexpectedly before 'e'.\n");
        goto cleanup;
    }
    // p++; // Don't necessarily need to advance past info dict 'e', parsing is done.

    // Check for required keys presence (ensure flags were set)
    if(!name_found || !piece_length_found || !pieces_found)
    {
        DebugOut("torrent_load: Missing essential info keys (name:%d, piece_length:%d, pieces:%d found?)\n",
                 name_found, piece_length_found, pieces_found);
        goto cleanup;
    }

    // Handle single vs multi-file determination based on keys found
    if(single_file_length >= 0 && files_list_found)
    {
        DebugOut("torrent_load: Error: Found both 'length' and 'files' keys in info dict.\n");
        goto cleanup;
    }
    else if(single_file_length >= 0)
    { // Single file case
        if(t->file_count != 0) // Sanity check - should be 0 if 'files' wasn't processed
        {
            DebugOut("torrent_load: Internal error: In single-file mode but file_count is %d.\n", t->file_count);
            goto cleanup;
        }
        t->total_size = single_file_length;
        t->file_count = 1;
        t->files[0].length = single_file_length;
        // Ensure t->name was successfully parsed before using it here
        strncpy(t->files[0].path[0], t->name, sizeof(t->files[0].path[0]) - 1);
        // Ensure null termination explicitly, strncpy might not if src >= dest-1
        t->files[0].path[0][sizeof(t->files[0].path[0]) - 1] = '\0';
        t->files[0].path_depth = 1;
        DebugOut("torrent_load: Confirmed single-file torrent. Size: %lld\n", t->total_size);
    }
    else if(files_list_found)
    { // Multi file case
        // total_size and file_count should have been populated during list parsing.
        if(t->file_count == 0 && t->total_size == 0)
        {
            // An empty 'files' list is possible.
            DebugOut("torrent_load: Confirmed multi-file torrent with empty 'files' list.\n");
        }
        else if(t->file_count > 0 && t->total_size >= 0)
        {
            // Standard multi-file case
            DebugOut("torrent_load: Confirmed multi-file torrent. Total size: %lld, File count: %d\n", t->total_size, t->file_count);
        }
        else
        {
            // Should not happen if parsing logic is correct
            DebugOut("torrent_load: Internal error: Multi-file state inconsistent (files:%d, size:%lld)\n", t->file_count, t->total_size);
            goto cleanup;
        }
    }
    else
    {
        // Neither 'length' nor 'files' key was found inside 'info'
        DebugOut("torrent_load: Error: Missing 'length' (for single file) or 'files' (for multi-file) key in info dict.\n");
        goto cleanup;
    }

    // Basic validation: total size vs piece info
    if(t->piece_length > 0 && t->piece_count >= 0)
    { // Avoid division by zero or negative counts
        long long expected_min_size = (long long)(t->piece_count - 1) * t->piece_length;
        long long expected_max_size = (long long)t->piece_count * t->piece_length;

        if(t->piece_count == 0)
        { // No pieces
            if(t->total_size != 0)
            {
                DebugOut("torrent_load: Warning: Piece count is 0 but total size is %lld.\n", t->total_size);
            }
            // Else: Both 0, consistent.
        }
        else
        { // One or more pieces
            if(t->total_size <= expected_min_size || t->total_size > expected_max_size)
            {
                DebugOut("torrent_load: Warning: Total size %lld is outside expected range (%lld - %lld) for %d pieces of %d length.\n",
                         t->total_size, expected_min_size + 1, expected_max_size, t->piece_count, t->piece_length);
                // Don't make this fatal, calculation only works if last piece isn't tiny.
            }
        }
    }
    else if(t->piece_length <= 0)
    {
        DebugOut("torrent_load: Warning: Cannot validate size against pieces due to invalid piece length (%d).\n", t->piece_length);
    }


    // Success path
    result = 0;
    DebugOut("torrent_load: Successfully loaded and parsed torrent '%s'.\n", t->name);

cleanup:
    // Free the buffer holding the original .torrent file content
    free(file_buffer);
    set_parse_end_ptr(NULL); // Clear global end pointer for parser safety

    // If we hit an error ('result' is still -1), free any resources allocated inside 't'
    if(result != 0)
    {
        // torrent_free only frees t->pieces currently.
        torrent_free(t);
        // Ensure 't' itself is still somewhat clean (null pieces ptr, zero counts)
        // although memset already did this at the start. Caller should not use 't' on error.
    }

    return result;
}