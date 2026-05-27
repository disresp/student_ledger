#include "student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

/* Позиции колонок таблицы */
const int COL_X[7] = { 10+MAIN_CONTENT_OFFSET_X, 45+MAIN_CONTENT_OFFSET_X,
                       115+MAIN_CONTENT_OFFSET_X, 410+MAIN_CONTENT_OFFSET_X,
                       690+MAIN_CONTENT_OFFSET_X, 840+MAIN_CONTENT_OFFSET_X,
                       970+MAIN_CONTENT_OFFSET_X };
const int COL_W[7] = { 35, 70, 295, 280, 150, 130, 230 };

/* ============================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ UTF-8
 * ============================================================ */

static void utf8_append(char* buf, int* len, int max, int codepoint) {
    if (codepoint < 0x80) {
        if (*len + 1 > max) return;
        buf[(*len)++] = (char)codepoint;
    } else if (codepoint < 0x800) {
        if (*len + 2 > max) return;
        buf[(*len)++] = (char)(0xC0 | (codepoint >> 6));
        buf[(*len)++] = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        if (*len + 3 > max) return;
        buf[(*len)++] = (char)(0xE0 | (codepoint >> 12));
        buf[(*len)++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buf[(*len)++] = (char)(0x80 | (codepoint & 0x3F));
    }
    buf[*len] = '\0';
}

static void utf8_pop(char* buf, int* len) {
    if (*len <= 0) return;
    int i = *len - 1;
    while (i > 0 && ((unsigned char)buf[i] & 0xC0) == 0x80) i--;
    *len = i;
    buf[*len] = '\0';
}

/* ============================================================
 *  ОТРИСОВКА ЭЛЕМЕНТОВ
 * ============================================================ */

int drawButton(int x, int y, int w, int h, const char* text, Color normal, Color hover) {
    Rectangle r = { (float)x, (float)y, (float)w, (float)h };
    DrawRectangleRounded(r, 0.15f, 6,
        CheckCollisionPointRec(GetMousePosition(), r) ? hover : normal);
    DrawRectangleRoundedLines(r, 0.15f, 6, (Color){ 0, 0, 0, 30 });
    float tw = MeasureTextEx(g_font, text, FONT_SIZE_INTERFACE, 1).x;
    DrawTextEx(g_font, text,
               (Vector2){ r.x + (w - tw) / 2.0f,
                          r.y + (h - FONT_SIZE_INTERFACE) / 2.0f },
               FONT_SIZE_INTERFACE, 1, BTN_TEXT);
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

int drawTextBox(int x, int y, int w, int h, const char* label,
                const char* buffer, int focused) {
    int labelW = (int)MeasureTextEx(g_font, label, FONT_SIZE_INTERFACE, 1).x + DEFAULT_PADDING - 4;
    DrawTextEx(g_font, label,
               (Vector2){ (float)x, y + (h - FONT_SIZE_INTERFACE) / 2.0f },
               FONT_SIZE_INTERFACE, 1, LABEL_COLOR);

    Rectangle inp = { (float)(x + labelW), (float)y,
                      (float)(w - labelW), (float)h };
    DrawRectangleRec(inp, INPUT_BG);
    DrawRectangleLinesEx(inp, focused ? 2 : 1,
                         focused ? FOCUS_BORDER : INPUT_BORDER);

    const char* visible = buffer;
    char trunc[128];
    if (MeasureTextEx(g_font, buffer, FONT_SIZE_INTERFACE, 1).x > (int)inp.width - DEFAULT_PADDING) {
        int start = (int)strlen(buffer);
        while (start > 0 && MeasureTextEx(g_font, buffer + start, FONT_SIZE_INTERFACE, 1).x >= (int)inp.width - DEFAULT_PADDING) {
            start--;
            while (start > 0 && ((unsigned char)buffer[start] & 0xC0) == 0x80) start--;
        }
        strcpy(trunc, buffer + start);
        visible = trunc;
    }

    DrawTextEx(g_font, visible,
               (Vector2){ inp.x + 4, y + (h - FONT_SIZE_INTERFACE) / 2.0f },
               FONT_SIZE_INTERFACE, 1, TEXT_COLOR);

    if (focused && ((int)(GetTime() * 2) % 2 == 0)) {
        float cx = inp.x + 4 + MeasureTextEx(g_font, visible, FONT_SIZE_INTERFACE, 1).x;
        DrawLineV((Vector2){ cx, inp.y + 4 },
                  (Vector2){ cx, inp.y + h - 4 },
                  (Color){ 30, 60, 180, 200 });
    }

    return CheckCollisionPointRec(GetMousePosition(), inp) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

int drawRadioGroup(int x, int y, int h, const char* labels[], int count, int selected) {
    for (int i = 0, cx = x; i < count; i++) {
        int ow = (int)MeasureTextEx(g_font, labels[i], FONT_SIZE_INTERFACE, 1).x + 22;
        Rectangle r = { (float)cx, (float)y, (float)ow, (float)h };
        DrawCircle(cx + 10, y + h / 2, 7,
                   i == selected ? RADIO_ACT : RADIO_INACT);
        if (i == selected)
            DrawCircle(cx + 10, y + h / 2, 3, (Color){ 255, 255, 255, 255 });
        DrawTextEx(g_font, labels[i],
                   (Vector2){ (float)(cx + 20), y + (h - FONT_SIZE_INTERFACE) / 2.0f },
                   FONT_SIZE_INTERFACE, 1, LABEL_COLOR);
        if (CheckCollisionPointRec(GetMousePosition(), r) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            selected = i;
        cx += ow + 4;
    }
    return selected;
}

/* ============================================================
 *  ТАБЛИЦА
 * ============================================================ */

void drawColText(int col, int y, int font_sz, const char* text, Color color) {
    Vector2 sz = MeasureTextEx(g_font, text, font_sz, 1);
    int max_w = COL_W[col] - 6;
    if (sz.x <= max_w) {
        DrawTextEx(g_font, text, (Vector2){ (float)COL_X[col], y },
                   font_sz, 1, color);
        return;
    }
    char buf[128];
    int byte_len = (int)strlen(text);
    while (byte_len > 0) {
        int cut = byte_len - 1;
        while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) cut--;
        memcpy(buf, text, cut);
        buf[cut] = '\0';
        strcat(buf, "...");
        Vector2 ts = MeasureTextEx(g_font, buf, font_sz, 1);
        if (ts.x <= max_w) {
            DrawTextEx(g_font, buf, (Vector2){ (float)COL_X[col], y },
                       font_sz, 1, color);
            return;
        }
        byte_len = cut;
    }
}

void drawHeader(void) {
    DrawRectangle(MAIN_CONTENT_OFFSET_X, HEADER_Y, SCRW - MAIN_CONTENT_OFFSET_X, ROW_H, HEADER_BG);
    const char* titles[] = { "#", "Группа", "ФИО", "Специальность",
                             "Форма", "Оценки", "Ср. балл" };
    for (int i = 0; i < 7; i++)
        DrawTextEx(g_font, titles[i],
                   (Vector2){ (float)COL_X[i], HEADER_Y + (ROW_H - 18) / 2 },
                   18, 1, HEADER_TEXT);
}

void drawRow(int idx, int y, const Student* s, int selected) {
    DrawRectangle(MAIN_CONTENT_OFFSET_X, y, SCRW - MAIN_CONTENT_OFFSET_X, ROW_H,
                  selected ? ROW_SELECTED : ((idx % 2 == 0) ? ROW_EVEN : ROW_ODD));

    char b[7][128];
    snprintf(b[0], sizeof(b[0]), "%d", idx + 1);

    if (g_editParams.isEditMode && selected) {
        snprintf(b[1], sizeof(b[1]), "%s", g_textBox.buffer2);
        strcpy(b[2], g_textBox.buffer3);
        strcpy(b[3], g_textBox.buffer1);
        strcpy(b[4], g_textBox.buffer4);

        char gradesText[64] = "";
        for (int i = 0; i < MAX_GRADES; i++) {
            char* gbuf = getGradeBuffer(i);
            if (gbuf && gbuf[0]) {
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "%s%d", i > 0 ? " " : "", atoi(gbuf));
                strcat(gradesText, tmp);
            }
        }
        snprintf(b[5], sizeof(b[5]), "%s", gradesText);

        float avg = 0.0f;
        int validCount = 0;
        for (int i = 0; i < MAX_GRADES; i++) {
            char* gbuf = getGradeBuffer(i);
            if (gbuf && gbuf[0]) {
                avg += (float)atoi(gbuf);
                validCount++;
            }
        }
        if (validCount > 0) avg /= validCount;
        else avg = calculateAverageGrade(*s);
        snprintf(b[6], sizeof(b[6]), "%.2f", avg);
    } else {
        snprintf(b[1], sizeof(b[1]), "%s", s->groupNumber);
        strcpy(b[2], s->fullName);
        strcpy(b[3], s->specialty);
        strcpy(b[4], s->educationForm);

        char gradesText[64] = "";
        for (int i = 0; i < s->gradeCount; i++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%s%d", i > 0 ? " " : "", s->grades[i]);
            strcat(gradesText, tmp);
        }
        snprintf(b[5], sizeof(b[5]), "%s", gradesText);
        snprintf(b[6], sizeof(b[6]), "%.2f", calculateAverageGrade(*s));
    }

    Color cc[] = {
        TEXT_COLOR, TEXT_COLOR, TEXT_COLOR, TEXT_COLOR,
        (strcmp(s->educationForm, "Бюджет") == 0) ? TITLE_BUDGET : TITLE_PAID,
        TEXT_COLOR, TEXT_COLOR
    };
    for (int i = 0; i < 7; i++)
        drawColText(i, y + (ROW_H - 16) / 2, 16, b[i], cc[i]);
}

void drawSection(int y, const char* title, Color bg) {
    DrawRectangle(MAIN_CONTENT_OFFSET_X, y, SCRW - MAIN_CONTENT_OFFSET_X, ROW_H, bg);
    Vector2 sz = MeasureTextEx(g_font, title, 17, 1);
    DrawTextEx(g_font, title,
               (Vector2){ (SCRW - sz.x) / 2, y + (ROW_H - 17) / 2 },
               17, 1, (Color){ 255, 255, 255, 255 });
}

void drawTable(void) {
    drawHeader();
    BeginScissorMode(MAIN_CONTENT_OFFSET_X, ROW_Y_START, SCRW - MAIN_CONTENT_OFFSET_X, TABLE_H);
    int y = ROW_Y_START - (int)g_scrollOffset;
    int ri = 0;

    if (g_currentViewMode == VIEW_BY_FORM) {
        int split = g_displayCount;
        for (int i = 0; i < g_displayCount; i++)
            if (strcmp(g_displayArray[i]->student.educationForm, "Бюджет") != 0)
                { split = i; break; }

        if (split > 0) {
            drawSection(y, "--- БЮДЖЕТНАЯ ФОРМА ---", TITLE_BUDGET);
            y += ROW_H; ri++;
        }
        for (int i = 0; i < split; i++) {
            if (g_showOnlyExcellentBudget && !isExcellentBudget(g_displayArray[i]->student)) continue;
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                drawRow(ri, y, &g_displayArray[i]->student, i == g_selectedRowIndex);
            y += ROW_H; ri++;
        }
        if (split < g_displayCount) {
            drawSection(y, "--- ПЛАТНАЯ ФОРМА ---", TITLE_PAID);
            y += ROW_H; ri++;
        }
        for (int i = split; i < g_displayCount; i++) {
            if (g_showOnlyExcellentBudget && !isExcellentBudget(g_displayArray[i]->student)) continue;
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                drawRow(ri, y, &g_displayArray[i]->student, i == g_selectedRowIndex);
            y += ROW_H; ri++;
        }
    } else {
        for (int i = 0; i < g_displayCount; i++) {
            if (g_showOnlyExcellentBudget && !isExcellentBudget(g_displayArray[i]->student)) continue;
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                drawRow(ri, y, &g_displayArray[i]->student, i == g_selectedRowIndex);
            y += ROW_H; ri++;
        }
    }
    EndScissorMode();
}

/* ============================================================
 *  ОБРАБОТКА ВВОДА
 * ============================================================ */

static char* getBufferForField(int fieldIndex) {
    switch (fieldIndex) {
        case 0: return g_textBox.buffer1;   /* specialty */
        case 1: return g_textBox.buffer2;   /* group */
        case 2: return g_textBox.buffer3;   /* fullName */
        case 3: return g_textBox.buffer4;   /* educationForm */
        case 4:  case 5:  case 6:  case 7:
        case 8:  case 9:  case 10: case 11:
        case 12: case 13: return getGradeBuffer(fieldIndex - 4);  /* grades */
        default: return NULL;
    }
}

static int* getLenForField(int fieldIndex) {
    /* Мы не храним отдельно длины, используем strlen */
    return NULL;
}

void handleKeys(void) {
    if (g_textBox.activeField < 0) return;

    int changed = 0;
    int c = GetCharPressed();
    while (c > 0) {
        int printable = (c >= 32 && c <= 126) ||
                        (c >= 0x400 && c <= 0x4FF) ||
                        (c >= 0x500 && c <= 0x52F);
        if (printable) {
            if (g_textBox.activeField == 0) {  /* specialty */
                utf8_append(g_textBox.buffer1, &(int){strlen(g_textBox.buffer1)}, 99, c);
                changed = 1;
            } else if (g_textBox.activeField == 1 && c >= '0' && c <= '9') {
                size_t plen = strlen(g_textBox.buffer2);
                if (plen < 18) { g_textBox.buffer2[plen] = (char)c; g_textBox.buffer2[plen+1] = '\0'; }
                changed = 1;
            } else if (g_textBox.activeField == 2) {  /* fullName */
                utf8_append(g_textBox.buffer3, &(int){strlen(g_textBox.buffer3)}, 99, c);
                changed = 1;
            } else if (g_textBox.activeField == 3) {  /* educationForm */
                utf8_append(g_textBox.buffer4, &(int){strlen(g_textBox.buffer4)}, 49, c);
                changed = 1;
            } else if (g_textBox.activeField >= 4 && g_textBox.activeField <= 13) {  /* grades */
                int gi = g_textBox.activeField - 4;
                char* buf = getGradeBuffer(gi);
                if (buf) {
                    size_t plen = strlen(buf);
                    if (c >= '0' && c <= '9' && plen < 2) {
                        buf[plen] = (char)c;
                        buf[plen+1] = '\0';
                        changed = 1;
                    }
                }
            }
        }
        c = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (g_textBox.activeField == 0) {
            int len = strlen(g_textBox.buffer1);
            utf8_pop(g_textBox.buffer1, &len);
            changed = 1;
        } else if (g_textBox.activeField == 1) {
            size_t plen = strlen(g_textBox.buffer2);
            if (plen > 0) g_textBox.buffer2[plen - 1] = '\0';
            changed = 1;
        } else if (g_textBox.activeField == 2) {
            int len = strlen(g_textBox.buffer3);
            utf8_pop(g_textBox.buffer3, &len);
            changed = 1;
        } else if (g_textBox.activeField == 3) {
            int len = strlen(g_textBox.buffer4);
            utf8_pop(g_textBox.buffer4, &len);
            changed = 1;
        } else if (g_textBox.activeField >= 4 && g_textBox.activeField <= 13) {
            int gi = g_textBox.activeField - 4;
            char* buf = getGradeBuffer(gi);
            if (buf) {
                size_t plen = strlen(buf);
                if (plen > 0) buf[plen - 1] = '\0';
                changed = 1;
            }
        }
    }

    if (changed && !g_editParams.isEditMode) {
        apply_filter();
    }
}

int getRowAtClick(int mouse_y) {
    if (mouse_y < ROW_Y_START || mouse_y >= ROW_Y_START + TABLE_H)
        return -1;
    int idx = (mouse_y - ROW_Y_START + (int)g_scrollOffset) / ROW_H;
    if (g_currentViewMode == VIEW_BY_FORM) {
        int split = g_displayCount;
        for (int i = 0; i < g_displayCount; i++)
            if (strcmp(g_displayArray[i]->student.educationForm, "Бюджет") != 0)
                { split = i; break; }
        int vi = idx;
        if (split > 0 && vi > 0) vi--;
        if (vi < 0) return -1;
        if (vi < split) return vi;
        if (split < g_displayCount) vi--;
        if (vi >= 0 && vi < g_displayCount - split)
            return split + vi;
        return -1;
    }
    return (idx >= 0 && idx < g_displayCount) ? idx : -1;
}

/* ============================================================
 *  БОКОВАЯ ПАНЕЛЬ
 * ============================================================ */

void drawSidebar(void) {
    DrawRectangle(0, 0, SIDE_PANEL_WIDTH, SCRH, SIDE_BG);
    DrawLine(SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, SCRH, SIDE_LINE);

    int sy = 8, by;

    /* Nav: Таблица (только на странице файла) */
    if (g_currentPage == PAGE_FILE) {
        if (drawButton(4, sy, SIDE_PANEL_WIDTH-8, BTN_H, "< Таблица",
                       (Color){42,44,52,255}, (Color){55,58,68,255}))
            g_currentPage = PAGE_TABLE;
        sy += BTN_H + 8;
    }

    /* --- ДЕЙСТВИЯ --- */
    {
        int rows = 3;
        Rectangle r = { 4, (float)sy, SIDE_PANEL_WIDTH-8, rows*(BTN_H+4)+22 };
        DrawRectangleRounded(r, 0.08f, 6, (Color){ 42, 44, 52, 255 });
        DrawRectangleRoundedLines(r, 0.08f, 6, PANEL_BORDER);
        by = sy + 6;
        DrawTextEx(g_font, "ДЕЙСТВИЯ", (Vector2){ 10, by }, 13, 1, LABEL_COLOR);
        by += 18;

        if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Добавить", BTN_ADD, BTN_ADD_HOVER))
            action_add();
        by += BTN_H + 4;

        if (g_editParams.isEditMode) {
            if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Готово", BTN_SAVE, BTN_SAVE_HOVER))
                action_confirm_edit();
            by += BTN_H + 4;
            if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Отмена", BTN_DEL, BTN_DEL_HOVER))
                action_cancel_edit();
            by += BTN_H + 4;
        } else {
            if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Изменить", BTN_COLOR, BTN_HOVER))
                action_edit_mode();
            by += BTN_H + 4;
            if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Удалить", BTN_DEL, BTN_DEL_HOVER))
                action_delete();
            by += BTN_H + 4;
        }
    }
    sy += 3*(BTN_H+4) + 26;

    /* --- ФИЛЬТРЫ --- */
    {
        Rectangle r = { 4, (float)sy, SIDE_PANEL_WIDTH-8, 3*(BTN_H+4)+22 };
        DrawRectangleRounded(r, 0.08f, 6, (Color){ 42, 44, 52, 255 });
        DrawRectangleRoundedLines(r, 0.08f, 6, PANEL_BORDER);
        by = sy + 6;
        DrawTextEx(g_font, "ФИЛЬТРЫ", (Vector2){ 10, by }, 13, 1, LABEL_COLOR);
        by += 18;
        if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Сортировка", BTN_COLOR, BTN_HOVER))
            action_sort();
        by += BTN_H + 4;
        if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Отличники (Бюджет)", BTN_COLOR, BTN_HOVER))
            action_excellent_paid();
        by += BTN_H + 4;
        if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "По форме", BTN_COLOR, BTN_HOVER))
            action_by_form();
    }
    sy += 3*(BTN_H+4) + 26;

    /* Nav: Файл */
    {
        Color c = (g_currentPage == PAGE_FILE) ? (Color){55,58,68,255} : (Color){42,44,52,255};
        if (drawButton(4, sy, SIDE_PANEL_WIDTH-8, BTN_H, "> Файл", c, (Color){60,64,75,255}))
            g_currentPage = PAGE_FILE;
        sy += BTN_H + 8;
    }

    /* --- СИСТЕМА --- */
    {
        Rectangle r = { 4, (float)sy, SIDE_PANEL_WIDTH-8, 2*(BTN_H+4)+22 };
        DrawRectangleRounded(r, 0.08f, 6, (Color){ 42, 44, 52, 255 });
        DrawRectangleRoundedLines(r, 0.08f, 6, PANEL_BORDER);
        by = sy + 6;
        DrawTextEx(g_font, "СИСТЕМА", (Vector2){ 10, by }, 13, 1, LABEL_COLOR);
        by += 18;
        if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Очистить", BTN_CLEAR, BTN_CLEAR_HOVER))
            action_clear();
        by += BTN_H + 4;
        if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Отменить", BTN_UNDO, BTN_UNDO_HOVER))
            action_undo();
    }
}

