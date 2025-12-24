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
    state->is_searching = false;
    state->search_limit_reached = false;
    state->selected_file_path = NULL;
    state->file_content = NULL;
    state->is_binary_file = false;
    state->file_size = 0;
    state->file_scroll_offset = 0;
    state->initialized = false;
    
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
    
    // Icône de recherche (loupe dessinée)
    int icon_x = PADDING + 8;
    int icon_y = search_y + 10;
    DrawCircle(icon_x + 5, icon_y + 5, 5, BLANK);
    DrawCircleLines(icon_x + 5, icon_y + 5, 5, DARKGRAY);
    DrawLineEx((Vector2){icon_x + 9, icon_y + 9}, (Vector2){icon_x + 13, icon_y + 13}, 2, DARKGRAY);
    
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
        char count_text[64];
        if (state->search_limit_reached) {
            snprintf(count_text, sizeof(count_text), "%d+ resultats (limite)", files->count);
        } else {
            snprintf(count_text, sizeof(count_text), "%d resultat%s", files->count, files->count > 1 ? "s" : "");
        }
        int count_width = MeasureText(count_text, 14);
        DrawText(count_text, state->window_width - count_width - PADDING - 10, search_y + 12, 14, 
                 state->search_limit_reached ? ORANGE : DARKGRAY);
    }
    
    // Détection du clic sur la barre de recherche
    if (CheckCollisionPointRec(GetMousePosition(), search_box) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->search_active = true;
    }
    
    // Zone de défilement
    int content_y = 140;
    int y = content_y - state->scroll_offset;
    
    // Calculer la largeur de la zone des fichiers (split view si fichier sélectionné)
    int file_list_width = state->selected_file_path ? state->window_width / 2 - 5 : state->window_width;
    
    // Dessiner les fichiers
    for (int i = 0; i < files->count; i++) {
        FileEntry* entry = &files->entries[i];
        
        // Ne dessiner que les éléments visibles
        if (y >= content_y - LINE_HEIGHT && y < state->window_height) {
            int x = PADDING + (entry->depth * INDENT_SIZE);
            
            // Fond de sélection
            Color bg_color = BLANK;
            Rectangle item_rect = { 0, (float)y, (float)file_list_width, LINE_HEIGHT };
            
            if (CheckCollisionPointRec(GetMousePosition(), item_rect)) {
                bg_color = Fade(SKYBLUE, 0.2f);
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
    
    // Panneau de visualisation du fichier (split view à droite)
    if (state->selected_file_path) {
        int panel_x = state->window_width / 2 + 5;
        int panel_width = state->window_width / 2 - 5;
        int panel_y = content_y;
        int panel_height = state->window_height - content_y - 30;
        
        // Séparateur vertical
        DrawRectangle(state->window_width / 2, content_y, 2, state->window_height - content_y, GRAY);
        
        // Fond du panneau
        DrawRectangle(panel_x, panel_y, panel_width, panel_height, WHITE);
        DrawRectangleLines(panel_x, panel_y, panel_width, panel_height, LIGHTGRAY);
        
        // En-tête du panneau avec bouton de fermeture
        DrawRectangle(panel_x, panel_y, panel_width, 30, LIGHTGRAY);
        
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
        DrawText(display_name, panel_x + 10, panel_y + 8, 16, DARKGRAY);
        
        // Bouton fermer (X)
        Rectangle close_btn = {(float)(panel_x + panel_width - 30), (float)(panel_y + 5), 20, 20};
        Color close_color = GRAY;
        if (CheckCollisionPointRec(GetMousePosition(), close_btn)) {
            close_color = RED;
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
        DrawText("X", panel_x + panel_width - 26, panel_y + 7, 16, WHITE);
        
        // Contenu du fichier
        int text_y = panel_y + 35;
        int text_area_height = panel_height - 35;
        
        if (state->is_binary_file) {
            // Afficher message pour fichier binaire
            DrawText("Fichier binaire", panel_x + 10, text_y + 10, 18, DARKGRAY);
            char size_str[64];
            if (state->file_size < 1024) {
                snprintf(size_str, sizeof(size_str), "Taille: %ld octets", state->file_size);
            } else if (state->file_size < 1024 * 1024) {
                snprintf(size_str, sizeof(size_str), "Taille: %.1f KB", state->file_size / 1024.0);
            } else {
                snprintf(size_str, sizeof(size_str), "Taille: %.1f MB", state->file_size / (1024.0 * 1024.0));
            }
            DrawText(size_str, panel_x + 10, text_y + 35, 16, GRAY);
            DrawText("Impossible d'afficher le contenu", panel_x + 10, text_y + 60, 14, GRAY);
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
                    DrawText(num_str, panel_x + 5, line_y, 14, GRAY);
                    
                    // Contenu de la ligne
                    DrawText(line_buffer, panel_x + 45, line_y, 14, BLACK);
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

bool ui_should_close(void) {
    return WindowShouldClose();
}
