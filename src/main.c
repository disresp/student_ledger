#include "student.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================
 *  ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ============================================================ */

StudentNode* g_head = NULL;
TextBoxState g_textBox = {0};
AuthParameters g_authParams = {0};
EditParameters g_editParams = {0};
SearchParameters g_searchParams = {0};
int g_currentPage = PAGE_TABLE;
ViewMode g_currentViewMode = VIEW_ALL;
int g_showOnlyExcellentBudget = 0;
int g_selectedRowIndex = -1;
float g_scrollOffset = 0.0f;
StudentNode** g_displayArray = NULL;
int g_displayCount = 0;
char g_statusMsg[128] = "";
double g_statusTime = 0.0;
Font g_font = {0};
int g_filterForm = 0;
StudentNode* g_undoStack[UNDO_DEPTH] = {NULL};
int g_undoTop = -1;

/* ============================================================
 *  ТОЧКА ВХОДА
 * ============================================================ */

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INIT_WIDTH, INIT_HEIGHT, "Студенческая ведомость");
    SetTargetFPS(60);

    {
        int codepoints[512];
        int cpCount = 0;
        for (int cp = 0x20; cp <= 0x7E; cp++) codepoints[cpCount++] = cp;
        for (int cp = 0x400; cp <= 0x4FF; cp++) codepoints[cpCount++] = cp;
        codepoints[cpCount++] = 0x401; codepoints[cpCount++] = 0x451;
        const char* fontPaths[] = {
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/times.ttf"
        };
        for (int i = 0; i < 3; i++) {
            g_font = LoadFontEx(fontPaths[i], 64, codepoints, cpCount);
            if (g_font.texture.id != 0) break;
        }
        if (g_font.texture.id == 0)
            g_font = GetFontDefault();
        else
            SetTextureFilter(g_font.texture, TEXTURE_FILTER_POINT);
    }

    auth_init();

    int dataLoaded = 0;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11))
            ToggleFullscreen();

        BeginDrawing();
        ClearBackground(BG_COLOR);

        if (!g_authParams.isLoggedIn)
        {
            dataLoaded = 0;
            drawLoginForm();
        }
        else
        {
            if (!dataLoaded) {
                char datFile[64];
                snprintf(datFile, sizeof(datFile), "%s_students.dat",
                         g_authParams.currentUsername);
                loadFromFile(&g_head, datFile);
                if (g_head == NULL) {
                    int g1[] = {8,9,7,8,10,9,8,7,9,8};
                    int g2[] = {6,7,6,8,7,9,6,7,8,7};
                    int g3[] = {9,10,9,8,9,10,9,9,10,9};
                    int g4[] = {7,6,8,7,9,8,7,6,8,7};
                    int g5[] = {5,6,5,7,6,8,5,6,7,6};
                    int g6[] = {8,8,9,10,9,8,8,9,10,9};
                    int g7[] = {7,7,8,7,8,7,7,8,7,8};
                    int g8[] = {9,9,10,9,9,8,9,10,9,9};
                    int g9[] = {6,5,7,6,8,7,6,5,7,6};
                    int gA[] = {10,10,9,10,9,10,10,9,10,9};
                    pushBack(&g_head, createStudent("Иванов Иван Иванович","551001","Программная инженерия","Бюджет",g1,10));
                    pushBack(&g_head, createStudent("Петрова Анна Сергеевна","551001","Программная инженерия","Бюджет",g2,10));
                    pushBack(&g_head, createStudent("Сидоров Алексей Павлович","520602","Прикладная математика","Бюджет",g3,10));
                    pushBack(&g_head, createStudent("Козлова Мария Дмитриевна","520602","Прикладная математика","Бюджет",g4,10));
                    pushBack(&g_head, createStudent("Новиков Денис Андреевич","551002","Компьютерные сети","Платное",g5,10));
                    pushBack(&g_head, createStudent("Белова Екатерина Викторовна","551002","Компьютерные сети","Платное",g6,10));
                    pushBack(&g_head, createStudent("Соколов Артём Михайлович","551001","Программная инженерия","Платное",g9,10));
                    pushBack(&g_head, createStudent("Зайцева Татьяна Алексеевна","551001","Программная инженерия","Платное",gA,10));
                }
                rebuildDisplay();
                dataLoaded = 1;
            }

            if (g_currentPage == PAGE_TABLE)
            {
                float mw = GetMouseWheelMove();
                if (mw != 0) {
                    int totalRows = g_displayCount;
                    if (g_currentViewMode == VIEW_BY_FORM)
                        totalRows += 2;
                    float maxScroll = (float)(totalRows * ROW_H - TABLE_H);
                    if (maxScroll < 0) maxScroll = 0;
                    g_scrollOffset -= mw * ROW_H * 2.0f;
                    if (g_scrollOffset < 0) g_scrollOffset = 0;
                    if (g_scrollOffset > maxScroll) g_scrollOffset = maxScroll;
                }

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    int mx = GetMouseX();
                    int my = GetMouseY();
                    if (mx > MAIN_CONTENT_OFFSET_X && my > ROW_Y_START) {
                        int row = getRowAtClick(my);
                        if (row >= 0 && row < g_displayCount) {
                            g_selectedRowIndex = row;
                            if (!g_editParams.isEditMode)
                                fillInputFromStudent(&g_displayArray[row]->student);
                        }
                    }
                }
            }

            drawSidebar();

            if (g_currentPage == PAGE_FILE)
                drawFilePage();
            else
                drawMainWindow();

            if (g_currentPage == PAGE_TABLE)
                handleKeys();
        }

        EndDrawing();
    }

    if (g_authParams.isLoggedIn && g_head) {
        char datFile[64];
        snprintf(datFile, sizeof(datFile), "%s_students.dat",
                 g_authParams.currentUsername);
        saveToFile(g_head, datFile);
    }

    clearList(&g_head);
    for (int i = 0; i < UNDO_DEPTH; i++)
        clearList(&g_undoStack[i]);
    if (g_displayArray) free(g_displayArray);
    UnloadFont(g_font);
    CloseWindow();
    return 0;
}