/* ============================================================
 *  СТРАНИЦА ФАЙЛОВ
 * ============================================================ */

void drawFilePage(void) {
    int fx = MAIN_CONTENT_OFFSET_X + 30, fy = PANEL_Y + 10;
    DrawTextEx(g_font, "Файловые операции", (Vector2){ (float)fx, (float)fy }, 22, 1, BTN_TEXT);
    fy += 40;

    const char* fnames[] = { "Сохранить CSV","Загрузить CSV","Импорт CSV","Экспорт CSV","Экспорт TXT" };
    const char* fdesc[] = {
        "Сохранить всю базу в students.csv",
        "Загрузить базу из students.csv (заменяет текущую)",
        "Добавить записи из произвольного CSV-файла",
        "Экспортировать отображаемый список в CSV-файл",
        "Экспортировать отчёт в текстовый файл"
    };

    for (int i = 0; i < 5; i++) {
        Color bc = (i < 2) ? BTN_SAVE : BTN_COLOR;
        Color bh = (i < 2) ? BTN_SAVE_HOVER : BTN_HOVER;
        if (drawButton(fx, fy, 210, BTN_H+4, fnames[i], bc, bh)) {
            switch (i) {
                case 0: saveDbCsv(CSV_FILE, g_head); break;
                case 1:
                    saveUndoState();
                    clearList(&g_head);
                    loadDbCsv(CSV_FILE, &g_head);
                    clearInputFields();
                    g_selectedRowIndex = -1;
                    rebuildDisplay();
                    break;
                case 2: {
                    char fname[MAX_PATH] = "";
                    extern int file_dialog_open(char*, size_t);
                    if (file_dialog_open(fname, sizeof(fname)))
                        import_csv(fname, &g_head);
                    break;
                }
                case 3:
                    export_csv(EXPORT_CSV_FILE, g_displayArray, g_displayCount);
                    break;
                case 4:
                    export_txt(EXPORT_TXT_FILE, g_displayArray, g_displayCount);
                    break;
            }
        }
        DrawTextEx(g_font, fdesc[i], (Vector2){ (float)(fx + 220), fy+6 }, 15, 1, LABEL_COLOR);
        fy += BTN_H + 16;
    }
}

