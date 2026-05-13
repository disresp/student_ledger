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

#define SCREEN_W   1200    /* ширина окна */
#define SCREEN_H   700     /* высота окна */
#define MAX_STUD   10      /* количество студентов (фикс. массив) */

#define MAX_SPEC   50      /* макс. длина специальности */
#define MAX_NAME   100     /* макс. длина ФИО */
#define MAX_FORM   12      /* макс. длина формы обучения */
#define GRADES     4       /* количество оценок */

/* Зоны экрана (в пикселях) */
#define BTN_Y      10      /* Y кнопок */
#define BTN_H      36      /* высота кнопок */
#define SEARCH_Y   55      /* Y строки поиска */
#define BOX_H      34      /* высота текстовых полей и радио-кнопок */
#define HEADER_Y   105     /* Y заголовка таблицы */
#define ROW_Y_START 130    /* Y начала строк таблицы */
#define ROW_H      22      /* высота одной строки */
#define TABLE_H    540     /* высота таблицы (от ROW_Y_START до низа) */

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
    char  speciality[MAX_SPEC + 1]; /* наименование специальности */
    int   group;                     /* номер группы */
    char  full_name[MAX_NAME + 1];  /* ФИО студента */
    char  form[MAX_FORM + 1];       /* "бюджетная" или "платная" */
    int   grades[GRADES];           /* оценки (каждая от 1 до 10) */
    float avg_score;                /* средний балл (вычисляется) */
} Student;

/* ==========================================================
 *  РЕЖИМЫ ПРОСМОТРА
 * ========================================================== */

typedef enum {
    VIEW_ALL,             /* исходные данные */
    VIEW_SORTED,          /* отсортировано по группе / ФИО */
    VIEW_EXCELLENT_PAID,  /* отличники на платном */
    VIEW_BY_FORM,         /* списки по форме обучения */
    VIEW_SEARCH           /* результат поиска */
} ViewMode;

/* ==========================================================
 *  ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ========================================================== */

/* Мастер-массив студентов (не изменяется) */
static Student g_students[MAX_STUD];

/* Массив указателей на отображаемых студентов + их количество */
static Student *g_display[MAX_STUD];
static int g_display_count = 0;

/* Текущий режим просмотра */
static ViewMode g_view = VIEW_ALL;

/* Прокрутка (в пикселях) */
static float g_scroll = 0.0f;

/* Поля для поиска */
static char g_search_name[MAX_NAME + 1] = "";
static int  g_search_name_len = 0;
static char g_search_group[8] = "";
static int  g_search_group_len = 0;

/* Радио-кнопки формы: 0 = "Все", 1 = "Бюджет", 2 = "Плат" */
static int g_search_form = 0;

/* Фокус поля ввода: -1 = нет, 0 = ФИО, 1 = группа */
static int g_focus = -1;

/* Шрифт с кириллицей */
static Font g_font;

/* Координаты текстовых полей (вычисляются один раз) */
static int g_tb_name_x = 0, g_tb_name_w = 0;
static int g_tb_group_x = 0, g_tb_group_w = 0;

/* ==========================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ========================================================== */

/* Вычисление среднего балла */
static float calc_avg(const int grades[GRADES]) {
    int sum = 0;
    for (int i = 0; i < GRADES; i++) sum += grades[i];
    return (float)sum / (float)GRADES;
}

/* Сравнение для сортировки по group, затем по full_name */
static int cmp_group_name(const void *a, const void *b) {
    const Student *sa = *(const Student **)a;
    const Student *sb = *(const Student **)b;
    if (sa->group != sb->group) return sa->group - sb->group;
    return stricmp(sa->full_name, sb->full_name);
}

/* Сравнение по среднему баллу (убывание), при равенстве — по ФИО */
static int cmp_avg_desc(const void *a, const void *b) {
    const Student *sa = *(const Student **)a;
    const Student *sb = *(const Student **)b;
    if (fabs(sa->avg_score - sb->avg_score) > 0.001f)
        return (sa->avg_score > sb->avg_score) ? -1 : 1;
    return stricmp(sa->full_name, sb->full_name);
}

/* Проверка: содержит ли str подстроку substr (без учёта регистра) */
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

/* Заполнить g_display всеми студентами */
static void reset_display(void) {
    for (int i = 0; i < MAX_STUD; i++) g_display[i] = &g_students[i];
    g_display_count = MAX_STUD;
}

