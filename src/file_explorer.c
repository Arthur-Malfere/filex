#include "file_explorer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

// Dossiers à exclure de la recherche récursive (optimisation pour macOS/Linux)
static const char* EXCLUDED_DIRS[] = {
    // Dépendances et builds
    "node_modules",
    "build",
    "dist",
    "target",
    ".gradle",
    ".m2",
    "vendor",
    "Pods",
    // VCS
    ".git",
    ".svn",
    ".hg",
    // Python
    "__pycache__",
    ".venv",
    "venv",
    "env",
    ".tox",
    // Cache
    ".cache",
    ".npm",
    ".yarn",
    // macOS système (éviter bottleneck)
    "Library",
    "System",
    "Applications",
    "Volumes",
    ".Spotlight-V100",
    ".DocumentRevisions-V100",
    ".fseventsd",
    ".TemporaryItems",
    ".Trashes",
    // Autres volumineuses
    "Downloads",
    "Desktop",
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

// Fonction pour récupérer les métadonnées d'un fichier
static void get_file_metadata(const char* path, FileEntry* entry) {
    struct stat st;
    if (stat(path, &st) == 0) {
        entry->mod_time = st.st_mtime;
        entry->permissions = st.st_mode;
        entry->owner_uid = st.st_uid;
        entry->owner_gid = st.st_gid;
    } else {
        entry->mod_time = 0;
        entry->permissions = 0;
        entry->owner_uid = 0;
        entry->owner_gid = 0;
    }
}

bool explore_directory(const char* path, FileList* list, int depth, bool show_hidden) {
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
        
        // Ignorer les fichiers cachés si non demandé
        if (!show_hidden && entry->d_name[0] == '.') {
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
        
        // Récupérer les métadonnées
        get_file_metadata(full_path, &file_entry);
        
        if (S_ISDIR(st.st_mode)) {
            file_entry.type = FILE_TYPE_DIRECTORY;
            file_list_add(list, &file_entry);
            
            // Exploration récursive
            explore_directory(full_path, list, depth + 1, show_hidden);
        } else {
            file_entry.type = FILE_TYPE_FILE;
            file_list_add(list, &file_entry);
        }
    }
    
    closedir(dir);
    return true;
}

bool explore_directory_shallow(const char* path, FileList* list, bool show_hidden) {
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
        
        // Ignorer les fichiers cachés si non demandé
        if (!show_hidden && entry->d_name[0] == '.') {
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
        
        // Récupérer les métadonnées
        get_file_metadata(full_path, &file_entry);
        
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

bool search_files_recursive(const char* path, const char* search_term, FileList* list, int depth, bool show_hidden) {
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
        
        // Ignorer les fichiers cachés si non demandé
        if (!show_hidden && entry->d_name[0] == '.') {
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
            
            // Récupérer les métadonnées
            get_file_metadata(full_path, &file_entry);
            
            if (S_ISDIR(st.st_mode)) {
                file_entry.type = FILE_TYPE_DIRECTORY;
            } else {
                file_entry.type = FILE_TYPE_FILE;
            }
            
            file_list_add(list, &file_entry);
        }
        
        // Continuer la recherche récursive dans les sous-dossiers
        if (S_ISDIR(st.st_mode)) {
            if (!search_files_recursive(full_path, search_term, list, depth + 1, show_hidden)) {
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

static bool is_valid_name(const char* name) {
    if (!name || name[0] == '\0') return false;
    // Empêcher les séparateurs pour éviter chemins relatifs dangereux
    if (strchr(name, '/') || strchr(name, '\\')) return false;
    // Empêcher les noms réservés
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    return true;
}

bool create_directory(const char* parent_path, const char* name) {
    if (!parent_path || !is_valid_name(name)) return false;
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", parent_path, name);

    struct stat st;
    if (stat(full_path, &st) == 0) {
        // Existe déjà
        return false;
    }

    if (mkdir(full_path, 0777) == 0) {
        return true;
    }
    return false;
}

bool create_file(const char* parent_path, const char* name) {
    if (!parent_path || !is_valid_name(name)) return false;
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", parent_path, name);

    struct stat st;
    if (stat(full_path, &st) == 0) {
        // Existe déjà
        return false;
    }

    FILE* f = fopen(full_path, "w");
    if (!f) return false;
    fclose(f);
    return true;
}

// === Gestion du cache ===
DirectoryCache* cache_create(void) {
    DirectoryCache* cache = (DirectoryCache*)malloc(sizeof(DirectoryCache));
    if (!cache) return NULL;
    
    cache->count = 0;
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        cache->entries[i].path[0] = '\0';
        cache->entries[i].files = NULL;
        cache->entries[i].last_access = 0;
        cache->entries[i].show_hidden = false;
    }
    
    return cache;
}

void cache_destroy(DirectoryCache* cache) {
    if (!cache) return;
    
    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].files) {
            file_list_destroy(cache->entries[i].files);
        }
    }
    
    free(cache);
}

FileList* cache_get(DirectoryCache* cache, const char* path, bool show_hidden) {
    if (!cache || !path) return NULL;
    
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].path, path) == 0 && 
            cache->entries[i].show_hidden == show_hidden) {
            // Mettre à jour l'heure d'accès
            cache->entries[i].last_access = time(NULL);
            return cache->entries[i].files;
        }
    }
    
    return NULL;
}

