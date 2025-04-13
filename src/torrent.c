#define _CRT_SECURE_NO_WARNINGS
#include "torrent.h"       // Needs Torrent struct, MAX_FILENAME_LEN, etc.
#include "torrent_parse.h" // Needs declarations for parse_*, skip_*, bencode_find_key, etc.
#include "debug.h"         // Needs DebugOut declaration

#include <stdio.h>
#include <stdlib.h> // For malloc, free
#include <string.h> // For memcpy, memset, strcmp
#include <stdint.h> // For standard types

//=============================================================================
// Helper: Dummy SHA-1 Implementation (REMOVE THIS WHEN USING A REAL LIBRARY)
//=============================================================================
// You MUST provide a real SHA-1 implementation (e.g., using OpenSSL, Windows CNG,
// or a standalone library) and update calculate_sha1, then remove this dummy.
int calculate_sha1(const unsigned char* data, size_t data_len, unsigned char hash_out[20])
{
    DebugOut("Warning: calculate_sha1() is a dummy implementation!\n");
    if(!data || !hash_out)
    { // Allow data_len == 0 ? Maybe.
        return -1; // Basic check
    }
    // Create a somewhat predictable-but-changing dummy hash based on data
    memset(hash_out, 0xDD, 20); // Fill with dummy data
    if(data_len > 0)
    {
        hash_out[0] = data[0];
        hash_out[1] = data[data_len - 1];
        hash_out[2] = (unsigned char)(data_len & 0xFF);
        hash_out[3] = (unsigned char)((data_len >> 8) & 0xFF);
        // Add a simple checksum element
        unsigned char checksum = 0;
        for(size_t i = 0; i < data_len; ++i)
        {
            checksum += data[i] * (i % 7 + 1);
        }
        hash_out[4] = checksum;
    }
    return 0; // Pretend success
}
//=============================================================================


//=============================================================================
// Function to free dynamically allocated memory within a Torrent struct
//=============================================================================
void torrent_free(Torrent* t)
{
    if(t)
    {
        free(t->pieces); // Free the piece hashes buffer (malloc'd in torrent_load)
        t->pieces = NULL; // Avoid double free
        t->piece_count = 0;
        // NOTE: This does NOT free 't' itself if it was allocated with malloc.
        // It also doesn't free the 'files' array, as it's embedded directly.
    }
}