/* ==========================================================
 *  ДЕЙСТВИЯ
 * ========================================================== */

/* 1. Сортировка: группировка по group, внутри — по ФИО */
static void action_sort(void) {
    reset_display();
    qsort(g_display, g_display_count, sizeof(Student *), cmp_group_name);
    g_view = VIEW_SORTED;
    g_scroll = 0.0f;
}

/* 2. Отличники (9-10) + платная форма */
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

/* 3. Списки по форме обучения, убывание среднего балла */
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

/* 4. Поиск: комбинированный фильтр */
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

/* Сброс к исходному виду */
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
        { "Программная инженерия", 101, "Петров Алексей Иванович",     "бюджетная", 9, 8, 7, 9 },
        { "Программная инженерия", 101, "Сидоров Борис Владимирович",  "платная",   10, 9, 10, 9 },
        { "Программная инженерия", 101, "Иванова Вера Константиновна", "бюджетная", 6, 7, 5, 8 },
        { "Прикладная математика", 102, "Смирнов Глеб Дмитриевич",     "платная",   9, 9, 10, 10 },
        { "Прикладная математика", 102, "Козлова Дарья Евгеньевна",    "бюджетная", 8, 8, 9, 7 },
        { "Прикладная математика", 102, "Новиков Евгений Жорович",     "платная",   4, 5, 6, 5 },
        { "Информационные системы", 201, "Попова Жанна Зиновьевна",    "бюджетная", 10, 10, 9, 10 },
        { "Информационные системы", 201, "Васильев Илья Игоревич",     "платная",   7, 6, 8, 7 },
        { "Информационные системы", 201, "Морозов Кирилл Леонидович",  "бюджетная", 5, 6, 4, 7 },
        { "Информационные системы", 201, "Фёдорова Лидия Михайловна",  "платная",   10, 9, 9, 10 },
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
 *  ОТРИСОВКА ЭЛЕМЕНТОВ ИНТЕРФЕЙСА
 * ========================================================== */

/* Кнопка. Возвращает 1, если нажата в этом кадре. */
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