void cache_put(DirectoryCache* cache, const char* path, FileList* files, bool show_hidden) {
    if (!cache || !path || !files) return;
    
    // Vérifier si déjà dans le cache
    for (int i = 0; i < cache->count; i++) {
        if (strcmp(cache->entries[i].path, path) == 0 && 
            cache->entries[i].show_hidden == show_hidden) {
            // Mettre à jour l'entrée existante
            if (cache->entries[i].files) {
                file_list_destroy(cache->entries[i].files);
            }
            cache->entries[i].files = files;
            cache->entries[i].last_access = time(NULL);
            return;
        }
    }
    
    // Ajouter une nouvelle entrée
    int index = -1;
    
    if (cache->count < MAX_CACHE_ENTRIES) {
        // Il y a de la place
        index = cache->count++;
    } else {
        // Cache plein, trouver la plus ancienne entrée
        time_t oldest_time = cache->entries[0].last_access;
        index = 0;
        
        for (int i = 1; i < MAX_CACHE_ENTRIES; i++) {
            if (cache->entries[i].last_access < oldest_time) {
                oldest_time = cache->entries[i].last_access;
                index = i;
            }
        }
        
        // Libérer l'ancienne entrée
        if (cache->entries[index].files) {
            file_list_destroy(cache->entries[index].files);
        }
    }
    
    // Ajouter la nouvelle entrée
    strncpy(cache->entries[index].path, path, MAX_PATH_LENGTH - 1);
    cache->entries[index].path[MAX_PATH_LENGTH - 1] = '\0';
    cache->entries[index].files = files;
    cache->entries[index].last_access = time(NULL);
    cache->entries[index].show_hidden = show_hidden;
}

// === Recherche par contenu ===
bool search_in_file_content(const char* file_path, const char* search_term) {
    if (!file_path || !search_term) return false;
    
    FILE* file = fopen(file_path, "rb");
    if (!file) return false;
    
    // Vérifier la taille du fichier
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Ignorer les fichiers trop gros
    if (file_size > MAX_CACHE_FILE_SIZE || file_size <= 0) {
        fclose(file);
        return false;
    }
    
    // Lire le contenu
    char* content = (char*)malloc(file_size + 1);
    if (!content) {
        fclose(file);
        return false;
    }
    
    size_t bytes_read = fread(content, 1, file_size, file);
    content[bytes_read] = '\0';
    fclose(file);
    
    // Vérifier si c'est du texte (pas binaire)
    bool is_binary = false;
    long check_size = bytes_read < 512 ? bytes_read : 512;
    for (long i = 0; i < check_size; i++) {
        unsigned char c = (unsigned char)content[i];
        if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
            is_binary = true;
            break;
        }
        if (c == 0) {
            is_binary = true;
            break;
        }
    }
    
    if (is_binary) {
        free(content);
        return false;
    }
    
    // Convertir en minuscules pour recherche insensible à la casse
    char lower_search[256];
    for (int i = 0; search_term[i] && i < 255; i++) {
        lower_search[i] = tolower(search_term[i]);
        lower_search[i + 1] = '\0';
    }
    
    // Rechercher dans le contenu
    bool found = false;
    for (size_t i = 0; i < bytes_read; i++) {
        char lower_c = tolower(content[i]);
        if (lower_c == lower_search[0]) {
            // Début potentiel de correspondance
            bool match = true;
            for (size_t j = 1; lower_search[j] != '\0' && (i + j) < bytes_read; j++) {
                if (tolower(content[i + j]) != lower_search[j]) {
                    match = false;
                    break;
                }
            }
            if (match && lower_search[1] == '\0') {
                found = true;
                break;
            }
            if (match) {
                // Vérifier qu'on a la chaîne complète
                size_t search_len = strlen(lower_search);
                if (i + search_len <= bytes_read) {
                    found = true;
                    break;
                }
            }
        }
    }
    
    free(content);
    return found;
}

