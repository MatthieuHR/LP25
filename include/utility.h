#pragma once

#include <defines.h>

char *concat_path(char *result, char *prefix, char *suffix){
    result = strcat(prefix,suffix);
    return result;
}