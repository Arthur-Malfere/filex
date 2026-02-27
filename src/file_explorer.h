#ifndef FILE_EXPLORER_H
#define FILE_EXPLORER_H

#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 1024
#define MAX_FILES 10000
#define MAX_SEARCH_RESULTS 5000  // Augmenté pour permettre plus de résultats
#define MAX_SEARCH_DEPTH 15      // Augmenté pour chercher plus profond
#define MAX_CACHE_ENTRIES 10
#define MAX_CACHE_FILE_SIZE 1048576  // 1MB max pour le cache
#define SEARCH_UPDATE_INTERVAL 100  // Mettre à jour l'UI tous les 100 fichiers

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
    // Métadonnées
    time_t mod_time;           // Date de modification
    mode_t permissions;         // Permissions (mode)
    uid_t owner_uid;           // UID du propriétaire
    gid_t owner_gid;           // GID du groupe
} FileEntry;

typedef struct {
    FileEntry* entries;
    int count;
    int capacity;
} FileList;

// Structure pour le cache de répertoires
typedef struct {
    char path[MAX_PATH_LENGTH];
    FileList* files;
    time_t last_access;
    bool show_hidden;
} CacheEntry;

typedef struct {
    CacheEntry entries[MAX_CACHE_ENTRIES];
    int count;
} DirectoryCache;

// Structure pour la recherche asynchrone
typedef enum {
    SEARCH_IDLE,
    SEARCH_RUNNING,
    SEARCH_COMPLETED,
    SEARCH_CANCELLED
} SearchStatus;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    SearchStatus status;
    char path[MAX_PATH_LENGTH];
    char search_term[256];
    bool search_by_content;
    bool show_hidden;
    FileList* results;
    bool limit_reached;
    // Statistiques de progression
    int files_scanned;
    int dirs_scanned;
    int files_matched;
    time_t start_time;
    double elapsed_time;
} AsyncSearch;

// Initialise une liste de fichiers
FileList* file_list_create(void);

// Libère la mémoire d'une liste de fichiers
void file_list_destroy(FileList* list);

// Explore un répertoire de manière récursive
bool explore_directory(const char* path, FileList* list, int depth, bool show_hidden);

// Explore seulement le contenu direct d'un répertoire (non-récursif)
bool explore_directory_shallow(const char* path, FileList* list, bool show_hidden);

// Recherche récursive de fichiers par nom (retourne false si limite atteinte)
bool search_files_recursive(const char* path, const char* search_term, FileList* list, int depth, bool show_hidden);

// Efface le contenu de la liste
void file_list_clear(FileList* list);

// Trie la liste par nom
void file_list_sort(FileList* list);

// Crée un nouveau dossier dans le chemin parent
bool create_directory(const char* parent_path, const char* name);

// Crée un nouveau fichier dans le chemin parent
bool create_file(const char* parent_path, const char* name);

// === Gestion du cache ===
// Initialise le cache
DirectoryCache* cache_create(void);

// Libère le cache
void cache_destroy(DirectoryCache* cache);

// Récupère depuis le cache (NULL si non trouvé)
FileList* cache_get(DirectoryCache* cache, const char* path, bool show_hidden);

// Ajoute au cache
void cache_put(DirectoryCache* cache, const char* path, FileList* files, bool show_hidden);

// === Recherche par contenu ===
// Recherche dans le contenu des fichiers (grep-like)
bool search_in_file_content(const char* file_path, const char* search_term);

// Recherche récursive par contenu
bool search_files_by_content(const char* path, const char* search_term, FileList* list, int depth, bool show_hidden);

// === Recherche asynchrone (threading) ===
// Crée une recherche asynchrone
AsyncSearch* async_search_create(void);

// Démarre une recherche asynchrone
void async_search_start(AsyncSearch* search, const char* path, const char* search_term, bool search_by_content, bool show_hidden);

// Vérifie le statut de la recherche
SearchStatus async_search_status(AsyncSearch* search);

// Récupère les résultats (à appeler quand status == SEARCH_COMPLETED)
FileList* async_search_get_results(AsyncSearch* search, bool* limit_reached);

// Obtient les statistiques de progression (thread-safe)
void async_search_get_progress(AsyncSearch* search, int* files_scanned, int* dirs_scanned, int* files_matched, double* elapsed_time);

// Obtient une copie des résultats intermédiaires (pour affichage progressif)
FileList* async_search_peek_results(AsyncSearch* search);

// Annule une recherche en cours
void async_search_cancel(AsyncSearch* search);

// Libère les ressources
void async_search_destroy(AsyncSearch* search);

#endif // FILE_EXPLORER_H
