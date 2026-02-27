#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define FONT_SIZE 20
#define LINE_HEIGHT 25
#define PADDING 10
#define INDENT_SIZE 20

static ThemeColors get_theme_colors(Theme theme) {
    ThemeColors colors;
    
    if (theme == THEME_LIGHT) {
        colors.bg_primary = RAYWHITE;
        colors.bg_secondary = (Color){230, 230, 230, 255};
        colors.text_primary = BLACK;
        colors.text_secondary = DARKGRAY;
        colors.text_disabled = LIGHTGRAY;
        colors.highlight = (Color){173, 216, 230, 255}; // Light blue
        colors.highlight_hover = (Color){100, 180, 255, 255};
        colors.accent = SKYBLUE;
        colors.border = LIGHTGRAY;
    } else {
        // THEME_DARK
        colors.bg_primary = (Color){30, 30, 30, 255};
        colors.bg_secondary = (Color){45, 45, 45, 255};
        colors.text_primary = WHITE;
        colors.text_secondary = (Color){200, 200, 200, 255};
        colors.text_disabled = (Color){100, 100, 100, 255};
        colors.highlight = (Color){70, 130, 180, 255}; // Steel blue
        colors.highlight_hover = (Color){100, 150, 220, 255};
        colors.accent = (Color){100, 180, 255, 255}; // Light blue accent
        colors.border = (Color){60, 60, 60, 255};
    }
    
    return colors;
}

static void format_time(time_t time_val, char* buffer, size_t buffer_size) {
    if (time_val == 0) {
        snprintf(buffer, buffer_size, "---");
        return;
    }
    
    struct tm* tm_info = localtime(&time_val);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M", tm_info);
}

static void format_permissions(mode_t mode, char* buffer, size_t buffer_size) {
    if (buffer_size < 10) return;
    
    buffer[0] = (S_ISDIR(mode)) ? 'd' : (S_ISLNK(mode)) ? 'l' : '-';
    buffer[1] = (mode & S_IRUSR) ? 'r' : '-';
    buffer[2] = (mode & S_IWUSR) ? 'w' : '-';
    buffer[3] = (mode & S_IXUSR) ? 'x' : '-';
    buffer[4] = (mode & S_IRGRP) ? 'r' : '-';
    buffer[5] = (mode & S_IWGRP) ? 'w' : '-';
    buffer[6] = (mode & S_IXGRP) ? 'x' : '-';
    buffer[7] = (mode & S_IROTH) ? 'r' : '-';
    buffer[8] = (mode & S_IWOTH) ? 'w' : '-';
    buffer[9] = (mode & S_IXOTH) ? 'x' : '-';
    buffer[10] = '\0';
}

UIState* ui_init(int width, int height, const char* title) {
    UIState* state = (UIState*)malloc(sizeof(UIState));
    if (!state) return NULL;
    
    state->window_width = width;
    state->window_height = height;
    state->scroll_offset = 0;
    state->selected_index = -1;
    state->clicked_path = NULL;
    state->go_back = false;
    state->search_text[0] = '\0';
    state->search_active = false;
    state->is_searching = false;
    state->search_limit_reached = false;
    state->selected_file_path = NULL;
    state->file_content = NULL;
    state->is_binary_file = false;
    state->file_size = 0;
    state->file_scroll_offset = 0;
    state->initialized = false;
    state->show_hidden = false;
    state->search_by_content = false;
    state->search_files_scanned = 0;
    state->search_dirs_scanned = 0;
    state->search_files_matched = 0;
    state->search_elapsed_time = 0.0;
    state->current_theme = THEME_LIGHT;
    state->colors = get_theme_colors(THEME_LIGHT);
    state->menu_active = false;
    state->menu_x = 0;
    state->menu_y = 0;
    state->create_active = false;
    state->create_confirmed = false;
    state->create_type = CREATE_NONE;
    state->create_name[0] = '\0';
    
    InitWindow(width, height, title);
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);
    
    state->font = GetFontDefault();
    state->initialized = true;
    
    return state;
}

void ui_destroy(UIState* state) {
    if (state && state->initialized) {
        if (state->clicked_path) {
            free(state->clicked_path);
        }
        if (state->selected_file_path) {
            free(state->selected_file_path);
        }
        if (state->file_content) {
            free(state->file_content);
        }
        CloseWindow();
        free(state);
    }
}

