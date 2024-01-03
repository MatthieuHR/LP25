#include <../include/defines.h>
#include <string.h>

/*!
 * @brief concat_path concatenates suffix to prefix into result
 * It checks if prefix ends by / and adds this token if necessary
 * It also checks that result will fit into PATH_SIZE length
 * @param result the result of the concatenation
 * @param prefix the first part of the resulting path
 * @param suffix the second part of the resulting path
 * @return a pointer to the resulting path, NULL when concatenation failed
 */
char *concat_path(char *result, char *prefix, char *suffix) {
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);

    // Check if the resulting path will fit into PATH_SIZE length
    if (prefix_len + suffix_len + 1 > PATH_SIZE) {
        return NULL;
    }

    strcpy(result, prefix);

    // Check if prefix ends with '/'
    if (prefix[prefix_len - 1] != '/') {
        strcat(result, "/");
    }

    strcat(result, suffix);

    return result;
}