/* Поле ввода с подписью. Возвращает 1, если кликнули. */
static int textbox(int x, int y, int w, int h, const char *label,
                   const char *buf, int focused) {
    Vector2 ls = MeasureTextEx(g_font, label, 14, 1);
    int lw = (int)ls.x + 6;
    /* подпись */
    DrawTextEx(g_font, label, (Vector2){ (float)x, y + (h - 14)/2 },
               14, 1, LABEL_COLOR);
    /* рамка поля */
    Rectangle r = { (float)(x + lw), (float)y, (float)(w - lw), (float)h };
    DrawRectangleRec(r, INPUT_BG);
    DrawRectangleLinesEx(r, focused ? 2 : 1, focused ? FOCUS_BORDER : INPUT_BORDER);
    /* текст с обрезанием */
    const char *display = buf;
    char truncated[128];
    Vector2 ts = MeasureTextEx(g_font, display, 14, 1);
    if (ts.x > r.width - 8) {
        /* урезаем слева, чтобы показать конец */
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
    /* курсор */
    if (focused && ((int)(GetTime() * 2) % 2 == 0)) {
        float cx = r.x + 4 + ts.x;
        DrawLineV((Vector2){ cx, r.y + 4 }, (Vector2){ cx, r.y + h - 4 },
                  (Color){ 30, 60, 180, 200 });
    }
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/* Группа радио-кнопок. Возвращает новый выбранный индекс. */
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
static const int COL_X[] = { 10, 45, 115, 410, 650, 770, 900 };
static const int COL_W[] = { 35, 70, 295, 240, 120, 130, 300 };

/* Обрезать текст по ширине колонки, добавить ... если не влезает */
static void draw_col_text(int col, int y, int font_size, const char *text, Color color) {
    Vector2 sz = MeasureTextEx(g_font, text, font_size, 1);
    if (sz.x <= COL_W[col] - 6) {
        DrawTextEx(g_font, text, (Vector2){ (float)COL_X[col], y }, font_size, 1, color);
        return;
    }
    /* Обрезаем, пока не влезет с "..." */
    char buf[128];
    int len = (int)strlen(text);
    while (len > 0) {
        int cut = len;
        /* Отступаем на один UTF-8 символ */
        cut--;
        while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) cut--;
        snprintf(buf, sizeof(buf), "%.*s...", cut, text);
        Vector2 ts = MeasureTextEx(g_font, buf, font_size, 1);
        if (ts.x <= COL_W[col] - 6) {
            DrawTextEx(g_font, buf, (Vector2){ (float)COL_X[col], y }, font_size, 1, color);
            return;
        }
        len = cut;
    }
}

/* Заголовок таблицы (фиксированный, не скроллится) */
static void draw_header(void) {
    DrawRectangle(0, HEADER_Y, SCREEN_W, ROW_H, HEADER_BG);
    const char *titles[] = { "#", "Группа", "ФИО", "Специальность",
                             "Форма", "Оценки", "Ср. балл" };
    for (int i = 0; i < 7; i++) {
        Vector2 sz = MeasureTextEx(g_font, titles[i], 16, 1);
        float tx = COL_X[i] + (COL_W[i] - sz.x) / 2;
        DrawTextEx(g_font, titles[i],
                   (Vector2){ tx, HEADER_Y + (ROW_H - 16)/2 },
                   16, 1, HEADER_TEXT);
    }
}

/* Одна строка таблицы */
static void draw_row(int idx, int y, const Student *s) {
    DrawRectangle(0, y, SCREEN_W, ROW_H, (idx % 2 == 0) ? ROW_EVEN : ROW_ODD);

    char buf[7][64];
    snprintf(buf[0], sizeof(buf[0]), "%d", idx + 1);
    snprintf(buf[1], sizeof(buf[1]), "%d", s->group);
    strcpy(buf[2], s->full_name);
    strcpy(buf[3], s->speciality);
    strcpy(buf[4], s->form);
    snprintf(buf[5], sizeof(buf[5]), "%d %d %d %d",
             s->grades[0], s->grades[1], s->grades[2], s->grades[3]);
    snprintf(buf[6], sizeof(buf[6]), "%.2f", s->avg_score);

    Color colors[] = {
        TEXT_COLOR, TEXT_COLOR, TEXT_COLOR, TEXT_COLOR,
        (strcmp(s->form, "бюджетная") == 0) ? TITLE_BUDGET : TITLE_PAID,
        TEXT_COLOR, TEXT_COLOR
    };

    for (int i = 0; i < 7; i++)
        draw_col_text(i, y + (ROW_H - 14) / 2, 14, buf[i], colors[i]);
}

/* Отрисовка полоски-заголовка для секции (бюджет / плат) */
static void draw_section(int y, const char *title, Color bg) {
    DrawRectangle(0, y, SCREEN_W, ROW_H, bg);
    Vector2 sz = MeasureTextEx(g_font, title, 15, 1);
    DrawTextEx(g_font, title,
               (Vector2){ (SCREEN_W - sz.x)/2, y + (ROW_H - 15)/2 },
               15, 1, (Color){ 255, 255, 255, 255 });
}

/* Основная таблица: заголовок + скроллируемые строки */
static void draw_table(void) {
    draw_header();
    BeginScissorMode(0, ROW_Y_START, SCREEN_W, TABLE_H);

    int y = ROW_Y_START - (int)g_scroll;
    int ri = 0;

    if (g_view == VIEW_BY_FORM) {
        /* Ищем границу: все бюджетные идут перед платными */
        int split = 0;
        for (int i = 0; i < g_display_count; i++)
            if (strcmp(g_display[i]->form, "платная") == 0) { split = i; break; }

        /* Бюджетная секция */
        if (split > 0) {
            draw_section(y, "--- БЮДЖЕТНАЯ ФОРМА ---", TITLE_BUDGET);
            y += ROW_H; ri++;
        }
        for (int i = 0; i < split; i++) {
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, g_display[i]);
            y += ROW_H; ri++;
        }

        /* Платная секция */
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
 *  ОБРАБОТКА ВВОДА ТЕКСТА
 * ========================================================== */

/* Добавить один Unicode-символ в UTF-8 строку */
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

/* Удалить последний UTF-8 символ */
static void utf8_pop(char *buf, int *len) {
    if (*len <= 0) return;
    int i = *len - 1;
    /* Идём назад, пока не найдём стартовый байт UTF-8 */
    while (i > 0 && ((unsigned char)buf[i] & 0xC0) == 0x80) i--;
    *len = i;
    buf[*len] = '\0';
}

/* Обработка нажатий клавиш в полях ввода */
static void handle_keys(void) {
    if (g_focus < 0) return;

    int c = GetCharPressed();
    while (c > 0) {
        int printable = (c >= 32 && c <= 126) ||
                        (c >= 0x400 && c <= 0x4FF) || /* кириллица */
                        (c >= 0x500 && c <= 0x52F);   /* дополнения */
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
 *  ВЫЧИСЛЕНИЕ ПОЛОЖЕНИЙ ПОЛЕЙ ВВОДА
 * ========================================================== */

static void calc_textbox_positions(void) {
    /* Поле ФИО: от x=10 */
    int l1 = (int)MeasureTextEx(g_font, "ФИО:", 14, 1).x + 6;
    g_tb_name_x = 10 + l1;
    g_tb_name_w = 300;

    /* Поле Группа: от x = 10 + 300 + 20 = 330 */
    int l2 = (int)MeasureTextEx(g_font, "Группа:", 14, 1).x + 6;
    g_tb_group_x = 330 + l2;
    g_tb_group_w = 100;
}

/* ==========================================================
 *  ГЛАВНАЯ ФУНКЦИЯ
 * ========================================================== */

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H,
               "Ведомость об успеваемости студентов");
    SetTargetFPS(60);

    /* Загружаем шрифт с кириллицей */
    {
        int codepoints[200], cp_count = 0;
        /* ASCII печатные символы */
        for (int i = 32; i < 127; i++) codepoints[cp_count++] = i;
        /* Русские буквы А-Я (0x0410-0x042F), а-я (0x0430-0x044F) */
        for (int i = 0x0410; i <= 0x044F; i++) codepoints[cp_count++] = i;
        /* Ё (0x0401) и ё (0x0451) */
        codepoints[cp_count++] = 0x0401;
        codepoints[cp_count++] = 0x0451;
        g_font = LoadFontEx("C:/Windows/Fonts/arial.ttf", 48, codepoints, cp_count);
        if (g_font.texture.id == 0)
            g_font = GetFontDefault();
    }

    calc_textbox_positions();
    fill_sample_data();
    reset_display();
    g_view = VIEW_ALL;

    while (!WindowShouldClose()) {
        /* ---- Ввод ---- */

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

        /* ---- Определяем клик по полям ввода (до отрисовки) ---- */

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 m = GetMousePosition();
            g_focus = -1;
            Rectangle rn = { (float)g_tb_name_x, (float)SEARCH_Y,
                             (float)g_tb_name_w, (float)BOX_H };
            Rectangle rg = { (float)g_tb_group_x, (float)SEARCH_Y,
                             (float)g_tb_group_w, (float)BOX_H };
            if (CheckCollisionPointRec(m, rn)) g_focus = 0;
            else if (CheckCollisionPointRec(m, rg)) g_focus = 1;
        }

        /* ---- Действия по кнопкам ---- */

        /* Строка 1: кнопки-действия */
        if (btn(10,   BTN_Y, 210, BTN_H, "Сортировка по группам/ФИО", BTN_COLOR, BTN_HOVER))
            action_sort();
        if (btn(230,  BTN_Y, 200, BTN_H, "Отличники (платные)", BTN_COLOR, BTN_HOVER))
            action_excellent_paid();
        if (btn(440,  BTN_Y, 230, BTN_H, "Списки по форме обучения", BTN_COLOR, BTN_HOVER))
            action_by_form();
        if (btn(680,  BTN_Y, 100, BTN_H, "Поиск", BTN_COLOR, BTN_HOVER)) { }

        /* Строка 2: поля поиска */
        textbox(10,   SEARCH_Y, 310, BOX_H, "ФИО:",
                g_search_name, g_focus == 0);
        textbox(330,  SEARCH_Y, 155, BOX_H, "Группа:",
                g_search_group, g_focus == 1);

        const char *form_items[] = { "Все", "Бюджет", "Плат" };
        g_search_form = radio_group(510, SEARCH_Y, BOX_H,
                                     form_items, 3, g_search_form);

        if (btn(700,  SEARCH_Y, 90, BOX_H, "Найти",
                (Color){ 50, 160, 50, 255 }, (Color){ 70, 200, 70, 255 }))
            action_search();
        if (btn(800,  SEARCH_Y, 90, BOX_H, "Сброс",
                (Color){ 160, 60, 60, 255 }, (Color){ 200, 80, 80, 255 }))
            action_reset();

        /* ---- Отрисовка ---- */

        BeginDrawing();
        ClearBackground(BG_COLOR);

        draw_table();

        /* Информационная строка внизу */
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