static void format_size(long bytes, char* buffer, size_t buffer_size) {
    if (bytes < 1024) {
        snprintf(buffer, buffer_size, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

static Color get_file_color(FileType type) {
    return type == FILE_TYPE_DIRECTORY ? BLUE : DARKGRAY;
}

// Vérifier si un fichier est caché (commence par '.')
static bool is_hidden_file(const char* filename) {
    return filename && filename[0] == '.';
}

// Obtenir la couleur de texte adaptée au thème pour fichiers cachés/sélectionnés
static Color get_text_color_for_entry(UIState* state, const char* filename, bool is_selected) {
    if (is_selected) {
        return state->colors.text_primary;
    }
    
    if (is_hidden_file(filename)) {
        // Pour les fichiers cachés: utiliser une couleur plus atténuée avec le thème
        return state->colors.text_secondary;
    }
    
    return state->colors.text_primary;
}

// Obtenir l'opacité pour un fichier (0.5 pour cachés, 1.0 pour normaux)
static float get_entry_opacity(const char* filename) {
    return is_hidden_file(filename) ? 0.6f : 1.0f;
}

static bool is_binary_content(const char* content, long size) {
    // Vérifier les premiers octets pour détecter du contenu binaire
    long check_size = size < 512 ? size : 512;
    for (long i = 0; i < check_size; i++) {
        unsigned char c = (unsigned char)content[i];
        // Si on trouve des caractères de contrôle (sauf \n, \r, \t), c'est binaire
        if (c < 32 && c != '\n' && c != '\r' && c != '\t' && c != 0) {
            return true;
        }
        if (c == 0) {
            return true; // NULL byte = binaire
        }
    }
    return false;
}

static bool load_file_content(UIState* state, const char* file_path) {
    // Libérer l'ancien contenu
    if (state->file_content) {
        free(state->file_content);
        state->file_content = NULL;
    }
    if (state->selected_file_path) {
        free(state->selected_file_path);
    }
    
    // Ouvrir le fichier
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        return false;
    }
    
    // Obtenir la taille
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    state->file_size = size;
    
    // Limiter la taille de lecture à 1 MB
    long read_size = size < 1048576 ? size : 1048576;
    
    // Lire le contenu
    state->file_content = (char*)malloc(read_size + 1);
    if (!state->file_content) {
        fclose(file);
        return false;
    }
    
    size_t bytes_read = fread(state->file_content, 1, read_size, file);
    state->file_content[bytes_read] = '\0';
    fclose(file);
    
    // Vérifier si binaire
    state->is_binary_file = is_binary_content(state->file_content, bytes_read);
    
    // Sauvegarder le chemin
    state->selected_file_path = (char*)malloc(strlen(file_path) + 1);
    if (state->selected_file_path) {
        strcpy(state->selected_file_path, file_path);
    }
    
    state->file_scroll_offset = 0;
    return true;
}

static bool matches_search(const char* filename, const char* search) {
    if (search[0] == '\0') return true;
    
    // Recherche insensible à la casse
    char lower_filename[256];
    char lower_search[256];
    
    for (int i = 0; filename[i] && i < 255; i++) {
        lower_filename[i] = tolower(filename[i]);
        lower_filename[i + 1] = '\0';
    }
    
    for (int i = 0; search[i] && i < 255; i++) {
        lower_search[i] = tolower(search[i]);
        lower_search[i + 1] = '\0';
    }
    
    return strstr(lower_filename, lower_search) != NULL;
}

void ui_render(UIState* state, FileList* files, const char* current_path) {
    if (!state || !files) return;
    
    // Mettre à jour les dimensions si la fenêtre a été redimensionnée
    if (IsWindowResized()) {
        state->window_width = GetScreenWidth();
        state->window_height = GetScreenHeight();
    }
    
    // Réinitialiser le chemin cliqué et go_back
    if (state->clicked_path) {
        free(state->clicked_path);
        state->clicked_path = NULL;
    }
    state->go_back = false;
    
    // Gestion de l'input pour la création (prioritaire)
    if (state->create_active) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 125) {
                int len = strlen(state->create_name);
                if (len < 254) {
                    state->create_name[len] = (char)key;
                    state->create_name[len + 1] = '\0';
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = strlen(state->create_name);
            if (len > 0) state->create_name[len - 1] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (state->create_name[0] != '\0') {
                state->create_confirmed = true;
            }
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            state->create_active = false;
            state->create_confirmed = false;
            state->create_type = CREATE_NONE;
            state->create_name[0] = '\0';
        }
    }

    // Gestion de l'input clavier pour la recherche
    if (!state->create_active && state->search_active) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 125) {
                int len = strlen(state->search_text);
                if (len < 254) {
                    state->search_text[len] = (char)key;
                    state->search_text[len + 1] = '\0';
                }
            }
            key = GetCharPressed();
        }
        
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = strlen(state->search_text);
            if (len > 0) {
                state->search_text[len - 1] = '\0';
            }
        }
        
        if (IsKeyPressed(KEY_ESCAPE)) {
            state->search_active = false;
            state->search_text[0] = '\0';
        }
    } else if (!state->create_active) {
        // Activer la recherche avec Ctrl+F ou Cmd+F
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_F)) {
            state->search_active = true;
        }
    }

    // Raccourci clavier pour toggle cachés: Ctrl/Cmd + H
    if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_H)) {
        state->show_hidden = !state->show_hidden;
    }
    
    // Détection du clic droit pour menu contextuel
    if (!state->create_active && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        state->menu_active = true;
        Vector2 mouse_pos = GetMousePosition();
        state->menu_x = (int)mouse_pos.x;
        state->menu_y = (int)mouse_pos.y;
    }
    
    // Gestion du scroll avec la molette
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        state->scroll_offset -= (int)(wheel * LINE_HEIGHT * 3);
        if (state->scroll_offset < 0) {
            state->scroll_offset = 0;
        }
        
        int max_scroll = (files->count * LINE_HEIGHT) - state->window_height + 150;
        if (max_scroll < 0) max_scroll = 0;
        if (state->scroll_offset > max_scroll) {
            state->scroll_offset = max_scroll;
        }
    }
    
    BeginDrawing();
    ClearBackground(state->colors.bg_primary);
    
    // En-tête avec bouton retour et toggle thème
    DrawRectangle(0, 0, state->window_width, 40, state->colors.bg_secondary);
    DrawText("Explorateur de Fichiers", PADDING + 100, 10, FONT_SIZE, state->colors.text_primary);
    
    // Bouton retour
    Rectangle back_button = {PADDING, 8, 80, 24};
    Color back_color = state->colors.bg_secondary;
    if (CheckCollisionPointRec(GetMousePosition(), back_button)) {
        back_color = state->colors.highlight_hover;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state->go_back = true;
        }
    }
    DrawRectangleRec(back_button, back_color);
    DrawText("< Retour", PADDING + 5, 12, 16, state->colors.text_primary);
    
    // Bouton toggle thème (à droite)
    Rectangle theme_button = {(float)(state->window_width - 40), 8, 32, 24};
    Color theme_color = state->colors.bg_secondary;
    if (CheckCollisionPointRec(GetMousePosition(), theme_button)) {
        theme_color = state->colors.highlight_hover;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            ui_toggle_theme(state);
        }
    }
    DrawRectangleRec(theme_button, theme_color);
    DrawText(state->current_theme == THEME_LIGHT ? "☀" : "🌙", (int)theme_button.x + 8, 10, 16, state->colors.text_primary);
    
    // Chemin actuel
    DrawRectangle(0, 40, state->window_width, 35, state->colors.bg_secondary);
    char path_display[512];
    snprintf(path_display, sizeof(path_display), "Chemin: %s", current_path);
    DrawText(path_display, PADDING, 48, 18, state->colors.text_primary);
    
    // Statistiques
    DrawRectangle(0, 75, state->window_width, 25, state->colors.bg_secondary);
    char stats[256];
    int dir_count = 0, file_count = 0;
    for (int i = 0; i < files->count; i++) {
        if (files->entries[i].type == FILE_TYPE_DIRECTORY) {
            dir_count++;
        } else {
            file_count++;
        }
    }
    snprintf(stats, sizeof(stats), "%d dossiers, %d fichiers", dir_count, file_count);
    DrawText(stats, PADDING, 80, 16, state->colors.text_primary);

    // Toggle 'Afficher fichiers cachés'
    int toggle_width = 220;
    Rectangle hidden_toggle = {(float)(state->window_width - toggle_width - PADDING), 78, (float)toggle_width, 20};
    Color toggle_bg = Fade(state->colors.bg_primary, 0.5f);
    DrawRectangleRec(hidden_toggle, toggle_bg);
    DrawRectangleLinesEx(hidden_toggle, 1, state->colors.border);
    // Checkbox
    Rectangle cb = {hidden_toggle.x + 6, hidden_toggle.y + 3, 14, 14};
    DrawRectangleLinesEx(cb, 2, state->colors.text_primary);
    if (state->show_hidden) {
        DrawRectangle(cb.x + 3, cb.y + 3, cb.width - 6, cb.height - 6, state->colors.accent);
    }
    DrawText("Afficher fichiers caches", (int)(cb.x + 24), (int)(hidden_toggle.y + 2), 14, state->colors.text_primary);
    if (CheckCollisionPointRec(GetMousePosition(), hidden_toggle) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->show_hidden = !state->show_hidden;
    }
    
    // Barre de recherche
    int search_y = 100;
    int search_height = 35;
    Rectangle search_box = {PADDING, (float)search_y + 5, (float)state->window_width - 2 * PADDING, (float)search_height - 10};
    
    Color search_bg = state->search_active ? state->colors.bg_primary : Fade(state->colors.bg_primary, 0.8f);
    DrawRectangleRec(search_box, search_bg);
    DrawRectangleLinesEx(search_box, 2, state->search_active ? state->colors.accent : state->colors.border);
    
    // Icône de recherche (loupe dessinée)
    int icon_x = PADDING + 8;
    int icon_y = search_y + 10;
    DrawCircle(icon_x + 5, icon_y + 5, 5, BLANK);
    DrawCircleLines(icon_x + 5, icon_y + 5, 5, state->colors.text_secondary);
    DrawLineEx((Vector2){icon_x + 9, icon_y + 9}, (Vector2){icon_x + 13, icon_y + 13}, 2, state->colors.text_secondary);
    
    // Texte de recherche
    if (state->search_text[0] != '\0') {
        DrawText(state->search_text, PADDING + 35, search_y + 10, 16, state->colors.text_primary);
        
        // Curseur clignotant
        if (state->search_active && ((int)(GetTime() * 2) % 2 == 0)) {
            int text_width = MeasureText(state->search_text, 16);
            DrawText("|", PADDING + 35 + text_width, search_y + 10, 16, state->colors.text_primary);
        }
    } else if (state->search_active) {
        // Curseur seul
        if ((int)(GetTime() * 2) % 2 == 0) {
            DrawText("|", PADDING + 35, search_y + 10, 16, state->colors.text_primary);
        }
    } else {
        // Placeholder
        DrawText("Rechercher... (Ctrl+F)", PADDING + 35, search_y + 10, 16, state->colors.text_disabled);
    }
    
    // Compteur de résultats
    if (state->search_text[0] != '\0') {
        char count_text[128];
        if (state->search_limit_reached) {
            snprintf(count_text, sizeof(count_text), "%d+ resultats (limite)", files->count);
        } else if (state->is_searching) {
            snprintf(count_text, sizeof(count_text), "%d resultat%s (recherche...)", files->count, files->count > 1 ? "s" : "");
        } else {
            snprintf(count_text, sizeof(count_text), "%d resultat%s", files->count, files->count > 1 ? "s" : "");
        }
        int count_width = MeasureText(count_text, 14);
        Color count_color = state->search_limit_reached ? ORANGE : (state->is_searching ? state->colors.accent : state->colors.text_secondary);
        DrawText(count_text, state->window_width - count_width - PADDING - 10, search_y + 12, 14, count_color);
    }
    
    // Toggle recherche par contenu (sous la barre de recherche)
    int toggle_content_y = search_y + search_height + 5;
    Rectangle content_toggle = {PADDING, (float)toggle_content_y, 200, 20};
    Color toggle_content_bg = Fade(state->colors.bg_primary, 0.5f);
    DrawRectangleRec(content_toggle, toggle_content_bg);
    DrawRectangleLinesEx(content_toggle, 1, state->colors.border);
    
    // Checkbox
    Rectangle content_cb = {content_toggle.x + 6, content_toggle.y + 3, 14, 14};
    DrawRectangleLinesEx(content_cb, 2, state->colors.text_secondary);
    if (state->search_by_content) {
        DrawRectangle(content_cb.x + 3, content_cb.y + 3, content_cb.width - 6, content_cb.height - 6, ORANGE);
    }
    DrawText("Chercher dans contenu", (int)(content_cb.x + 24), (int)(content_toggle.y + 2), 14, state->colors.text_primary);
    if (CheckCollisionPointRec(GetMousePosition(), content_toggle) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->search_by_content = !state->search_by_content;
        // Déclencher une nouvelle recherche si on est en mode recherche
        if (state->search_text[0] != '\0') {
            // La recherche sera relancée dans la boucle principale
        }
    }
    
    // Barre de progression si recherche en cours
    int progress_y = toggle_content_y + 25;
    if (state->is_searching) {
        DrawRectangle(PADDING, progress_y, state->window_width - 2 * PADDING, 25, Fade(state->colors.accent, 0.1f));
        
        char progress_text[256];
        if (state->search_by_content) {
            snprintf(progress_text, sizeof(progress_text), 
                    "Scan: %d fichiers, %d dossiers | Trouvés: %d | Temps: %.1fs",
                    state->search_files_scanned, state->search_dirs_scanned, 
                    state->search_files_matched, state->search_elapsed_time);
        } else {
            snprintf(progress_text, sizeof(progress_text), 
                    "Scan: %d fichiers, %d dossiers | Trouvés: %d | Temps: %.1fs",
                    state->search_files_scanned, state->search_dirs_scanned, 
                    state->search_files_matched, state->search_elapsed_time);
        }
        DrawText(progress_text, PADDING + 5, progress_y + 5, 14, state->colors.accent);
        
        // Animation de chargement
        int spinner_x = state->window_width - 40;
        int spinner_y = progress_y + 12;
        float angle = (float)((int)(GetTime() * 500) % 360);
        DrawCircleSector((Vector2){spinner_x, spinner_y}, 8, angle, angle + 270, 16, BLUE);
    }
    
    // Détection du clic sur la barre de recherche
    if (!state->create_active && !state->menu_active && CheckCollisionPointRec(GetMousePosition(), search_box) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->search_active = true;
        state->menu_active = false;
    }

    // Menu contextuel
    if (state->menu_active) {
        int menu_item_height = 30;
        Rectangle menu_bg = {(float)state->menu_x, (float)state->menu_y, 180, (float)(menu_item_height * 2)};
        DrawRectangleRec(menu_bg, state->colors.bg_secondary);
        DrawRectangleLinesEx(menu_bg, 2, state->colors.border);
        
        // Item 1: Nouveau dossier
        Rectangle item1 = {(float)state->menu_x, (float)state->menu_y, 180, (float)menu_item_height};
        Color item1_color = BLANK;
        if (CheckCollisionPointRec(GetMousePosition(), item1)) {
            item1_color = Fade(state->colors.accent, 0.3f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state->menu_active = false;
                state->create_active = true;
                state->create_confirmed = false;
                state->create_type = CREATE_DIRECTORY;
                state->create_name[0] = '\0';
                state->search_active = false;
            }
        }
        DrawRectangleRec(item1, item1_color);
        DrawText("Nouveau dossier", (int)state->menu_x + 10, (int)state->menu_y + 7, 14, state->colors.text_primary);
        
        // Item 2: Nouveau fichier
        Rectangle item2 = {(float)state->menu_x, (float)(state->menu_y + menu_item_height), 180, (float)menu_item_height};
        Color item2_color = BLANK;
        if (CheckCollisionPointRec(GetMousePosition(), item2)) {
            item2_color = Fade(state->colors.accent, 0.3f);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state->menu_active = false;
                state->create_active = true;
                state->create_confirmed = false;
                state->create_type = CREATE_FILE;
                state->create_name[0] = '\0';
                state->search_active = false;
            }
        }
        DrawRectangleRec(item2, item2_color);
        DrawText("Nouveau fichier", (int)state->menu_x + 10, (int)(state->menu_y + menu_item_height + 7), 14, state->colors.text_primary);
        
        // Close menu on escape or click outside
        if (IsKeyPressed(KEY_ESCAPE)) {
            state->menu_active = false;
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Rectangle menu_rect = {(float)state->menu_x, (float)state->menu_y, 180, (float)(menu_item_height * 2)};
            if (!CheckCollisionPointRec(GetMousePosition(), menu_rect)) {
                state->menu_active = false;
            }
        }
    }
    
    // Champ de saisie pour création (modal)
    if (state->create_active) {
        // Semi-transparent overlay
        DrawRectangle(0, 0, state->window_width, state->window_height, Fade(BLACK, 0.3f));
        
        // Modal box
        int modal_width = 400;
        int modal_height = 140;
        int modal_x = (state->window_width - modal_width) / 2;
        int modal_y = (state->window_height - modal_height) / 2;
        
        DrawRectangle(modal_x, modal_y, modal_width, modal_height, state->colors.bg_primary);
        Rectangle modal_rect = {(float)modal_x, (float)modal_y, (float)modal_width, (float)modal_height};
        DrawRectangleLinesEx(modal_rect, 3, state->colors.accent);
        
        // Title
        const char* title = (state->create_type == CREATE_DIRECTORY) ? "Créer un dossier" : "Créer un fichier";
        int title_width = MeasureText(title, 18);
        DrawText(title, modal_x + (modal_width - title_width) / 2, modal_y + 15, 18, state->colors.text_primary);
        
        // Input field
        int input_y = modal_y + 50;
        Rectangle input_box = {(float)(modal_x + 15), (float)input_y, (float)(modal_width - 30), 35};
        DrawRectangleRec(input_box, state->colors.bg_secondary);
        DrawRectangleLinesEx(input_box, 2, state->colors.accent);
        
        const char* placeholder = (state->create_type == CREATE_DIRECTORY) ? "Nom du dossier..." : "Nom du fichier...";
        const char* text = (state->create_name[0] != '\0') ? state->create_name : placeholder;
        Color text_color = (state->create_name[0] != '\0') ? state->colors.text_primary : state->colors.text_disabled;
        DrawText(text, (int)input_box.x + 10, (int)input_box.y + 9, 16, text_color);
        
        // Blinking cursor
        if (state->create_name[0] != '\0' && ((int)(GetTime() * 2) % 2 == 0)) {
            int tw = MeasureText(state->create_name, 16);
            DrawText("|", (int)input_box.x + 10 + tw, (int)input_box.y + 9, 16, state->colors.text_primary);
        }
        
        // Buttons
        int btn_width = 80;
        int btn_y = modal_y + 100;
        int btn_spacing = 20;
        int total_width = btn_width * 2 + btn_spacing;
        int start_x = modal_x + (modal_width - total_width) / 2;
        
        // OK button
        Rectangle ok_btn = {(float)start_x, (float)btn_y, (float)btn_width, 25};
        Color ok_color = Fade(state->colors.accent, 0.8f);
        if (CheckCollisionPointRec(GetMousePosition(), ok_btn)) {
            ok_color = state->colors.accent;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (state->create_name[0] != '\0') {
                    state->create_confirmed = true;
                }
            }
        }
        DrawRectangleRec(ok_btn, ok_color);
        DrawText("OK", (int)ok_btn.x + 30, (int)ok_btn.y + 5, 14, state->colors.bg_primary);
        
        // Cancel button
        Rectangle cancel_btn = {(float)(start_x + btn_width + btn_spacing), (float)btn_y, (float)btn_width, 25};
        Color cancel_color = Fade(state->colors.text_secondary, 0.8f);
        if (CheckCollisionPointRec(GetMousePosition(), cancel_btn)) {
            cancel_color = state->colors.text_secondary;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state->create_active = false;
                state->create_confirmed = false;
                state->create_type = CREATE_NONE;
                state->create_name[0] = '\0';
            }
        }
        DrawRectangleRec(cancel_btn, cancel_color);
        DrawText("Cancel", (int)cancel_btn.x + 18, (int)cancel_btn.y + 5, 14, state->colors.bg_primary);
        
        // Handle ESC to close modal
        if (IsKeyPressed(KEY_ESCAPE)) {
            state->create_active = false;
            state->create_confirmed = false;
            state->create_type = CREATE_NONE;
            state->create_name[0] = '\0';
        }
    }
    
    // Zone de défilement
    int content_y = state->is_searching ? 200 : 170;  // Plus d'espace si recherche en cours
    int y = content_y - state->scroll_offset;
    
    // Calculer la largeur de la zone des fichiers (split view si fichier sélectionné)
    int file_list_width = state->selected_file_path ? state->window_width / 2 - 5 : state->window_width;
    
    // En-tête de colonnes
    DrawRectangle(0, content_y, file_list_width, LINE_HEIGHT, state->colors.bg_secondary);
    DrawLine(0, content_y + LINE_HEIGHT, file_list_width, content_y + LINE_HEIGHT, state->colors.border);
    
    int col_name_width = 300;
    int col_size_width = 100;
    int col_date_width = 150;
    
    DrawText("Nom", PADDING + 30, content_y + 5, 14, state->colors.text_secondary);
    DrawText("Taille", PADDING + col_name_width, content_y + 5, 14, state->colors.text_secondary);
    DrawText("Modifié", PADDING + col_name_width + col_size_width, content_y + 5, 14, state->colors.text_secondary);
    
    int header_y = content_y + LINE_HEIGHT;
    y = header_y - state->scroll_offset;
    
    // Dessiner les fichiers
    for (int i = 0; i < files->count; i++) {
        FileEntry* entry = &files->entries[i];
        
        // Ne dessiner que les éléments visibles
        if (y >= header_y - LINE_HEIGHT && y < state->window_height) {
            int x = PADDING + (entry->depth * INDENT_SIZE);
            
            // Fond de sélection
            Color bg_color = BLANK;
            Rectangle item_rect = { 0, (float)y, (float)file_list_width, LINE_HEIGHT };
            
            if (CheckCollisionPointRec(GetMousePosition(), item_rect)) {
                bg_color = state->colors.highlight;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state->selected_index = i;
                    if (entry->type == FILE_TYPE_DIRECTORY) {
                        // Navigation dans un dossier
                        state->clicked_path = (char*)malloc(strlen(entry->path) + 1);
                        if (state->clicked_path) {
                            strcpy(state->clicked_path, entry->path);
                        }
                    } else {
                        // Charger le contenu du fichier
                        load_file_content(state, entry->path);
                    }
                }
            }
            
            if (i == state->selected_index) {
                bg_color = Fade(SKYBLUE, 0.4f);
            }
            
            DrawRectangleRec(item_rect, bg_color);
            
            if (i == state->selected_index) {
                bg_color = state->colors.highlight_hover;
                DrawRectangleRec(item_rect, Fade(bg_color, 0.3f));
            }
            
            // Opacité adaptée pour fichiers cachés
            float alpha = get_entry_opacity(entry->name);
            
            // Icône dessinée
            Color icon_color = (entry->type == FILE_TYPE_DIRECTORY) ? state->colors.accent : state->colors.text_secondary;
            icon_color = Fade(icon_color, alpha);
            
            if (entry->type == FILE_TYPE_DIRECTORY) {
                // Dossier : rectangle avec onglet
                DrawRectangle(x + 2, y + 8, 18, 14, icon_color);
                DrawRectangle(x + 2, y + 5, 8, 3, icon_color);
            } else {
                // Fichier : rectangle avec coin plié
                DrawRectangle(x + 3, y + 5, 14, 17, Fade(icon_color, 0.5f));
                DrawRectangle(x + 3, y + 5, 14, 1, icon_color);
                DrawRectangle(x + 3, y + 5, 1, 17, icon_color);
                DrawRectangle(x + 17, y + 5, 1, 17, icon_color);
                DrawRectangle(x + 3, y + 22, 15, 1, icon_color);
                // Coin plié
                DrawTriangle(
                    (Vector2){x + 17, y + 5},
                    (Vector2){x + 12, y + 5},
                    (Vector2){x + 17, y + 10},
                    icon_color
                );
            }
            
            // Colonne 1: Nom - avec couleur adaptée pour fichiers cachés
            Color name_color = get_text_color_for_entry(state, entry->name, i == state->selected_index);
            name_color = Fade(name_color, alpha);
            DrawText(entry->name, x + 28, y + 3, FONT_SIZE - 2, name_color);
            
            // Colonne 2: Taille (seulement pour les fichiers)
            Color size_color = Fade(state->colors.text_secondary, alpha);
            if (entry->type == FILE_TYPE_FILE) {
                char size_str[64];
                format_size(entry->size, size_str, sizeof(size_str));
                DrawText(size_str, PADDING + col_name_width, y + 5, FONT_SIZE - 4, size_color);
            } else {
                DrawText("[dossier]", PADDING + col_name_width, y + 5, FONT_SIZE - 4, size_color);
            }
            
            // Colonne 3: Date de modification
            char date_str[32];
            format_time(entry->mod_time, date_str, sizeof(date_str));
            DrawText(date_str, PADDING + col_name_width + col_size_width, y + 5, FONT_SIZE - 4, size_color);
        }
        
        y += LINE_HEIGHT;
    }
    
    // Panneau de visualisation du fichier (split view à droite)
    if (state->selected_file_path) {
        int panel_x = state->window_width / 2 + 5;
        int panel_width = state->window_width / 2 - 5;
        int panel_y = content_y;
        int panel_height = state->window_height - content_y - 30;
        
        // Séparateur vertical
        DrawRectangle(state->window_width / 2, content_y, 2, state->window_height - content_y, state->colors.border);
        
        // Fond du panneau
        DrawRectangle(panel_x, panel_y, panel_width, panel_height, state->colors.bg_primary);
        DrawRectangleLines(panel_x, panel_y, panel_width, panel_height, state->colors.border);
        
        // En-tête du panneau avec bouton de fermeture
        DrawRectangle(panel_x, panel_y, panel_width, 30, state->colors.bg_secondary);
        
        // Extraire le nom du fichier
        const char* file_name = strrchr(state->selected_file_path, '/');
        file_name = file_name ? file_name + 1 : state->selected_file_path;
        
        // Nom du fichier (tronqué si nécessaire)
        char display_name[100];
        if (strlen(file_name) > 30) {
            snprintf(display_name, sizeof(display_name), "%.27s...", file_name);
        } else {
            strncpy(display_name, file_name, sizeof(display_name) - 1);
        }
        DrawText(display_name, panel_x + 10, panel_y + 8, 16, state->colors.text_primary);
        
        // Bouton fermer (X)
        Rectangle close_btn = {(float)(panel_x + panel_width - 30), (float)(panel_y + 5), 20, 20};
        Color close_color = Fade(state->colors.text_secondary, 0.7f);
        if (CheckCollisionPointRec(GetMousePosition(), close_btn)) {
            close_color = ORANGE;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (state->selected_file_path) {
                    free(state->selected_file_path);
                    state->selected_file_path = NULL;
                }
                if (state->file_content) {
                    free(state->file_content);
                    state->file_content = NULL;
                }
            }
        }
        DrawRectangleRec(close_btn, close_color);
        DrawText("X", panel_x + panel_width - 26, panel_y + 7, 16, state->colors.bg_primary);
        
        // Contenu du fichier
        int text_y = panel_y + 35;
        int text_area_height = panel_height - 35;
        
        if (state->is_binary_file) {
            // Afficher message pour fichier binaire
            DrawText("Fichier binaire", panel_x + 10, text_y + 10, 18, state->colors.text_secondary);
            char size_str[64];
            if (state->file_size < 1024) {
                snprintf(size_str, sizeof(size_str), "Taille: %ld octets", state->file_size);
            } else if (state->file_size < 1024 * 1024) {
                snprintf(size_str, sizeof(size_str), "Taille: %.1f KB", state->file_size / 1024.0);
            } else {
                snprintf(size_str, sizeof(size_str), "Taille: %.1f MB", state->file_size / (1024.0 * 1024.0));
            }
            DrawText(size_str, panel_x + 10, text_y + 35, 16, state->colors.text_disabled);
            DrawText("Impossible d'afficher le contenu", panel_x + 10, text_y + 60, 14, state->colors.text_disabled);
        } else if (state->file_content) {
            // Gérer le scroll avec la molette dans la zone du panneau
            Rectangle panel_area = {(float)panel_x, (float)text_y, (float)panel_width, (float)text_area_height};
            if (CheckCollisionPointRec(GetMousePosition(), panel_area)) {
                float wheel = GetMouseWheelMove();
                if (wheel != 0) {
                    state->file_scroll_offset -= (int)(wheel * 20);
                    if (state->file_scroll_offset < 0) {
                        state->file_scroll_offset = 0;
                    }
                }
            }
            
            // Afficher le contenu ligne par ligne
            BeginScissorMode(panel_x, text_y, panel_width, text_area_height);
            
            int line_y = text_y - state->file_scroll_offset;
            int line_height = 16;
            char* line_start = state->file_content;
            char* line_end;
            int line_num = 1;
            
            while (line_start && *line_start && line_y < text_y + text_area_height) {
                line_end = strchr(line_start, '\n');
                
                if (line_y + line_height >= text_y) {
                    // Extraire la ligne
                    char line_buffer[512];
                    int line_len;
                    if (line_end) {
                        line_len = line_end - line_start;
                        if (line_len > 510) line_len = 510;
                    } else {
                        line_len = strlen(line_start);
                        if (line_len > 510) line_len = 510;
                    }
                    
                    strncpy(line_buffer, line_start, line_len);
                    line_buffer[line_len] = '\0';
                    
                    // Numéro de ligne
                    char num_str[8];
                    snprintf(num_str, sizeof(num_str), "%4d", line_num);
                    DrawText(num_str, panel_x + 5, line_y, 14, state->colors.text_secondary);
                    
                    // Contenu de la ligne - avec highlight si recherche par contenu
                    if (state->search_by_content && state->search_text[0] != '\0') {
                        // Chercher et mettre en avant le texte trouvé
                        const char* search_pos = strstr(line_buffer, state->search_text);
                        if (search_pos) {
                            // Afficher le début avant la correspondance
                            int before_len = search_pos - line_buffer;
                            char before_buf[512];
                            strncpy(before_buf, line_buffer, before_len);
                            before_buf[before_len] = '\0';
                            DrawText(before_buf, panel_x + 45, line_y, 14, state->colors.text_primary);
                            
                            // Mettre en évidence la correspondance
                            int match_width = MeasureText(before_buf, 14);
                            Rectangle highlight_box = {
                                (float)(panel_x + 45 + match_width),
                                (float)(line_y - 1),
                                (float)MeasureText(state->search_text, 14) + 4,
                                14 + 2
                            };
                            DrawRectangleRec(highlight_box, Fade(state->colors.accent, 0.3f));
                            DrawText(state->search_text, panel_x + 45 + match_width + 2, line_y, 14, ORANGE);
                            
                            // Afficher la fin après la correspondance
                            const char* after_start = search_pos + strlen(state->search_text);
                            int after_width = match_width + MeasureText(state->search_text, 14) + 2;
                            DrawText(after_start, panel_x + 45 + after_width, line_y, 14, state->colors.text_primary);
                        } else {
                            DrawText(line_buffer, panel_x + 45, line_y, 14, state->colors.text_primary);
                        }
                    } else {
                        DrawText(line_buffer, panel_x + 45, line_y, 14, state->colors.text_primary);
                    }
                }
                
                line_y += line_height;
                line_num++;
                
                if (line_end) {
                    line_start = line_end + 1;
                } else {
                    break;
                }
            }
            
            EndScissorMode();
            
            // Info sur la taille du fichier si tronqué
            if (state->file_size > 1048576) {
                DrawText("(Affichage limite a 1 MB)", panel_x + 10, panel_y + panel_height - 20, 12, ORANGE);
            }
        }
    }
    
    // Instructions
    const char* instructions = state->search_active ? 
        "Tapez pour chercher | BACKSPACE pour effacer | ESC pour annuler" :
        "Ctrl+F: Rechercher | Cliquez sur un dossier pour y entrer | Molette pour defiler | ESC pour quitter";
    int text_width = MeasureText(instructions, 12);
    DrawText(instructions, state->window_width - text_width - PADDING, state->window_height - 25, 12, state->colors.text_secondary);
    
    EndDrawing();
}

