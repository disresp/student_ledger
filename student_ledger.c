/*
 * ============================================================
 *  Ведомость об успеваемости студентов
 *  Язык C + библиотека Raylib (графический интерфейс)
 *
 *  Функции:
 *    1. Сортировка по группе, затем по ФИО
 *    2. Фильтр: отличники (оценки 9-10) + платная форма
 *    3. Списки по форме обучения (бюджет / плат),
 *       отсортированные по убыванию среднего балла
 *    4. Поиск по ФИО (частично, без учёта регистра),
 *       номеру группы (точно), форме обучения (точно)
 *
 *  Сборка (MSYS2 MinGW64):
 *    gcc student_ledger.c -o student_ledger.exe ^
 *        -IC:/msys64/mingw64/include          ^
 *        -LC:/msys64/mingw64/lib              ^
 *        -lraylib -lopengl32 -lgdi32 -lwinmm
 * ============================================================
 */

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ==========================================================
 *  КОНСТАНТЫ
 * ========================================================== */

#define SCREEN_W   1200
#define SCREEN_H   700
#define MAX_STUD   10

#define MAX_SPEC   50
#define MAX_NAME   100
#define MAX_FORM   24
#define GRADES     4

/* Зоны экрана (в пикселях) */
#define BTN_Y      10
#define BTN_H      36
#define SEARCH_Y   55
#define BOX_H      34
#define HEADER_Y   105
#define ROW_Y_START 130
#define ROW_H      22
#define TABLE_H    540

/* Цвета интерфейса */
#define BG_COLOR     CLITERAL(Color){ 245, 245, 255, 255 }
#define HEADER_BG    CLITERAL(Color){  60,  60, 120, 255 }
#define HEADER_TEXT  CLITERAL(Color){ 255, 255, 255, 255 }
#define ROW_EVEN     CLITERAL(Color){ 255, 255, 255, 255 }
#define ROW_ODD      CLITERAL(Color){ 235, 240, 250, 255 }
#define BTN_COLOR    CLITERAL(Color){  70, 100, 180, 255 }
#define BTN_HOVER    CLITERAL(Color){  90, 130, 220, 255 }
#define BTN_TEXT     CLITERAL(Color){ 255, 255, 255, 255 }
#define INPUT_BG     CLITERAL(Color){ 255, 255, 255, 255 }
#define INPUT_BORDER CLITERAL(Color){ 100, 100, 100, 255 }
#define FOCUS_BORDER CLITERAL(Color){  30,  60, 180, 255 }
#define RADIO_ACT    CLITERAL(Color){  70, 100, 180, 255 }
#define RADIO_INACT  CLITERAL(Color){ 180, 180, 180, 255 }
#define TEXT_COLOR   CLITERAL(Color){  20,  20,  20, 255 }
#define LABEL_COLOR  CLITERAL(Color){  50,  50,  80, 255 }
#define TITLE_BUDGET CLITERAL(Color){   0, 130,  50, 255 }
#define TITLE_PAID   CLITERAL(Color){ 200,  80,  30, 255 }

/* ==========================================================
 *  СТРУКТУРА СТУДЕНТА
 * ========================================================== */

typedef struct {
    char  speciality[MAX_SPEC + 1];
    int   group;
    char  full_name[MAX_NAME + 1];
    char  form[MAX_FORM + 1];
    int   grades[GRADES];
    float avg_score;
} Student;

/* ==========================================================
 *  РЕЖИМЫ ПРОСМОТРА
 * ========================================================== */

typedef enum {
    VIEW_ALL,
    VIEW_SORTED,
    VIEW_EXCELLENT_PAID,
    VIEW_BY_FORM,
    VIEW_SEARCH
} ViewMode;

/* ==========================================================
 *  ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ========================================================== */

static Student g_students[MAX_STUD];
static Student *g_display[MAX_STUD];
static int g_display_count = 0;
static ViewMode g_view = VIEW_ALL;
static float g_scroll = 0.0f;