//=============================================================================
// Helper to read entire file into a dynamically allocated buffer
//=============================================================================
static unsigned char* read_entire_file(const char* path, size_t* out_size)
{
    // Ensure out_size is valid if provided
    if(out_size) *out_size = 0;

    FILE* f = fopen(path, "rb");
    if(!f)
    {
        DebugOut("read_entire_file: Failed to open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size_long = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(file_size_long < 0)
    {
        DebugOut("read_entire_file: Failed to get file size (ftell error) for %s\n", path);
        fclose(f);
        return NULL;
    }
    if(file_size_long == 0)
    {
        DebugOut("read_entire_file: Warning: File size is 0 for %s. Returning NULL buffer.\n", path);
        fclose(f);
        // *out_size is already 0 from initialization
        return NULL; // Return NULL for 0-byte file, as it's likely unusable for torrent parsing.
    }

    size_t file_size = (size_t)file_size_long;
    unsigned char* buffer = (unsigned char*)malloc(file_size);

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
        free(buffer);
        fclose(f);
        return NULL;
    }

    fclose(f);
    DebugOut("read_entire_file: Read %zu bytes from %s\n", bytes_read, path);

    if(out_size)
    {
        *out_size = file_size;
    }
    return buffer;
}

//=============================================================================
// Main Torrent Loading Function
//=============================================================================
int torrent_load(const char* path, Torrent* t)
{
    // --- Pre-check and Initialization ---
    if(!path || !t)
    {
        DebugOut("torrent_load: Error: Invalid NULL arguments (path or t).\n");
        return -1;
    }

    size_t size = 0;
    unsigned char* file_buffer = read_entire_file(path, &size);

    if(!file_buffer || size == 0)
    {
        DebugOut("torrent_load: Failed to read file content or file is empty for %s.\n", path);
        // read_entire_file should have logged specific error.
        if(file_buffer) free(file_buffer); // Free if read_entire_file succeeded but size=0 somehow
        return -1;
    }

    // ** CRITICAL ** : The functions below (set_parse_end_ptr, parse_*, etc.)
    //                  MUST be defined in torrent_parse.c and linked correctly!
    set_parse_end_ptr(file_buffer + size); // Set boundary for parsing functions

    memset(t, 0, sizeof(*t)); // Clear the user's Torrent struct
    t->pieces = NULL;         // Ensure dynamic pointer is NULL initially

    const unsigned char* p = file_buffer;
    const unsigned char* end = file_buffer + size;
    int result = -1; // Default to error


    // --- Basic Bencode Check ---
    if(!is_safe_to_read(p, 1) || *p != 'd')
    {
        DebugOut("torrent_load: Torrent file does not start with dictionary 'd'.\n");
        goto cleanup;
    }
    p++; // Skip 'd' of root dictionary

    // --- Variables for First Pass (finding raw info dict and announce) ---
    const unsigned char* info_dict_raw_start = NULL;
    const unsigned char* info_dict_raw_end = NULL;
    const unsigned char* parse_ptr_after_info_key = NULL;
    int announce_found = 0;
    int announce_list_found = 0;


    // === First Pass: Locate Info Dictionary boundaries, parse Announce URLs ===
    const unsigned char* current_item_ptr = p; // Iterator for top-level dict keys
    while(is_safe_to_read(current_item_ptr, 1) && *current_item_ptr != 'e')
    {
        char key[64]; // Buffer for keys like "announce", "info", "comment", etc.
        memset(key, 0, sizeof(key)); // Ensure null termination
        const unsigned char* value_ptr = parse_bstr(current_item_ptr, key, sizeof(key));
        if(!value_ptr)
        {
            DebugOut("torrent_load: Failed to parse top-level dictionary key (pass 1).\n");
            goto cleanup;
        }

        if(strcmp(key, "announce") == 0)
        {
            const unsigned char* next_p = parse_bstr(value_ptr, t->announce, MAX_ANNOUNCE_URL_LEN);
            if(!next_p)
            {
                DebugOut("torrent_load: Failed to parse 'announce' value.\n");
                goto cleanup;
            }
            DebugOut("torrent_load: announce = %s\n", t->announce);
            announce_found = 1;
            current_item_ptr = next_p; // Advance main iterator past this value
        }
        else if(strcmp(key, "announce-list") == 0)
        {
            // Parse announce-list (list of lists of strings)
            const unsigned char* list_ptr = value_ptr; // Pointer to start of value (should be 'l')
            if(!is_safe_to_read(list_ptr, 1) || *list_ptr != 'l') { DebugOut("torrent_load: 'announce-list' is not a list.\n"); goto cleanup; }
            list_ptr++; // Skip 'l' (main tier list)

            t->announce_tier_count = 0; // Initialize count
            while(is_safe_to_read(list_ptr, 1) && *list_ptr != 'e')
            { // Iterate through tiers (lists)
                if(t->announce_tier_count >= MAX_ANNOUNCE_TIERS)
                { // Check tier limit
                    DebugOut("torrent_load: Warning: Max announce tiers (%d) reached, skipping rest.\n", MAX_ANNOUNCE_TIERS);
                    list_ptr = skip_bencoded(list_ptr); if(!list_ptr) goto cleanup; continue;
                }
                if(!is_safe_to_read(list_ptr, 1) || *list_ptr != 'l') { DebugOut("torrent_load: Announce list tier is not a list.\n"); goto cleanup; }
                list_ptr++; // Skip 'l' (current tier list)

                AnnounceTier* tier = &t->announce_list[t->announce_tier_count]; // Get current tier struct
                tier->url_count = 0; // Initialize URL count for this tier

                while(is_safe_to_read(list_ptr, 1) && *list_ptr != 'e')
                { // Iterate through URLs (strings) in tier
                    if(tier->url_count >= MAX_TRACKERS_PER_TIER)
                    { // Check URL limit within tier
                        DebugOut("torrent_load: Warning: Max trackers per tier (%d) reached, skipping rest.\n", MAX_TRACKERS_PER_TIER);
                        list_ptr = skip_bencoded(list_ptr); if(!list_ptr) goto cleanup; continue;
                    }
                    // Parse the tracker URL string
                    list_ptr = parse_bstr(list_ptr, tier->urls[tier->url_count], MAX_ANNOUNCE_URL_LEN);
                    if(!list_ptr) { DebugOut("torrent_load: Failed to parse tracker URL in announce-list.\n"); goto cleanup; }
                    // DebugOut("   Added URL: %s\n", tier->urls[tier->url_count]);
                    tier->url_count++; // Increment URL count
                }
                if(!is_safe_to_read(list_ptr, 1) || *list_ptr != 'e') { DebugOut("torrent_load: Announce list tier not terminated by 'e'.\n"); goto cleanup; }
                list_ptr++; // Skip 'e' (end of current tier list)

                if(tier->url_count > 0) { t->announce_tier_count++; } // Only increment tier count if it wasn't empty
            }
            if(!is_safe_to_read(list_ptr, 1) || *list_ptr != 'e') { DebugOut("torrent_load: Announce list (main) not terminated by 'e'.\n"); goto cleanup; }
            list_ptr++; // Skip 'e' (end of main tier list)

            announce_list_found = 1;
            current_item_ptr = list_ptr; // Advance main iterator past announce-list
            DebugOut("torrent_load: Parsed announce-list with %d tiers.\n", t->announce_tier_count);
        }
        else if(strcmp(key, "info") == 0)
        {
            // Found the info dictionary!
            // value_ptr points to the start of its bencoded value (should be 'd')
            if(!is_safe_to_read(value_ptr, 1) || *value_ptr != 'd') { DebugOut("torrent_load: 'info' key value is not a dictionary.\n"); goto cleanup; }

            info_dict_raw_start = value_ptr; // Mark start of RAW info dictionary value
            parse_ptr_after_info_key = value_ptr; // Store this for the second pass parsing

            // Skip over the entire raw bencoded info dictionary to find its end
            info_dict_raw_end = skip_bencoded(value_ptr);
            if(!info_dict_raw_end) { DebugOut("torrent_load: Failed to find end of 'info' dictionary.\n"); goto cleanup; }

            // Advance main iterator past the raw info dictionary we just skipped
            current_item_ptr = info_dict_raw_end;
            DebugOut("torrent_load: Located raw info dictionary range (len %td).\n", info_dict_raw_end - info_dict_raw_start);
        }
        else
        {
            // Skip other optional top-level keys we don't handle (e.g., "comment")
            DebugOut("torrent_load: Skipping top-level key '%s'.\n", key);
            current_item_ptr = skip_bencoded(value_ptr); // Skip the corresponding value
            if(!current_item_ptr) { DebugOut("torrent_load: Failed to skip value for top-level key '%s'.\n", key); goto cleanup; }
        }
    } // End of first pass loop (iterating top-level keys)


    // --- Post First Pass Checks ---
    if(!info_dict_raw_start || !info_dict_raw_end || !parse_ptr_after_info_key)
    {
        DebugOut("torrent_load: Critical Error: Required 'info' dictionary was not found or delimited.\n");
        goto cleanup;
    }
    if(!announce_found && !announce_list_found)
    {
        DebugOut("torrent_load: Warning: Neither 'announce' nor 'announce-list' key found (maybe trackerless?).\n");
    }


    // === Calculate Info Hash (SHA-1) ===
    size_t info_dict_len = info_dict_raw_end - info_dict_raw_start;
    if(info_dict_len == 0) { DebugOut("torrent_load: Error: Info dictionary has zero length.\n"); goto cleanup; }

    // Call the SHA-1 function (currently the dummy version above)
    if(calculate_sha1(info_dict_raw_start, info_dict_len, t->info_hash) != 0)
    {
        DebugOut("torrent_load: Failed to calculate SHA-1 hash of info dictionary.\n");
        goto cleanup; // Treat failure to hash as fatal
    }
    else
    {
        char info_hash_hex[41];
        for(int i = 0; i < 20; ++i) sprintf_s(info_hash_hex + i * 2, 3, "%02x", t->info_hash[i]); // Use sprintf_s for safety
        DebugOut("torrent_load: Calculated Info Hash: %s\n", info_hash_hex);
    }


    // === Second Pass: Parse the Contents of the Info Dictionary ===
    p = parse_ptr_after_info_key; // Reset 'p' to start of info dict value ('d')
    if(!is_safe_to_read(p, 1) || *p != 'd') { DebugOut("torrent_load: Internal error: Info dict start pointer invalid for pass 2.\n"); goto cleanup; }
    p++; // Move p *inside* the info dictionary


    // --- Parse 'info' Dictionary Keys (Loop adapted from previous logic) ---
    int name_found = 0, piece_length_found = 0, pieces_found = 0;
    long long single_file_length = -1;
    int files_list_found = 0;

    while(is_safe_to_read(p, 1) && *p != 'e')
    {
        // Ensure MAX_FILENAME_LEN is defined correctly in torrent.h
        char key[MAX_FILENAME_LEN];
        memset(key, 0, sizeof(key)); // Clear key buffer
        const unsigned char* value_p;

        value_p = parse_bstr(p, key, sizeof(key));
        if(!value_p) { DebugOut("torrent_load: Failed to parse info key (pass 2).\n"); goto cleanup; }
        p = value_p; // Advance p past the key string

        if(!is_safe_to_read(p, 1)) { DebugOut("torrent_load: Unexpected end after info key '%s' (pass 2).\n", key); goto cleanup; }

        // Process known keys inside 'info'
        if(strcmp(key, "name") == 0)
        {
            p = parse_bstr(p, t->name, sizeof(t->name)); // t->name should be MAX_FILENAME_LEN
            if(!p) { DebugOut("torrent_load: Failed to parse 'name'.\n"); goto cleanup; }
            DebugOut("torrent_load: name = %s\n", t->name);
            name_found = 1;
        }
        else if(strcmp(key, "piece length") == 0)
        {
            long long val = 0;
            p = parse_int(p, &val);
            if(!p || val <= 0) { DebugOut("torrent_load: Failed to parse 'piece length' or invalid value <= 0.\n"); goto cleanup; }
            if(val > INT_MAX) { DebugOut("torrent_load: Warning: Piece length %lld exceeds INT_MAX, may cause issues if not handled.\n", val); }
            t->piece_length = (int)val; // Store as int; Check for truncation handled above
            DebugOut("torrent_load: piece length = %d\n", t->piece_length);
            piece_length_found = 1;
        }
        else if(strcmp(key, "pieces") == 0)
        {
            size_t len = 0;
            const unsigned char* pieces_data_ptr = parse_bstr_len(p, &len);
            if(!pieces_data_ptr) { DebugOut("torrent_load: Failed to parse length header of 'pieces'.\n"); goto cleanup; }

            if(len == 0) { DebugOut("torrent_load: Warning: 'pieces' length is zero.\n"); t->piece_count = 0; t->pieces = NULL; p = pieces_data_ptr; }
            else if(len % 20 != 0) { DebugOut("torrent_load: Invalid 'pieces' length %zu (not multiple of 20).\n", len); goto cleanup; }
            else
            {
                if(!is_safe_to_read(pieces_data_ptr, len)) { DebugOut("torrent_load: Unexpected end reading pieces data (%zu bytes expected).\n", len); goto cleanup; }
                t->piece_count = (int)(len / 20);
                if(t->piece_count <= 0) { DebugOut("torrent_load: Error: Invalid piece count %d calculated from length %zu.\n", t->piece_count, len); goto cleanup; } // Should not happen if len%20==0 and len>0
                t->pieces = (unsigned char*)malloc(len); // Allocate buffer
                if(!t->pieces) { DebugOut("torrent_load: Failed to allocate %zu bytes for piece hashes.\n", len); goto cleanup; }
                memcpy(t->pieces, pieces_data_ptr, len); // Copy hash data
                p = pieces_data_ptr + len; // Advance p past piece data
                DebugOut("torrent_load: piece count = %d (total %zu bytes)\n", t->piece_count, len);
            }
            pieces_found = 1;
        }
        else if(strcmp(key, "length") == 0)
        { // Found in 'info': implies single-file torrent
            p = parse_int(p, &single_file_length);
            if(!p || single_file_length < 0) { DebugOut("torrent_load: Failed to parse single-file 'length' or negative value.\n"); goto cleanup; }
            DebugOut("torrent_load: Found single-file length = %lld\n", single_file_length);
        }
        else if(strcmp(key, "files") == 0)
        { // Found in 'info': implies multi-file torrent
            if(!is_safe_to_read(p, 1) || *p != 'l') { DebugOut("torrent_load: 'files' key does not contain a list 'l'.\n"); goto cleanup; }
            p++; // Skip 'l' (start of file list)
            files_list_found = 1;
            t->total_size = 0; // Reset total size for multi-file (will be summed)
            t->file_count = 0; // Reset file count

            while(is_safe_to_read(p, 1) && *p != 'e')
            { // Loop through file entries (dictionaries)
                if(t->file_count >= MAX_FILES_PER_TORRENT)
                { // Check file limit
                    DebugOut("torrent_load: Warning: Exceeded MAX_FILES_PER_TORRENT (%d), skipping rest.\n", MAX_FILES_PER_TORRENT);
                    p = skip_bencoded(p); if(!p) goto cleanup; continue;
                }
                if(!is_safe_to_read(p, 1) || *p != 'd') { DebugOut("torrent_load: Expected dictionary 'd' for file entry.\n"); goto cleanup; }
                p++; // Skip 'd' (start of file entry dict)

                TorrentFile* f = &t->files[t->file_count]; // Get pointer to current file entry in struct
                memset(f, 0, sizeof(*f)); // Clear this file entry
                int current_file_len_found = 0;
                int current_file_path_found = 0;

                while(is_safe_to_read(p, 1) && *p != 'e')
                { // Parse keys inside file entry dict
                    char file_key[64]; memset(file_key, 0, sizeof(file_key));
                    value_p = parse_bstr(p, file_key, sizeof(file_key));
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
                        p++; // Skip 'l' (start of path component list)
                        f->path_depth = 0; // Reset path depth for this file
                        while(is_safe_to_read(p, 1) && *p != 'e')
                        { // Loop through path components (strings)
                            if(f->path_depth >= MAX_PATH_COMPONENTS)
                            { // Check component limit
                                DebugOut("torrent_load: Warning: Exceeded MAX_PATH_COMPONENTS (%d) for file %d path, skipping rest.\n", MAX_PATH_COMPONENTS, t->file_count);
                                p = skip_bencoded(p); if(!p) goto cleanup; continue;
                            }
                            // Ensure path buffer in TorrentFile is sized correctly (MAX_FILENAME_LEN)
                            p = parse_bstr(p, f->path[f->path_depth], MAX_FILENAME_LEN);
                            if(!p) { DebugOut("torrent_load: Failed parsing file path component %d.\n", f->path_depth); goto cleanup; }
                            f->path_depth++;
                        }
                        if(!is_safe_to_read(p, 1) || *p != 'e') { DebugOut("torrent_load: Path list not terminated by 'e'.\n"); goto cleanup; }
                        p++; // Skip 'e' (end of path component list)
                        current_file_path_found = 1;
                    }
                    else
                    { // Skip unknown keys in file entry dict (e.g., 'attr')
                        DebugOut("torrent_load: Skipping unknown file key '%s'.\n", file_key);
                        p = skip_bencoded(p); if(!p) { DebugOut("torrent_load: Failed to skip unknown file key value.\n"); goto cleanup; }
                    }
                } // End of file entry dict parse loop

                if(!is_safe_to_read(p, 1) || *p != 'e') { DebugOut("torrent_load: File dictionary not terminated by 'e'.\n"); goto cleanup; }
                p++; // Skip 'e' (end of file entry dict)

                if(!current_file_len_found || !current_file_path_found) { DebugOut("torrent_load: File entry %d missing required 'length' or 'path'.\n", t->file_count); goto cleanup; }
                // Optional: Check if f->path_depth == 0 (empty path) - might be invalid?

                DebugOut("torrent_load: Added file %d, length %lld, path[0] '%s' (%d components)\n",
                         t->file_count, f->length, (f->path_depth > 0 ? f->path[0] : "N/A"), f->path_depth);
                t->file_count++; // Increment successfully parsed file count

            } // End of file list ('l') loop

            if(!is_safe_to_read(p, 1) || *p != 'e') { DebugOut("torrent_load: Files list 'l' not terminated by 'e'.\n"); goto cleanup; }
            p++; // Skip 'e' (end of files list)
        }
        else
        { // Skip unknown keys in info dictionary (e.g., 'private', 'source')
            DebugOut("torrent_load: Skipping unknown info key '%s'\n", key);
            p = skip_bencoded(p); if(!p) { DebugOut("torrent_load: Failed to skip value for unknown info key '%s'.\n", key); goto cleanup; }
        }
    } // End of info dictionary parsing loop (pass 2)


    // --- Final Validation and Structure Setup ---
    if(!is_safe_to_read(p, 1) || *p != 'e')
    { // Check termination of info dictionary
        DebugOut("torrent_load: Info dictionary parsing ended unexpectedly before final 'e'.\n");
        goto cleanup;
    }

    if(!name_found || !piece_length_found || !pieces_found)
    { // Check required fields parsed
        DebugOut("torrent_load: Missing essential info keys (name:%d, piece_length:%d, pieces:%d found?).\n",
                 name_found, piece_length_found, pieces_found);
        goto cleanup;
    }

    // Determine file structure (single vs multi) based on keys found in info dict
    if(single_file_length >= 0 && files_list_found) { DebugOut("torrent_load: Error: Found both 'length' and 'files' keys in info dict.\n"); goto cleanup; }
    else if(single_file_length >= 0)
    { // Single file setup
        if(t->file_count != 0) { DebugOut("torrent_load: Internal error: Single-file mode but file_count is %d.\n", t->file_count); goto cleanup; }
        t->total_size = single_file_length;
        t->file_count = 1;
        t->files[0].length = single_file_length;
        strncpy(t->files[0].path[0], t->name, MAX_FILENAME_LEN - 1);
        t->files[0].path[0][MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
        t->files[0].path_depth = 1;
        DebugOut("torrent_load: Confirmed single-file torrent. Size: %lld\n", t->total_size);
    }
    else if(files_list_found)
    { // Multi file final check
        if(t->file_count > 0) { DebugOut("torrent_load: Confirmed multi-file torrent. Total size: %lld, File count: %d\n", t->total_size, t->file_count); }
        else { DebugOut("torrent_load: Confirmed multi-file torrent with empty 'files' list. Size: %lld\n", t->total_size); } // Size should be 0
    }
    else { DebugOut("torrent_load: Error: Missing 'length' or 'files' key in info dict.\n"); goto cleanup; }

    // Final sanity check on total size vs piece info
    if(t->piece_length > 0 && t->piece_count > 0)
    {
        long long expected_max_size = (long long)t->piece_count * t->piece_length;
        long long expected_min_size_exclusive = expected_max_size - t->piece_length;
        if(t->total_size <= expected_min_size_exclusive || t->total_size > expected_max_size)
        {
            DebugOut("torrent_load: Warning: Total size %lld seems inconsistent for %d pieces * %d length (expected %lld - %lld].\n",
                     t->total_size, t->piece_count, t->piece_length, expected_min_size_exclusive + 1, expected_max_size);
        }
    }

    // --- Success ---
    result = 0; // Mark as success
    DebugOut("torrent_load: Successfully processed torrent '%s'.\n", t->name);


cleanup: // Label for jumping on error
    // Free the buffer holding the original .torrent file content
    free(file_buffer); // file_buffer MUST be freed regardless of success/failure
    set_parse_end_ptr(NULL); // Clear global parser boundary pointer

    // If we hit an error ('result' is still -1), free memory allocated INSIDE 't'
    if(result != 0 && t) // Ensure 't' is not NULL here either
    {
        torrent_free(t); // Frees t->pieces if it was allocated
        // Optionally, clear key fields in 't' again to indicate invalid state?
        // memset(t, 0, sizeof(*t)); // Or maybe not, keep partial info for debugging?
    }

    return result; // Return 0 on success, -1 on failure
}