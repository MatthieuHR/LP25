#include "files-list.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
void clear_files_list(files_list_t *list) {
    while (list->head) {
        files_list_entry_t *tmp = list->head;
        list->head = tmp->next;
        free(tmp);
    }
}
files_list_entry_t *add_file_entry(files_list_t *list, char *file_path) {

    files_list_entry_t *new_entry = malloc(sizeof(files_list_entry_t));
    if (new_entry == NULL) {
        return -1;
    }
    files_list_entry_t *temp = list->head;
    strncpy(new_entry->path_and_name, file_path, sizeof(new_entry->path_and_name));

    while (temp->next != NULL) {
        if (strcmp(temp->path_and_name, file_path) == 0) {
            return 0;
        }else {
            if (strcmp(temp->path_and_name, file_path) < 0 ) {
                new_entry->prev = temp;
                new_entry->next = temp->next;
                temp->next = new_entry;
                return 0;
            }else {
                temp = temp->next;
            }
        }
    }
    if (temp->next == NULL) {
        temp->next = new_entry;
        new_entry->prev = temp;
        new_entry->next = NULL;
        return 0;
    }
}
int add_entry_to_tail(files_list_t *list, files_list_entry_t *entry) {
    if (entry == NULL) {
        return -1;
    }
    entry->next = NULL;
    entry->prev = list->tail;
    if (list->head == NULL) {
        list->head = entry;
    } else {
        list->tail->next = entry;
    }
    list->tail = entry;
    return 0;
}

files_list_entry_t *find_entry_by_name(files_list_t *list, char *file_path, size_t start_of_src, size_t start_of_dest) {
    files_list_entry_t* cursor = list->head;
    while (cursor != NULL) {
        if (strcmp(cursor->path_and_name + start_of_src, file_path + start_of_dest) == 0) {
            return cursor;
        }
        cursor = cursor->next;
    }
    return NULL;
}
void display_files_list(files_list_t *list) {
    if (!list)
        return;

    for (files_list_entry_t *cursor=list->head; cursor!=NULL; cursor=cursor->next) {
        printf("%s\n", cursor->path_and_name);
    }
}

void display_files_list_reversed(files_list_t *list) {
    if (!list)
        return;

    for (files_list_entry_t *cursor=list->tail; cursor!=NULL; cursor=cursor->prev) {
        printf("%s\n", cursor->path_and_name);
    }
}