/* ============================================================
 *  ГЛАВНОЕ ОКНО (страница таблицы)
 * ============================================================ */

void drawMainWindow(void) {
    /* Фон панели редактирования */
    Rectangle pr = { (float)MAIN_CONTENT_OFFSET_X+8, (float)PANEL_Y,
                     SCRW - MAIN_CONTENT_OFFSET_X - 16, PANEL_H };
    DrawRectangleRounded(pr, 0.08f, 6, PANEL_BG);
    DrawRectangleRoundedLines(pr, 0.08f, 6, PANEL_BORDER);

    /* Первый ряд — поля ввода */
    if (drawTextBox(MAIN_CONTENT_OFFSET_X+10, INPUT_Y, 400, INPUT_H, "Специальность:",
                g_textBox.buffer1, g_textBox.activeField == 0))
        g_textBox.activeField = 0;
    if (drawTextBox(MAIN_CONTENT_OFFSET_X+420, INPUT_Y, 130, INPUT_H, "Группа:",
                g_textBox.buffer2, g_textBox.activeField == 1))
        g_textBox.activeField = 1;
    if (drawTextBox(MAIN_CONTENT_OFFSET_X+560, INPUT_Y, 350, INPUT_H, "ФИО:",
                g_textBox.buffer3, g_textBox.activeField == 2))
        g_textBox.activeField = 2;
    if (drawTextBox(MAIN_CONTENT_OFFSET_X+920, INPUT_Y, 260, INPUT_H, "Форма обучения:",
                g_textBox.buffer4, g_textBox.activeField == 3))
        g_textBox.activeField = 3;

    /* Второй ряд — оценки + радиокнопки фильтра */
    DrawTextEx(g_font, "Оценки:",
               (Vector2){ (float)(MAIN_CONTENT_OFFSET_X + 10), INPUT_Y2 + (INPUT_H - FONT_SIZE_INTERFACE) / 2.0f },
               FONT_SIZE_INTERFACE, 1, LABEL_COLOR);

    int gx = MAIN_CONTENT_OFFSET_X + 10 + (int)MeasureTextEx(g_font, "Оценки:", FONT_SIZE_INTERFACE, 1).x + 12;
    int grade_input_w = 28;
    for (int i = 0; i < MAX_GRADES; i++) {
        char label[8];
        snprintf(label, sizeof(label), "%d:", i + 1);
        float lw = MeasureTextEx(g_font, label, FONT_SIZE_INTERFACE, 1).x + DEFAULT_PADDING - 4;
        int tw = (int)lw + grade_input_w;

        if (drawTextBox(gx, INPUT_Y2, tw, INPUT_H, label,
                    getGradeBuffer(i), g_textBox.activeField == 4 + i))
            g_textBox.activeField = 4 + i;
        gx += tw + 4;
    }

    /* Радиокнопки фильтра по форме (второй ряд, после оценок) */
    const char* form_items[] = { "Все", "Бюджет", "Плат" };
    {
        int old = g_filterForm;
        g_filterForm = drawRadioGroup(gx + 8, INPUT_Y2, INPUT_H,
                                       form_items, 3, g_filterForm);
        if (g_filterForm != old && !g_editParams.isEditMode) {
            apply_filter();
        }
    }

    /* Таблица */
    drawTable();

    /* Информационная строка */
    {
        const char* mode = "";
        switch (g_currentViewMode) {
            case VIEW_ALL:            mode = "Все студенты"; break;
            case VIEW_SORTED:         mode = "Сортировка по группам и ФИО"; break;
            case VIEW_EXCELLENT_PAID: mode = "Отличники (Бюджет, ср. балл >= 8.0)"; break;
            case VIEW_BY_FORM:        mode = "Списки по форме обучения"; break;
        }
        char info[128];
        snprintf(info, sizeof(info), "Режим: %s  |  Показано: %d из %d",
                 mode, g_displayCount, countNodes(g_head));
        DrawTextEx(g_font, info, (Vector2){ (float)MAIN_CONTENT_OFFSET_X, (float)SCRH - 22 },
                   16, 1, LABEL_COLOR);

        double elapsed = GetTime() - g_statusTime;
        if (elapsed < 4.0 && g_statusMsg[0]) {
            Color sc = (strncmp(g_statusMsg, "Ошибка", 6) == 0) ? STATUS_ERR : STATUS_OK;
            if (elapsed > 3.0) sc.a = (unsigned char)(255 - (int)((elapsed - 3.0) * 255));
            Vector2 sz = MeasureTextEx(g_font, g_statusMsg, 16, 1);
            DrawTextEx(g_font, g_statusMsg,
                       (Vector2){ (float)(SCRW - sz.x - 10), (float)SCRH - 22 },
                       16, 1, sc);
        }
    }
}
