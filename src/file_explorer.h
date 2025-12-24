#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <stdbool.h>

#define MAX_PATH_LENGTH 1024
#define MAX_FILES 10000

typedef enum {
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY
} FileType;

typedef struct {
    char path[MAX_PATH_LENGTH];
    char name[256];
    FileType type;
    long size;
    int depth;
} FileEntry;

typedef struct {
    FileEntry* entries;
    int count;
    int capacity;
} FileList;

// Initialise une liste de fichiers
FileList* file_list_create(void);

// Libère la mémoire d'une liste de fichiers
void file_list_destroy(FileList* list);

// Explore un répertoire de manière récursive
bool explore_directory(const char* path, FileList* list, int depth);

// Trie la liste par nom
void file_list_sort(FileList* list);

#endif // FILE_EXPLORER_H