bool search_files_by_content(const char* path, const char* search_term, FileList* list, int depth, bool show_hidden) {
    // Limites de sécurité
    if (depth > MAX_SEARCH_DEPTH) {
        return true;
    }
    
    if (list->count >= MAX_SEARCH_RESULTS) {
        return false;
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        return true;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (list->count >= MAX_SEARCH_RESULTS) {
            closedir(dir);
            return false;
        }
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (!show_hidden && entry->d_name[0] == '.') {
            continue;
        }
        
        // Vérifier exclusion
        bool excluded = false;
        for (int i = 0; EXCLUDED_DIRS[i] != NULL; i++) {
            if (strcmp(entry->d_name, EXCLUDED_DIRS[i]) == 0) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            // Recherche récursive
            if (!search_files_by_content(full_path, search_term, list, depth + 1, show_hidden)) {
                closedir(dir);
                return false;
            }
        } else {
            // Rechercher dans le contenu du fichier
            if (search_in_file_content(full_path, search_term)) {
                FileEntry file_entry;
                strncpy(file_entry.path, full_path, sizeof(file_entry.path) - 1);
                file_entry.path[sizeof(file_entry.path) - 1] = '\0';
                
                strncpy(file_entry.name, entry->d_name, sizeof(file_entry.name) - 1);
                file_entry.name[sizeof(file_entry.name) - 1] = '\0';
                
                file_entry.size = st.st_size;
                file_entry.depth = depth;
                file_entry.type = FILE_TYPE_FILE;
                
                // Récupérer les métadonnées
                get_file_metadata(full_path, &file_entry);
                
                file_list_add(list, &file_entry);
            }
        }
    }
    
    closedir(dir);
    return true;
}

// === Recherche asynchrone (threading) ===
typedef struct {
    AsyncSearch* search;
} SearchThreadData;

// Wrapper pour la recherche avec statistiques
static bool search_recursive_with_stats(AsyncSearch* search, const char* path, const char* search_term, FileList* list, int depth, bool show_hidden) {
    if (depth > MAX_SEARCH_DEPTH) {
        return true;
    }
    
    if (list->count >= MAX_SEARCH_RESULTS) {
        return false;
    }
    
    // Vérifier si annulé
    pthread_mutex_lock(&search->mutex);
    bool cancelled = (search->status == SEARCH_CANCELLED);
    pthread_mutex_unlock(&search->mutex);
    
    if (cancelled) {
        return false;
    }
    
    DIR* dir = opendir(path);
    if (!dir) {
        return true;
    }
    
    // Incrémenter le compteur de dossiers
    pthread_mutex_lock(&search->mutex);
    search->dirs_scanned++;
    pthread_mutex_unlock(&search->mutex);
    
    // Convertir le terme de recherche en minuscules
    char lower_search[256];
    for (int i = 0; search_term[i] && i < 255; i++) {
        lower_search[i] = tolower(search_term[i]);
        lower_search[i + 1] = '\0';
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (list->count >= MAX_SEARCH_RESULTS) {
            closedir(dir);
            return false;
        }
        
        // Vérifier si annulé
        pthread_mutex_lock(&search->mutex);
        cancelled = (search->status == SEARCH_CANCELLED);
        pthread_mutex_unlock(&search->mutex);
        
        if (cancelled) {
            closedir(dir);
            return false;
        }
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        if (!show_hidden && entry->d_name[0] == '.') {
            continue;
        }
        
        // Vérifier exclusion
        bool excluded = false;
        for (int i = 0; EXCLUDED_DIRS[i] != NULL; i++) {
            if (strcmp(entry->d_name, EXCLUDED_DIRS[i]) == 0) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;
        
        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == -1) {
            continue;
        }
        
        // Incrémenter le compteur de fichiers scannés
        if (!S_ISDIR(st.st_mode)) {
            pthread_mutex_lock(&search->mutex);
            search->files_scanned++;
            pthread_mutex_unlock(&search->mutex);
        }
        
        // Vérifier si le nom correspond
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
            
            // Récupérer les métadonnées
            get_file_metadata(full_path, &file_entry);
            
            if (S_ISDIR(st.st_mode)) {
                file_entry.type = FILE_TYPE_DIRECTORY;
            } else {
                file_entry.type = FILE_TYPE_FILE;
            }
            
            file_list_add(list, &file_entry);
            
            pthread_mutex_lock(&search->mutex);
            search->files_matched++;
            pthread_mutex_unlock(&search->mutex);
        }
        
        // Continuer la recherche récursive
        if (S_ISDIR(st.st_mode)) {
            if (!search_recursive_with_stats(search, full_path, search_term, list, depth + 1, show_hidden)) {
                closedir(dir);
                return false;
            }
        }
    }
    
    closedir(dir);
    return true;
}