static char g_search_name[MAX_NAME + 1] = "";
static int  g_search_name_len = 0;
static char g_search_group[8] = "";
static int  g_search_group_len = 0;
static int  g_search_form = 0;  /* 0 = все, 1 = бюджет, 2 = плат */
static int  g_focus = -1;

static Font g_font;

/* ==========================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ========================================================== */

static float calc_avg(const int grades[GRADES]) {
    int sum = 0;
    for (int i = 0; i < GRADES; i++) sum += grades[i];
    return (float)sum / (float)GRADES;
}

static int cmp_group_name(const void *a, const void *b) {
    const Student *sa = *(const Student **)a;
    const Student *sb = *(const Student **)b;
    if (sa->group != sb->group) return sa->group - sb->group;
    return stricmp(sa->full_name, sb->full_name);
}

static int cmp_avg_desc(const void *a, const void *b) {
    const Student *sa = *(const Student **)a;
    const Student *sb = *(const Student **)b;
    if (fabs(sa->avg_score - sb->avg_score) > 0.001f)
        return (sa->avg_score > sb->avg_score) ? -1 : 1;
    return stricmp(sa->full_name, sb->full_name);
}

static int str_icontains(const char *str, const char *substr) {
    if (!*substr) return 1;
    size_t l1 = strlen(str);
    size_t l2 = strlen(substr);
    if (l2 > l1) return 0;
    for (size_t i = 0; i <= l1 - l2; i++) {
        int match = 1;
        for (size_t j = 0; j < l2; j++) {
            if (tolower((unsigned char)str[i + j]) !=
                tolower((unsigned char)substr[j])) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

static void reset_display(void) {
    for (int i = 0; i < MAX_STUD; i++) g_display[i] = &g_students[i];
    g_display_count = MAX_STUD;
}

/* ==========================================================
 *  ДЕЙСТВИЯ
 * ========================================================== */

static void action_sort(void) {
    reset_display();
    qsort(g_display, g_display_count, sizeof(Student *), cmp_group_name);
    g_view = VIEW_SORTED;
    g_scroll = 0.0f;
}

static void action_excellent_paid(void) {
    g_display_count = 0;
    for (int i = 0; i < MAX_STUD; i++) {
        int excellent = 1;
        for (int k = 0; k < GRADES; k++)
            if (g_students[i].grades[k] < 9) { excellent = 0; break; }
        if (excellent && strcmp(g_students[i].form, "платная") == 0)
            g_display[g_display_count++] = &g_students[i];
    }
    g_view = VIEW_EXCELLENT_PAID;
    g_scroll = 0.0f;
}

static void action_by_form(void) {
    Student *budget[MAX_STUD], *paid[MAX_STUD];
    int bc = 0, pc = 0;
    for (int i = 0; i < MAX_STUD; i++) {
        if (strcmp(g_students[i].form, "бюджетная") == 0)
            budget[bc++] = &g_students[i];
        else
            paid[pc++] = &g_students[i];
    }
    qsort(budget, bc, sizeof(Student *), cmp_avg_desc);
    qsort(paid, pc, sizeof(Student *), cmp_avg_desc);
    g_display_count = 0;
    for (int i = 0; i < bc; i++) g_display[g_display_count++] = budget[i];
    for (int i = 0; i < pc; i++) g_display[g_display_count++] = paid[i];
    g_view = VIEW_BY_FORM;
    g_scroll = 0.0f;
}

static void action_search(void) {
    g_display_count = 0;
    int grp = (g_search_group_len > 0) ? atoi(g_search_group) : 0;
    for (int i = 0; i < MAX_STUD; i++) {
        const Student *s = &g_students[i];
        if (g_search_name_len > 0 && !str_icontains(s->full_name, g_search_name))
            continue;
        if (grp > 0 && s->group != grp) continue;
        if (g_search_form == 1 && strcmp(s->form, "бюджетная") != 0) continue;
        if (g_search_form == 2 && strcmp(s->form, "платная") != 0) continue;
        g_display[g_display_count++] = &g_students[i];
    }
    g_view = VIEW_SEARCH;
    g_scroll = 0.0f;
}

static void action_reset(void) {
    reset_display();
    g_view = VIEW_ALL;
    g_scroll = 0.0f;
    g_search_name[0] = '\0';  g_search_name_len = 0;
    g_search_group[0] = '\0'; g_search_group_len = 0;
    g_search_form = 0;
    g_focus = -1;
}

/* ==========================================================
 *  ТЕСТОВЫЕ ДАННЫЕ (10 студентов)
 * ========================================================== */

static void fill_sample_data(void) {
    struct {
        const char *spec; int grp; const char *name; const char *form;
        int g1, g2, g3, g4;
    } data[MAX_STUD] = {
        { "Программная инженерия", 101, "Петров Алексей Иванович",    "бюджетная", 9, 8, 7, 9 },
        { "Программная инженерия", 101, "Сидоров Борис Владимирович", "платная",   10, 9, 10, 9 },
        { "Программная инженерия", 101, "Иванова Вера Константиновна","бюджетная", 6, 7, 5, 8 },
        { "Прикладная математика", 102, "Смирнов Глеб Дмитриевич",    "платная",   9, 9, 10, 10 },
        { "Прикладная математика", 102, "Козлова Дарья Евгеньевна",   "бюджетная", 8, 8, 9, 7 },
        { "Прикладная математика", 102, "Новиков Евгений Жорович",    "платная",   4, 5, 6, 5 },
        { "Информационные системы", 201, "Попова Жанна Зиновьевна",   "бюджетная", 10, 10, 9, 10 },
        { "Информационные системы", 201, "Васильев Илья Игоревич",    "платная",   7, 6, 8, 7 },
        { "Информационные системы", 201, "Морозов Кирилл Леонидович", "бюджетная", 5, 6, 4, 7 },
        { "Информационные системы", 201, "Фёдорова Лидия Михайловна", "платная",   10, 9, 9, 10 },
    };
    for (int i = 0; i < MAX_STUD; i++) {
        strcpy(g_students[i].speciality, data[i].spec);
        g_students[i].group = data[i].grp;
        strcpy(g_students[i].full_name, data[i].name);
        strcpy(g_students[i].form, data[i].form);
        g_students[i].grades[0] = data[i].g1;
        g_students[i].grades[1] = data[i].g2;
        g_students[i].grades[2] = data[i].g3;
        g_students[i].grades[3] = data[i].g4;
        g_students[i].avg_score = calc_avg(g_students[i].grades);
    }
}

/* ==========================================================
 *  ОТРИСОВКА ЭЛЕМЕНТОВ
 * ========================================================== */

static int btn(int x, int y, int w, int h, const char *text,
               Color col, Color hover) {
    Rectangle r = { (float)x, (float)y, (float)w, (float)h };
    Vector2 m = GetMousePosition();
    Color c = CheckCollisionPointRec(m, r) ? hover : col;
    DrawRectangleRounded(r, 0.15f, 6, c);
    DrawRectangleRoundedLines(r, 0.15f, 6, (Color){ 0,0,0,30 });
    Vector2 sz = MeasureTextEx(g_font, text, 16, 1);
    DrawTextEx(g_font, text,
               (Vector2){ r.x + (w - sz.x)/2, r.y + (h - sz.y)/2 },
               16, 1, BTN_TEXT);
    return CheckCollisionPointRec(m, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static int textbox(int x, int y, int w, int h, const char *label,
                   const char *buf, int focused) {
    Vector2 ls = MeasureTextEx(g_font, label, 14, 1);
    int lw = (int)ls.x + 6;
    DrawTextEx(g_font, label, (Vector2){ (float)x, y + (h - 14)/2 },
               14, 1, LABEL_COLOR);
    Rectangle r = { (float)(x + lw), (float)y, (float)(w - lw), (float)h };
    DrawRectangleRec(r, INPUT_BG);
    DrawRectangleLinesEx(r, focused ? 2 : 1, focused ? FOCUS_BORDER : INPUT_BORDER);
    const char *display = buf;
    char truncated[128];
    Vector2 ts = MeasureTextEx(g_font, display, 14, 1);
    if (ts.x > r.width - 8) {
        int len = (int)strlen(display);
        int start = len;
        while (start > 0) {
            Vector2 t = MeasureTextEx(g_font, display + start, 14, 1);
            if (t.x < r.width - 8) break;
            start--;
        }
        strcpy(truncated, display + start);
        display = truncated;
        ts = MeasureTextEx(g_font, display, 14, 1);
    }
    DrawTextEx(g_font, display,
               (Vector2){ r.x + 4, y + (h - 14)/2 }, 14, 1, TEXT_COLOR);
    if (focused && ((int)(GetTime() * 2) % 2 == 0)) {
        float cx = r.x + 4 + ts.x;
        DrawLineV((Vector2){ cx, r.y + 4 }, (Vector2){ cx, r.y + h - 4 },
                  (Color){ 30, 60, 180, 200 });
    }
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static int radio_group(int x, int y, int h, const char *items[],
                       int count, int selected) {
    int cx = x;
    for (int i = 0; i < count; i++) {
        int tw = (int)MeasureTextEx(g_font, items[i], 14, 1).x;
        int bw = tw + 22;
        Rectangle r = { (float)cx, (float)y, (float)bw, (float)h };
        Color c = (i == selected) ? RADIO_ACT : RADIO_INACT;
        DrawCircle(cx + 10, y + h/2, 7, c);
        if (i == selected)
            DrawCircle(cx + 10, y + h/2, 3, (Color){ 255,255,255,255 });
        DrawTextEx(g_font, items[i],
                   (Vector2){ (float)(cx + 20), y + (h - 14)/2 },
                   14, 1, LABEL_COLOR);
        if (CheckCollisionPointRec(GetMousePosition(), r) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            selected = i;
        cx += bw + 4;
    }
    return selected;
}

/* Позиции колонок таблицы */
static const int COL_X[] = { 10, 45, 115, 410, 650, 800, 940 };
static const int COL_W[] = { 35, 70, 295, 240, 150, 140, 260 };

/* Обрезать текст по ширине колонки */
static void draw_col_text(int col, int y, int font_sz, const char *text, Color color) {
    Vector2 sz = MeasureTextEx(g_font, text, font_sz, 1);
    int max_w = COL_W[col] - 6;
    if (sz.x <= max_w) {
        DrawTextEx(g_font, text, (Vector2){ (float)COL_X[col], y }, font_sz, 1, color);
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
            DrawTextEx(g_font, buf, (Vector2){ (float)COL_X[col], y }, font_sz, 1, color);
            return;
        }
        byte_len = cut;
    }
}

/* Заголовок таблицы */
static void draw_header(void) {
    DrawRectangle(0, HEADER_Y, SCREEN_W, ROW_H, HEADER_BG);
    const char *titles[] = {
        "#", "Группа", "ФИО", "Специальность",
        "Форма", "Оценки", "Ср. балл"
    };
    for (int i = 0; i < 7; i++)
        DrawTextEx(g_font, titles[i],
                   (Vector2){ (float)COL_X[i], HEADER_Y + (ROW_H - 16)/2 },
                   16, 1, HEADER_TEXT);
}

/* Одна строка таблицы */
static void draw_row(int idx, int y, const Student *s) {
    DrawRectangle(0, y, SCREEN_W, ROW_H, (idx % 2 == 0) ? ROW_EVEN : ROW_ODD);
    char b[7][64];
    snprintf(b[0], sizeof(b[0]), "%d", idx + 1);
    snprintf(b[1], sizeof(b[1]), "%d", s->group);
    strcpy(b[2], s->full_name);
    strcpy(b[3], s->speciality);
    strcpy(b[4], s->form);
    snprintf(b[5], sizeof(b[5]), "%d %d %d %d",
             s->grades[0], s->grades[1], s->grades[2], s->grades[3]);
    snprintf(b[6], sizeof(b[6]), "%.2f", s->avg_score);
    Color cc[] = {
        TEXT_COLOR, TEXT_COLOR, TEXT_COLOR, TEXT_COLOR,
        (strcmp(s->form, "бюджетная") == 0) ? TITLE_BUDGET : TITLE_PAID,
        TEXT_COLOR, TEXT_COLOR
    };
    for (int i = 0; i < 7; i++)
        draw_col_text(i, y + (ROW_H - 14) / 2, 14, b[i], cc[i]);
}

/* Полоска-заголовок секции (бюджет / плат) */
static void draw_section(int y, const char *title, Color bg) {
    DrawRectangle(0, y, SCREEN_W, ROW_H, bg);
    Vector2 sz = MeasureTextEx(g_font, title, 15, 1);
    DrawTextEx(g_font, title,
               (Vector2){ (SCREEN_W - sz.x)/2, y + (ROW_H - 15)/2 },
               15, 1, (Color){ 255, 255, 255, 255 });
}

/* Основная таблица с прокруткой */
static void draw_table(void) {
    draw_header();
    BeginScissorMode(0, ROW_Y_START, SCREEN_W, TABLE_H);
    int y = ROW_Y_START - (int)g_scroll;
    int ri = 0;

    if (g_view == VIEW_BY_FORM) {
        int split = 0;
        for (int i = 0; i < g_display_count; i++)
            if (strcmp(g_display[i]->form, "платная") == 0) { split = i; break; }
        if (split > 0) {
            draw_section(y, "--- БЮДЖЕТНАЯ ФОРМА ---", TITLE_BUDGET);
            y += ROW_H; ri++;
        }
        for (int i = 0; i < split; i++) {
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, g_display[i]);
            y += ROW_H; ri++;
        }
        if (split < g_display_count) {
            draw_section(y, "--- ПЛАТНАЯ ФОРМА ---", TITLE_PAID);
            y += ROW_H; ri++;
        }
        for (int i = split; i < g_display_count; i++) {
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, g_display[i]);
            y += ROW_H; ri++;
        }
    } else {
        for (int i = 0; i < g_display_count; i++) {
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, g_display[i]);
            y += ROW_H; ri++;
        }
    }
    EndScissorMode();
}

/* ==========================================================
 *  ОБРАБОТКА ВВОДА
 * ========================================================== */

static void utf8_append(char *buf, int *len, int max, int codepoint) {
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

static void utf8_pop(char *buf, int *len) {
    if (*len <= 0) return;
    int i = *len - 1;
    while (i > 0 && ((unsigned char)buf[i] & 0xC0) == 0x80) i--;
    *len = i;
    buf[*len] = '\0';
}

static void handle_keys(void) {
    if (g_focus < 0) return;
    int c = GetCharPressed();
    while (c > 0) {
        int printable = (c >= 32 && c <= 126) ||
                        (c >= 0x400 && c <= 0x4FF) ||
                        (c >= 0x500 && c <= 0x52F);
        if (printable) {
            if (g_focus == 0)
                utf8_append(g_search_name, &g_search_name_len, MAX_NAME, c);
            else if (g_focus == 1 && c >= '0' && c <= '9' && g_search_group_len < 6)
                utf8_append(g_search_group, &g_search_group_len, 6, c);
        }
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (g_focus == 0) utf8_pop(g_search_name, &g_search_name_len);
        else if (g_focus == 1) utf8_pop(g_search_group, &g_search_group_len);
    }
}

/* ==========================================================
 *  ГЛАВНАЯ ФУНКЦИЯ
 * ========================================================== */

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "Ведомость об успеваемости студентов");
    SetTargetFPS(60);

    /* Загружаем шрифт с кириллицей */
    {
        int codepoints[192], cp_count = 0;
        for (int i = 32; i < 127; i++) codepoints[cp_count++] = i;
        for (int i = 0x0410; i <= 0x044F; i++) codepoints[cp_count++] = i;
        codepoints[cp_count++] = 0x0401;
        codepoints[cp_count++] = 0x0451;

        g_font = LoadFontEx("C:/Windows/Fonts/arial.ttf", 24, codepoints, cp_count);
        SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);
    }

    fill_sample_data();
    reset_display();
    g_view = VIEW_ALL;

    while (!WindowShouldClose()) {
        /* Ввод */
        handle_keys();

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            int extra = (g_view == VIEW_BY_FORM) ? 2 : 0;
            int total_h = (g_display_count + extra) * ROW_H;
            int max_scroll = (total_h > TABLE_H) ? total_h - TABLE_H : 0;
            g_scroll -= wheel * 20.0f;
            if (g_scroll < 0) g_scroll = 0;
            if (g_scroll > max_scroll) g_scroll = (float)max_scroll;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 m = GetMousePosition();
            int l1 = (int)MeasureTextEx(g_font, "ФИО:", 14, 1).x + 6;
            int l2 = (int)MeasureTextEx(g_font, "Группа:", 14, 1).x + 6;
            int nx = 10 + l1, nw = 300;
            int gx = 330 + l2, gw = 100;
            g_focus = -1;
            if (CheckCollisionPointRec(m, (Rectangle){ (float)nx, SEARCH_Y, (float)nw, BOX_H })) g_focus = 0;
            else if (CheckCollisionPointRec(m, (Rectangle){ (float)gx, SEARCH_Y, (float)gw, BOX_H })) g_focus = 1;
        }

        /* Кнопки */
        if (btn(10,  BTN_Y, 210, BTN_H, "Сортировка по группам/ФИО", BTN_COLOR, BTN_HOVER))
            action_sort();
        if (btn(230, BTN_Y, 200, BTN_H, "Отличники (платные)", BTN_COLOR, BTN_HOVER))
            action_excellent_paid();
        if (btn(440, BTN_Y, 230, BTN_H, "Списки по форме обучения", BTN_COLOR, BTN_HOVER))
            action_by_form();
        btn(680, BTN_Y, 100, BTN_H, "Поиск", BTN_COLOR, BTN_HOVER);

        textbox(10,  SEARCH_Y, 310, BOX_H, "ФИО:", g_search_name, g_focus == 0);
        textbox(330, SEARCH_Y, 155, BOX_H, "Группа:", g_search_group, g_focus == 1);

        const char *form_items[] = { "Все", "Бюджет", "Плат" };
        {
            int old_form = g_search_form;
            g_search_form = radio_group(510, SEARCH_Y, BOX_H, form_items, 3, g_search_form);
            if (g_search_form != old_form) action_search();
        }

        if (btn(700, SEARCH_Y, 90, BOX_H, "Найти",
                (Color){ 50, 160, 50, 255 }, (Color){ 70, 200, 70, 255 }))
            action_search();
        if (btn(800, SEARCH_Y, 90, BOX_H, "Сброс",
                (Color){ 160, 60, 60, 255 }, (Color){ 200, 80, 80, 255 }))
            action_reset();

        /* Отрисовка */
        BeginDrawing();
        ClearBackground(BG_COLOR);
        draw_table();

        const char *mode = "";
        switch (g_view) {
            case VIEW_ALL:            mode = "Все студенты"; break;
            case VIEW_SORTED:         mode = "Сортировка по группам и ФИО"; break;
            case VIEW_EXCELLENT_PAID: mode = "Отличники (платная форма)"; break;
            case VIEW_BY_FORM:        mode = "Списки по форме обучения"; break;
            case VIEW_SEARCH:         mode = "Результат поиска"; break;
        }
        char info[128];
        snprintf(info, sizeof(info), "Режим: %s  |  Показано: %d из %d",
                 mode, g_display_count, MAX_STUD);
        DrawTextEx(g_font, info, (Vector2){ 10, SCREEN_H - 22 }, 14, 1, LABEL_COLOR);
        EndDrawing();
    }

    UnloadFont(g_font);
    CloseWindow();
    return 0;
}
