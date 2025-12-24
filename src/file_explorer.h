#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <stdbool.h>

#define MAX_PATH_LENGTH 1024
#define MAX_FILES 10000
#define MAX_SEARCH_RESULTS 1000
#define MAX_SEARCH_DEPTH 10

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

// Explore seulement le contenu direct d'un répertoire (non-récursif)
bool explore_directory_shallow(const char* path, FileList* list);

// Recherche récursive de fichiers par nom (retourne false si limite atteinte)
bool search_files_recursive(const char* path, const char* search_term, FileList* list, int depth);

// Efface le contenu de la liste
void file_list_clear(FileList* list);

// Trie la liste par nom
void file_list_sort(FileList* list);

#endif // FILE_EXPLORER_H