char* ui_get_clicked_path(UIState* state) {
    if (!state) return NULL;
    
    char* path = state->clicked_path;
    state->clicked_path = NULL;
    return path;
}

bool ui_should_go_back(UIState* state) {
    return state ? state->go_back : false;
}

bool ui_is_searching(UIState* state) {
    return state ? state->is_searching : false;
}

const char* ui_get_search_text(UIState* state) {
    return state ? state->search_text : "";
}

void ui_set_searching(UIState* state, bool searching) {
    if (state) {
        state->is_searching = searching;
    }
}

void ui_set_search_limit_reached(UIState* state, bool reached) {
    if (state) {
        state->search_limit_reached = reached;
    }
}

void ui_set_search_stats(UIState* state, int files_scanned, int dirs_scanned, int files_matched, double elapsed_time) {
    if (state) {
        state->search_files_scanned = files_scanned;
        state->search_dirs_scanned = dirs_scanned;
        state->search_files_matched = files_matched;
        state->search_elapsed_time = elapsed_time;
    }
}

void ui_set_theme(UIState* state, Theme theme) {
    if (state) {
        state->current_theme = theme;
        state->colors = get_theme_colors(theme);
    }
}

Theme ui_get_theme(UIState* state) {
    return state ? state->current_theme : THEME_LIGHT;
}

void ui_toggle_theme(UIState* state) {
    if (state) {
        state->current_theme = (state->current_theme == THEME_LIGHT) ? THEME_DARK : THEME_LIGHT;
        state->colors = get_theme_colors(state->current_theme);
    }
}

bool ui_should_close(void) {
    return WindowShouldClose();
}

bool ui_get_show_hidden(UIState* state) {
    return state ? state->show_hidden : false;
}

bool ui_get_search_by_content(UIState* state) {
    return state ? state->search_by_content : false;
}

bool ui_creation_confirmed(UIState* state) {
    return state ? state->create_confirmed : false;
}

const char* ui_get_creation_name(UIState* state) {
    return state ? state->create_name : "";
}

CreateType ui_get_creation_type(UIState* state) {
    return state ? state->create_type : CREATE_NONE;
}

void ui_clear_creation_request(UIState* state) {
    if (!state) return;
    state->create_active = false;
    state->create_confirmed = false;
    state->create_type = CREATE_NONE;
    state->create_name[0] = '\0';
}