static void* search_thread_function(void* arg) {
    SearchThreadData* data = (SearchThreadData*)arg;
    AsyncSearch* search = data->search;
    
    pthread_mutex_lock(&search->mutex);
    if (search->status != SEARCH_RUNNING) {
        pthread_mutex_unlock(&search->mutex);
        free(data);
        return NULL;
    }
    search->start_time = time(NULL);
    pthread_mutex_unlock(&search->mutex);
    
    // Effectuer la recherche
    bool limit_reached;
    if (search->search_by_content) {
        limit_reached = !search_files_by_content(
            search->path, 
            search->search_term, 
            search->results, 
            0, 
            search->show_hidden
        );
    } else {
        limit_reached = !search_recursive_with_stats(
            search,
            search->path, 
            search->search_term, 
            search->results, 
            0, 
            search->show_hidden
        );
    }
    
    // Trier les résultats
    file_list_sort(search->results);
    
    pthread_mutex_lock(&search->mutex);
    if (search->status == SEARCH_RUNNING) {
        search->limit_reached = limit_reached;
        search->elapsed_time = difftime(time(NULL), search->start_time);
        search->status = SEARCH_COMPLETED;
    }
    pthread_mutex_unlock(&search->mutex);
    
    free(data);
    return NULL;
}

AsyncSearch* async_search_create(void) {
    AsyncSearch* search = (AsyncSearch*)malloc(sizeof(AsyncSearch));
    if (!search) return NULL;
    
    if (pthread_mutex_init(&search->mutex, NULL) != 0) {
        free(search);
        return NULL;
    }
    
    search->status = SEARCH_IDLE;
    search->path[0] = '\0';
    search->search_term[0] = '\0';
    search->search_by_content = false;
    search->show_hidden = false;
    search->results = NULL;
    search->limit_reached = false;
    search->files_scanned = 0;
    search->dirs_scanned = 0;
    search->files_matched = 0;
    search->start_time = 0;
    search->elapsed_time = 0.0;
    
    return search;
}

void async_search_start(AsyncSearch* search, const char* path, const char* search_term, bool search_by_content, bool show_hidden) {
    if (!search || !path || !search_term) return;
    
    pthread_mutex_lock(&search->mutex);
    
    // Annuler toute recherche en cours
    if (search->status == SEARCH_RUNNING) {
        search->status = SEARCH_CANCELLED;
        pthread_mutex_unlock(&search->mutex);
        pthread_join(search->thread, NULL);
        pthread_mutex_lock(&search->mutex);
    }
    
    // Nettoyer les anciens résultats
    if (search->results) {
        file_list_destroy(search->results);
    }
    
    // Créer une nouvelle liste de résultats
    search->results = file_list_create();
    if (!search->results) {
        pthread_mutex_unlock(&search->mutex);
        return;
    }
    
    // Copier les paramètres
    strncpy(search->path, path, MAX_PATH_LENGTH - 1);
    search->path[MAX_PATH_LENGTH - 1] = '\0';
    
    strncpy(search->search_term, search_term, 255);
    search->search_term[255] = '\0';
    
    search->search_by_content = search_by_content;
    search->show_hidden = show_hidden;
    search->limit_reached = false;
    search->files_scanned = 0;
    search->dirs_scanned = 0;
    search->files_matched = 0;
    search->start_time = time(NULL);
    search->elapsed_time = 0.0;
    search->status = SEARCH_RUNNING;
    
    // Créer et lancer le thread
    SearchThreadData* data = (SearchThreadData*)malloc(sizeof(SearchThreadData));
    data->search = search;
    
    if (pthread_create(&search->thread, NULL, search_thread_function, data) != 0) {
        free(data);
        search->status = SEARCH_IDLE;
        pthread_mutex_unlock(&search->mutex);
        return;
    }
    
    pthread_mutex_unlock(&search->mutex);
}

