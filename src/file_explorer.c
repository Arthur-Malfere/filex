#include "file_explorer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

// Dossiers à exclure de la recherche récursive
static const char* EXCLUDED_DIRS[] = {
    "node_modules",
    ".git",
    ".svn",
    ".hg",
    "__pycache__",
    ".cache",
    "build",
    "dist",
    "target",
    ".venv",
    "venv",
    "Library",
    "System",
    "Applications",
    "Volumes",
    NULL
};

FileList* file_list_create(void) {
    FileList* list = (FileList*)malloc(sizeof(FileList));
    if (!list) return NULL;
    
    list->capacity = 1000;
    list->count = 0;
    list->entries = (FileEntry*)malloc(sizeof(FileEntry) * list->capacity);
    
    if (!list->entries) {
        free(list);
        return NULL;
    }
    
    return list;
}

void file_list_destroy(FileList* list) {
    if (list) {
        if (list->entries) {
            free(list->entries);
        }
        free(list);
    }
}

static bool file_list_add(FileList* list, const FileEntry* entry) {
    if (list->count >= MAX_FILES) {
        return false;
    }
    
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity * 2;
        if (new_capacity > MAX_FILES) {
            new_capacity = MAX_FILES;
        }
        
        FileEntry* new_entries = (FileEntry*)realloc(list->entries, sizeof(FileEntry) * new_capacity);
        if (!new_entries) {
            return false;
        }
        
        list->entries = new_entries;
        list->capacity = new_capacity;
    }
    
    list->entries[list->count++] = *entry;
    return true;
}

bool explore_directory(const char* path, FileList* list, int depth) {
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Impossible d'ouvrir le répertoire: %s\n", path);
        return false;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorer . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Ignorer les fichiers cachés
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }
        
        FileEntry file_entry;
        strncpy(file_entry.path, full_path, sizeof(file_entry.path) - 1);
        file_entry.path[sizeof(file_entry.path) - 1] = '\0';
        
        strncpy(file_entry.name, entry->d_name, sizeof(file_entry.name) - 1);
        file_entry.name[sizeof(file_entry.name) - 1] = '\0';
        
        file_entry.size = st.st_size;
        file_entry.depth = depth;
        
        if (S_ISDIR(st.st_mode)) {
            file_entry.type = FILE_TYPE_DIRECTORY;
            file_list_add(list, &file_entry);
            
            // Exploration récursive
            explore_directory(full_path, list, depth + 1);
        } else {
            file_entry.type = FILE_TYPE_FILE;
            file_list_add(list, &file_entry);
        }
    }
    
    closedir(dir);
    return true;
}

bool explore_directory_shallow(const char* path, FileList* list) {
    DIR* dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Impossible d'ouvrir le répertoire: %s\n", path);
        return false;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorer . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Ignorer les fichiers cachés
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }
        
        FileEntry file_entry;
        strncpy(file_entry.path, full_path, sizeof(file_entry.path) - 1);
        file_entry.path[sizeof(file_entry.path) - 1] = '\0';
        
        strncpy(file_entry.name, entry->d_name, sizeof(file_entry.name) - 1);
        file_entry.name[sizeof(file_entry.name) - 1] = '\0';
        
        file_entry.size = st.st_size;
        file_entry.depth = 0;
        
        if (S_ISDIR(st.st_mode)) {
            file_entry.type = FILE_TYPE_DIRECTORY;
        } else {
            file_entry.type = FILE_TYPE_FILE;
        }
        
        file_list_add(list, &file_entry);
    }
    
    closedir(dir);
    return true;
}

bool search_files_recursive(const char* path, const char* search_term, FileList* list, int depth) {
    // Limites de sécurité
    if (depth > MAX_SEARCH_DEPTH) {
        return true;  // Continuer mais ne pas descendre plus profond
    }
    
    if (list->count >= MAX_SEARCH_RESULTS) {
        return false;  // Limite de résultats atteinte
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        return true;  // Continuer même si on ne peut pas ouvrir ce dossier
    }
    
    // Convertir le terme de recherche en minuscules
    char lower_search[256];
    for (int i = 0; search_term[i] && i < 255; i++) {
        lower_search[i] = tolower(search_term[i]);
        lower_search[i + 1] = '\0';
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Vérifier la limite de résultats
        if (list->count >= MAX_SEARCH_RESULTS) {
            closedir(dir);
            return false;
        }
        
        // Ignorer . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Ignorer les fichiers cachés
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        // Vérifier si le dossier est dans la liste d'exclusion
        bool excluded = false;
        for (int i = 0; EXCLUDED_DIRS[i] != NULL; i++) {
            if (strcmp(entry->d_name, EXCLUDED_DIRS[i]) == 0) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }
        
        // Vérifier si le nom correspond à la recherche
        char lower_name[256];
        for (int i = 0; entry->d_name[i] && i < 255; i++) {
            lower_name[i] = tolower(entry->d_name[i]);
            lower_name[i + 1] = '\0';
        }
        
        bool matches = strstr(lower_name, lower_search) != NULL;
        
        if (matches) {
            FileEntry file_entry;
            strncpy(file_entry.path, full_path, sizeof(file_entry.path) - 1);
            file_entry.path[sizeof(file_entry.path) - 1] = '\0';
            
            strncpy(file_entry.name, entry->d_name, sizeof(file_entry.name) - 1);
            file_entry.name[sizeof(file_entry.name) - 1] = '\0';
            
            file_entry.size = st.st_size;
            file_entry.depth = depth;
            
            if (S_ISDIR(st.st_mode)) {
                file_entry.type = FILE_TYPE_DIRECTORY;
            } else {
                file_entry.type = FILE_TYPE_FILE;
            }
            
            file_list_add(list, &file_entry);
        }
        
        // Continuer la recherche récursive dans les sous-dossiers
        if (S_ISDIR(st.st_mode)) {
            if (!search_files_recursive(full_path, search_term, list, depth + 1)) {
                closedir(dir);
                return false;  // Limite atteinte
            }
        }
    }
    
    closedir(dir);
    return true;
}

void file_list_clear(FileList* list) {
    if (list) {
        list->count = 0;
    }
}

static int compare_entries(const void* a, const void* b) {
    const FileEntry* entry_a = (const FileEntry*)a;
    const FileEntry* entry_b = (const FileEntry*)b;
    
    // Les dossiers en premier
    if (entry_a->type != entry_b->type) {
        return entry_a->type == FILE_TYPE_DIRECTORY ? -1 : 1;
    }
    
    // Puis par profondeur
    if (entry_a->depth != entry_b->depth) {
        return entry_a->depth - entry_b->depth;
    }
    
    // Enfin par nom
    return strcmp(entry_a->name, entry_b->name);
}

void file_list_sort(FileList* list) {
    if (list && list->entries && list->count > 0) {
        qsort(list->entries, list->count, sizeof(FileEntry), compare_entries);
    }
}
