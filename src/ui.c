#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define FONT_SIZE 20
#define LINE_HEIGHT 25
#define PADDING 10
#define INDENT_SIZE 20

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
    state->initialized = false;
    
    InitWindow(width, height, title);
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
    
    // Réinitialiser le chemin cliqué et go_back
    if (state->clicked_path) {
        free(state->clicked_path);
        state->clicked_path = NULL;
    }
    state->go_back = false;
    
    // Gestion de l'input clavier pour la recherche
    if (state->search_active) {
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
    } else {
        // Activer la recherche avec Ctrl+F ou Cmd+F
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER)) && IsKeyPressed(KEY_F)) {
            state->search_active = true;
        }
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
    ClearBackground(RAYWHITE);
    
    // En-tête avec bouton retour
    DrawRectangle(0, 0, state->window_width, 40, DARKGRAY);
    DrawText("Explorateur de Fichiers", PADDING + 100, 10, FONT_SIZE, WHITE);
    
    // Bouton retour
    Rectangle back_button = {PADDING, 8, 80, 24};
    Color back_color = LIGHTGRAY;
    if (CheckCollisionPointRec(GetMousePosition(), back_button)) {
        back_color = GRAY;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state->go_back = true;
        }
    }
    DrawRectangleRec(back_button, back_color);
    DrawText("< Retour", PADDING + 5, 12, 16, DARKGRAY);
    
    // Chemin actuel
    DrawRectangle(0, 40, state->window_width, 35, LIGHTGRAY);
    char path_display[512];
    snprintf(path_display, sizeof(path_display), "Chemin: %s", current_path);
    DrawText(path_display, PADDING, 48, 18, DARKGRAY);
    
    // Statistiques
    DrawRectangle(0, 75, state->window_width, 25, GRAY);
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
    DrawText(stats, PADDING, 80, 16, WHITE);
    
    // Barre de recherche
    int search_y = 100;
    int search_height = 35;
    Rectangle search_box = {PADDING, (float)search_y + 5, (float)state->window_width - 2 * PADDING, (float)search_height - 10};
    
    Color search_bg = state->search_active ? WHITE : Fade(WHITE, 0.7f);
    DrawRectangleRec(search_box, search_bg);
    DrawRectangleLinesEx(search_box, 2, state->search_active ? SKYBLUE : GRAY);
    
    // Icône de recherche
    DrawText("🔍", PADDING + 8, search_y + 8, 16, DARKGRAY);
    
    // Texte de recherche
    if (state->search_text[0] != '\0') {
        DrawText(state->search_text, PADDING + 35, search_y + 10, 16, BLACK);
        
        // Curseur clignotant
        if (state->search_active && ((int)(GetTime() * 2) % 2 == 0)) {
            int text_width = MeasureText(state->search_text, 16);
            DrawText("|", PADDING + 35 + text_width, search_y + 10, 16, BLACK);
        }
    } else if (state->search_active) {
        // Curseur seul
        if ((int)(GetTime() * 2) % 2 == 0) {
            DrawText("|", PADDING + 35, search_y + 10, 16, BLACK);
        }
    } else {
        // Placeholder
        DrawText("Rechercher... (Ctrl+F)", PADDING + 35, search_y + 10, 16, GRAY);
    }
    
    // Compteur de résultats
    if (state->search_text[0] != '\0') {
        int match_count = 0;
        for (int i = 0; i < files->count; i++) {
            if (matches_search(files->entries[i].name, state->search_text)) {
                match_count++;
            }
        }
        char count_text[64];
        snprintf(count_text, sizeof(count_text), "%d résultat%s", match_count, match_count > 1 ? "s" : "");
        int count_width = MeasureText(count_text, 14);
        DrawText(count_text, state->window_width - count_width - PADDING - 10, search_y + 12, 14, DARKGRAY);
    }
    
    // Détection du clic sur la barre de recherche
    if (CheckCollisionPointRec(GetMousePosition(), search_box) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->search_active = true;
    }
    
    // Zone de défilement
    int content_y = 140;
    int y = content_y - state->scroll_offset;
    
    // Dessiner les fichiers
    for (int i = 0; i < files->count; i++) {
        FileEntry* entry = &files->entries[i];
        
        // Filtrer selon la recherche
        if (!matches_search(entry->name, state->search_text)) {
            continue;
        }
        
        // Ne dessiner que les éléments visibles
        if (y >= content_y - LINE_HEIGHT && y < state->window_height) {
            int x = PADDING + (entry->depth * INDENT_SIZE);
            
            // Fond de sélection
            Color bg_color = BLANK;
            Rectangle item_rect = { 0, (float)y, (float)state->window_width, LINE_HEIGHT };
            
            if (CheckCollisionPointRec(GetMousePosition(), item_rect)) {
                bg_color = Fade(SKYBLUE, 0.2f);
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state->selected_index = i;
                    // Si c'est un dossier, on navigue dedans
                    if (entry->type == FILE_TYPE_DIRECTORY) {
                        state->clicked_path = (char*)malloc(strlen(entry->path) + 1);
                        if (state->clicked_path) {
                            strcpy(state->clicked_path, entry->path);
                        }
                    }
                }
            }
            
            if (i == state->selected_index) {
                bg_color = Fade(SKYBLUE, 0.4f);
            }
            
            DrawRectangleRec(item_rect, bg_color);
            
            // Icône dessinée
            if (entry->type == FILE_TYPE_DIRECTORY) {
                // Dossier : rectangle avec onglet
                DrawRectangle(x + 2, y + 8, 18, 14, BLUE);
                DrawRectangle(x + 2, y + 5, 8, 3, BLUE);
            } else {
                // Fichier : rectangle avec coin plié
                DrawRectangle(x + 3, y + 5, 14, 17, LIGHTGRAY);
                DrawRectangle(x + 3, y + 5, 14, 1, DARKGRAY);
                DrawRectangle(x + 3, y + 5, 1, 17, DARKGRAY);
                DrawRectangle(x + 17, y + 5, 1, 17, DARKGRAY);
                DrawRectangle(x + 3, y + 22, 15, 1, DARKGRAY);
                // Coin plié
                DrawTriangle(
                    (Vector2){x + 17, y + 5},
                    (Vector2){x + 12, y + 5},
                    (Vector2){x + 17, y + 10},
                    GRAY
                );
            }
            
            // Nom
            DrawText(entry->name, x + 28, y + 3, FONT_SIZE - 2, BLACK);
            
            // Taille (seulement pour les fichiers)
            if (entry->type == FILE_TYPE_FILE) {
                char size_str[64];
                format_size(entry->size, size_str, sizeof(size_str));
                int size_width = MeasureText(size_str, FONT_SIZE - 4);
                DrawText(size_str, state->window_width - size_width - PADDING, y + 5, FONT_SIZE - 4, DARKGRAY);
            } else {
                // Flèche pour les dossiers
                DrawText(">", state->window_width - 30, y + 3, FONT_SIZE, DARKGRAY);
            }
        }
        
        y += LINE_HEIGHT;
    }
    
    // Instructions
    const char* instructions = state->search_active ? 
        "Tapez pour chercher | BACKSPACE pour effacer | ESC pour annuler" :
        "Ctrl+F: Rechercher | Cliquez sur un dossier pour y entrer | Molette pour defiler | ESC pour quitter";
    int text_width = MeasureText(instructions, 12);
    DrawText(instructions, state->window_width - text_width - PADDING, state->window_height - 25, 12, DARKGRAY);
    
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

bool ui_should_close(void) {
    return WindowShouldClose();
}