SearchStatus async_search_status(AsyncSearch* search) {
    if (!search) return SEARCH_IDLE;
    
    pthread_mutex_lock(&search->mutex);
    SearchStatus status = search->status;
    pthread_mutex_unlock(&search->mutex);
    
    return status;
}

FileList* async_search_get_results(AsyncSearch* search, bool* limit_reached) {
    if (!search) return NULL;
    
    pthread_mutex_lock(&search->mutex);
    
    if (search->status != SEARCH_COMPLETED) {
        pthread_mutex_unlock(&search->mutex);
        return NULL;
    }
    
    FileList* results = search->results;
    if (limit_reached) {
        *limit_reached = search->limit_reached;
    }
    
    search->results = NULL;
    search->status = SEARCH_IDLE;
    
    pthread_mutex_unlock(&search->mutex);
    
    return results;
}

void async_search_get_progress(AsyncSearch* search, int* files_scanned, int* dirs_scanned, int* files_matched, double* elapsed_time) {
    if (!search) return;
    
    pthread_mutex_lock(&search->mutex);
    
    if (files_scanned) *files_scanned = search->files_scanned;
    if (dirs_scanned) *dirs_scanned = search->dirs_scanned;
    if (files_matched) *files_matched = search->files_matched;
    if (elapsed_time) {
        if (search->status == SEARCH_RUNNING) {
            *elapsed_time = difftime(time(NULL), search->start_time);
        } else {
            *elapsed_time = search->elapsed_time;
        }
    }
    
    pthread_mutex_unlock(&search->mutex);
}

FileList* async_search_peek_results(AsyncSearch* search) {
    if (!search) return NULL;
    
    pthread_mutex_lock(&search->mutex);
    
    if (!search->results || search->results->count == 0) {
        pthread_mutex_unlock(&search->mutex);
        return NULL;
    }
    
    // Créer une copie des résultats actuels
    FileList* copy = file_list_create();
    if (!copy) {
        pthread_mutex_unlock(&search->mutex);
        return NULL;
    }
    
    for (int i = 0; i < search->results->count && i < MAX_FILES; i++) {
        if (copy->count >= copy->capacity) {
            int new_capacity = copy->capacity * 2;
            if (new_capacity > MAX_FILES) new_capacity = MAX_FILES;
            FileEntry* new_entries = (FileEntry*)realloc(copy->entries, sizeof(FileEntry) * new_capacity);
            if (!new_entries) break;
            copy->entries = new_entries;
            copy->capacity = new_capacity;
        }
        copy->entries[copy->count++] = search->results->entries[i];
    }
    
    pthread_mutex_unlock(&search->mutex);
    
    return copy;
}

void async_search_cancel(AsyncSearch* search) {
    if (!search) return;
    
    pthread_mutex_lock(&search->mutex);
    
    if (search->status == SEARCH_RUNNING) {
        search->status = SEARCH_CANCELLED;
        pthread_mutex_unlock(&search->mutex);
        pthread_join(search->thread, NULL);
    } else {
        pthread_mutex_unlock(&search->mutex);
    }
}

void async_search_destroy(AsyncSearch* search) {
    if (!search) return;
    
    async_search_cancel(search);
    
    if (search->results) {
        file_list_destroy(search->results);
    }
    
    pthread_mutex_destroy(&search->mutex);
    free(search);
}
