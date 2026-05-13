/*
 * ============================================================
 *  Ведомость об успеваемости студентов
 *  Язык C + библиотека Raylib (графический интерфейс)
 *
 *  Структуры:
 *    Student  – узел однонаправленного списка (специальность,
 *               группа, ФИО, форма обучения, оценки, ср. балл)
 *    List     – список (head, tail, count)
 *
 *  Функции:
 *    1. Добавление, редактирование, удаление записей
 *    2. Сортировка по группе, затем по ФИО
 *    3. Фильтр: отличники (оценки 9-10) + платная форма
 *    4. Списки по форме обучения (бюджет / плат),
 *       отсортированные по убыванию среднего балла
 *    5. Поиск по ФИО (частично, без учёта регистра),
 *       номеру группы (точно), форме обучения (точно)
 *    6. Сохранение и загрузка базы в/из CSV-файла
 *       (students.csv, поля разделены ';')
 *    7. Автосохранение при выходе
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

/* Временный буфер ввода имени файла */
#define FILENAME_BUF 256
static char g_temp_filename[FILENAME_BUF] = "";
static int  g_temp_filename_len = 0;
static int  g_waiting_filename = 0;   /* 1 = ожидание ввода имени файла */
/* 0=импорт, 1=экспорт CSV, 2=экспорт TXT */
static int  g_filename_mode = 0;

/* ==========================================================
 *  КОНСТАНТЫ
 * ========================================================== */

#define SCREEN_W    1400
#define SCREEN_H    700
#define SIDE_PANEL_W  160
#define MAIN_X      180   /* сдвиг основного контента вправо */

#define MAX_SPEC    50
#define MAX_NAME    100
#define MAX_FORM    24
#define GRADES      4
#define GRADE_STR   4

#define CSV_FILE    "students.csv"

/* Зоны экрана (в пикселях) */
#define BTN_Y       10
#define BTN_H       36
#define PANEL_Y     55
#define PANEL_H     52
#define INPUT_Y     (PANEL_Y + 6)
#define INPUT_H     34

#define HEADER_Y    (PANEL_Y + PANEL_H + 8)
#define ROW_H       22
#define ROW_Y_START (HEADER_Y + ROW_H + 2)
#define TABLE_H     (SCREEN_H - ROW_Y_START - 28)

/* Цвета интерфейса */
#define BG_COLOR        CLITERAL(Color){ 245, 245, 255, 255 }
#define HEADER_BG       CLITERAL(Color){  60,  60, 120, 255 }
#define HEADER_TEXT     CLITERAL(Color){ 255, 255, 255, 255 }
#define ROW_EVEN        CLITERAL(Color){ 255, 255, 255, 255 }
#define ROW_ODD         CLITERAL(Color){ 235, 240, 250, 255 }
#define ROW_SELECTED    CLITERAL(Color){ 200, 210, 255, 255 }
#define BTN_COLOR       CLITERAL(Color){  70, 100, 180, 255 }
#define BTN_HOVER       CLITERAL(Color){  90, 130, 220, 255 }
#define BTN_ADD         CLITERAL(Color){  50, 150,  50, 255 }
#define BTN_ADD_HOVER   CLITERAL(Color){  70, 190,  70, 255 }
#define BTN_DEL         CLITERAL(Color){ 180,  50,  50, 255 }
#define BTN_DEL_HOVER   CLITERAL(Color){ 220,  70,  70, 255 }
#define BTN_SAVE        CLITERAL(Color){  50, 100, 180, 255 }
#define BTN_SAVE_HOVER  CLITERAL(Color){  70, 130, 220, 255 }
#define BTN_TEXT        CLITERAL(Color){ 255, 255, 255, 255 }
#define INPUT_BG        CLITERAL(Color){ 255, 255, 255, 255 }
#define INPUT_BORDER    CLITERAL(Color){ 100, 100, 100, 255 }
#define FOCUS_BORDER    CLITERAL(Color){  30,  60, 180, 255 }
#define RADIO_ACT       CLITERAL(Color){  70, 100, 180, 255 }
#define RADIO_INACT     CLITERAL(Color){ 180, 180, 180, 255 }
#define TEXT_COLOR      CLITERAL(Color){  20,  20,  20, 255 }
#define LABEL_COLOR     CLITERAL(Color){  50,  50,  80, 255 }
#define TITLE_BUDGET    CLITERAL(Color){   0, 130,  50, 255 }
#define TITLE_PAID      CLITERAL(Color){ 200,  80,  30, 255 }
#define STATUS_OK       CLITERAL(Color){   0, 130,  50, 255 }
#define STATUS_ERR      CLITERAL(Color){ 200,  30,  30, 255 }
#define BTN_CLEAR       CLITERAL(Color){ 190, 150,  40, 255 }
#define BTN_CLEAR_HOVER CLITERAL(Color){ 220, 180,  60, 255 }
#define BTN_UNDO        CLITERAL(Color){ 130,  90, 170, 255 }
#define BTN_UNDO_HOVER  CLITERAL(Color){ 160, 120, 200, 255 }

/* ==========================================================
 *  СТРУКТУРЫ
 * ========================================================== */

/*
 *  Student – узел однонаправленного списка.
 *  Содержит все данные об одном студенте и указатель на
 *  следующий элемент списка (next).
 */
typedef struct Student {
    char  speciality[MAX_SPEC + 1];
    int   group;
    char  full_name[MAX_NAME + 1];
    char  form[MAX_FORM + 1];
    int   grades[GRADES];
    float avg_score;
    struct Student *next;
} Student;

/*
 *  List – структура для управления списком студентов.
 *  head – указатель на первый элемент,
 *  tail – указатель на последний,
 *  count – количество элементов в списке.
 */
typedef struct {
    Student *head;
    Student *tail;
    int count;
} List;

/* ==========================================================
 *  РЕЖИМЫ ПРОСМОТРА
 * ========================================================== */

typedef enum {
    VIEW_ALL,            /* все студенты в исходном порядке */
    VIEW_SORTED,         /* сортировка по группе и ФИО */
    VIEW_EXCELLENT_PAID, /* отличники на платной форме */
    VIEW_BY_FORM,        /* списки по форме обучения */

} ViewMode;

/* ==========================================================
 *  ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
 * ========================================================== */

static List g_list = { NULL, NULL, 0 };      /* основной список */
static List g_undo_list = { NULL, NULL, 0 }; /* резервная копия для отмены */
static Student **g_display = NULL;            /* массив указателей для отображения */
static int g_display_count = 0;               /* количество отображаемых записей */
static ViewMode g_view = VIEW_ALL;            /* текущий режим просмотра */
static float g_scroll = 0.0f;                 /* смещение прокрутки таблицы */
static int g_selected_idx = -1;               /* индекс выбранной строки в g_display */
static int g_editing = 0;                     /* 1 = режим редактирования активен */
static Student g_edit_backup;                 /* резервная копия при входе в режим */

/* Буферы ввода для редактирования / добавления */
static char g_inp_spec[MAX_SPEC + 1] = "";   int g_inp_spec_len = 0;
static char g_inp_group[8] = "";             int g_inp_group_len = 0;
static char g_inp_name[MAX_NAME + 1] = "";   int g_inp_name_len = 0;
static char g_inp_form[MAX_FORM + 1] = "";   int g_inp_form_len = 0;
static char g_inp_grades[GRADES][GRADE_STR] = { "" };
static int  g_inp_grades_len[GRADES] = { 0, 0, 0, 0 };

/* Фильтр по форме: 0 = все, 1 = бюджет, 2 = плат */
static int g_filter_form = 0;

/* Фокус ввода: -1 = нет, 0-3 = поля ввода (спец,группа,ФИО,форма),
   4-7 = оценки */
static int g_focus = -1;

/* Строка состояния */
static char g_status[128] = "";
static double g_status_time = 0.0;

static Font g_font;

/* Предварительные объявления */
static void clear_input_fields(void);
static void apply_filter(void);

/* ==========================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ========================================================== */

/*
 *  calc_avg – вычисление среднего балла по четырём оценкам.
 */
static float calc_avg(const int grades[GRADES]) {
    int sum = 0;
    for (int i = 0; i < GRADES; i++) sum += grades[i];
    return (float)sum / (float)GRADES;
}

/*
 *  set_status – устанавливает текст в строке состояния.
 *  color_flag: 0 = обычный, 1 = успех, 2 = ошибка (для окраски).
 */
static void set_status(const char *text) {
    strncpy(g_status, text, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
    g_status_time = GetTime();
}

/* ==========================================================
 *  ФУНКЦИИ РАБОТЫ СО СПИСКОМ
 * ========================================================== */

/*
 *  newStudent – создаёт новый узел Student.
 *  Принимает все поля студента, выделяет память,
 *  копирует данные, вычисляет средний балл.
 *  Возвращает указатель на созданный узел.
 */
static Student *newStudent(const char *spec, int group,
                           const char *name, const char *form,
                           int g1, int g2, int g3, int g4) {
    Student *s = (Student *)malloc(sizeof(Student));
    if (!s) return NULL;
    strncpy(s->speciality, spec, MAX_SPEC);
    s->speciality[MAX_SPEC] = '\0';
    s->group = group;
    strncpy(s->full_name, name, MAX_NAME);
    s->full_name[MAX_NAME] = '\0';
    strncpy(s->form, form, MAX_FORM);
    s->form[MAX_FORM] = '\0';
    s->grades[0] = g1;
    s->grades[1] = g2;
    s->grades[2] = g3;
    s->grades[3] = g4;
    s->avg_score = calc_avg(s->grades);
    s->next = NULL;
    return s;
}

/*
 *  listPushBack – добавляет узел в конец списка.
 */
static void listPushBack(List *list, Student *s) {
    if (!list->head) {
        list->head = s;
        list->tail = s;
    } else {
        list->tail->next = s;
        list->tail = s;
    }
    list->count++;
}

/*
 *  listRemove – удаляет узел по индексу.
 *  Освобождает память, перелинковывает список.
 *  Возвращает 1 при успехе, 0 если индекс вне диапазона.
 */
static int listRemove(List *list, int index) {
    if (index < 0 || index >= list->count) return 0;
    Student *prev = NULL;
    Student *cur = list->head;
    for (int i = 0; i < index; i++) { prev = cur; cur = cur->next; }
    if (!prev)
        list->head = cur->next;
    else
        prev->next = cur->next;
    if (cur == list->tail)
        list->tail = prev;
    free(cur);
    list->count--;
    return 1;
}

/*
 *  listClear – удаляет все узлы списка, освобождает память.
 */
static void listClear(List *list) {
    Student *cur = list->head;
    while (cur) {
        Student *next = cur->next;
        free(cur);
        cur = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

/*
 *  listFind – возвращает индекс узла в списке по указателю,
 *  или -1 если не найден.
 */
static int listFind(const List *list, const Student *target) {
    Student *cur = list->head;
    for (int i = 0; i < list->count; i++) {
        if (cur == target) return i;
        cur = cur->next;
    }
    return -1;
}

/*
 *  listCopy – глубокое копирование списка.
 */
static void listCopy(List *dst, const List *src) {
    listClear(dst);
    Student *cur = src->head;
    while (cur) {
        Student *s = newStudent(cur->speciality, cur->group, cur->full_name,
                                cur->form, cur->grades[0], cur->grades[1],
                                cur->grades[2], cur->grades[3]);
        if (s) listPushBack(dst, s);
        cur = cur->next;
    }
}

/*
 *  save_undo_state – сохраняет копию g_list в g_undo_list.
 */
static void save_undo_state(void) {
    listCopy(&g_undo_list, &g_list);
}

/* ==========================================================
 *  ФУНКЦИИ РАБОТЫ С CSV
 * ========================================================== */

/*
 *  saveDbCsv – сохраняет базу студентов в CSV-файл.
 *  Формат: специальность;группа;ФИО;форма;оценка1;оценка2;оценка3;оценка4
 *  Каждая запись на отдельной строке.
 */
static void saveDbCsv(const char *filename, const List *list) {
    FILE *f = fopen(filename, "w");
    if (!f) { set_status("Ошибка: не удалось открыть файл для записи"); return; }
    Student *cur = list->head;
    while (cur) {
        fprintf(f, "%s;%d;%s;%s;%d;%d;%d;%d\n",
                cur->speciality, cur->group, cur->full_name, cur->form,
                cur->grades[0], cur->grades[1], cur->grades[2], cur->grades[3]);
        cur = cur->next;
    }
    fclose(f);
    set_status("База сохранена в students.csv");
}

/*
 *  loadDbCsv – загружает базу студентов из CSV-файла.
 *  Парсит строки, разделённые ';', создаёт узлы.
 *  Вычисляет средний балл для каждой записи.
 */
static void loadDbCsv(const char *filename, List *list) {
    FILE *f = fopen(filename, "r");
    if (!f) { set_status("Файл students.csv не найден, создан пустой список"); return; }
    char line[512];
    int loaded = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (len == 0) continue;

        char *spec = line;
        char *p = strchr(spec, ';');  if (!p) continue; *p++ = '\0';
        char *grp_s = p;
        p = strchr(grp_s, ';');       if (!p) continue; *p++ = '\0';
        char *name = p;
        p = strchr(name, ';');        if (!p) continue; *p++ = '\0';
        char *form = p;
        p = strchr(form, ';');        if (!p) continue; *p++ = '\0';
        int g1 = atoi(p);
        p = strchr(p, ';');           if (!p) continue; *p++ = '\0';
        int g2 = atoi(p);
        p = strchr(p, ';');           if (!p) continue; *p++ = '\0';
        int g3 = atoi(p);
        p = strchr(p, ';');           if (!p) continue; *p++ = '\0';
        int g4 = atoi(p);

        int group = atoi(grp_s);
        Student *s = newStudent(spec, group, name, form, g1, g2, g3, g4);
        if (s) { listPushBack(list, s); loaded++; }
    }
    fclose(f);
    char msg[64];
    snprintf(msg, sizeof(msg), "Загружено %d записей из students.csv", loaded);
    set_status(msg);
}

/*
 *  import_csv – импортирует студентов из произвольного CSV-файла.
 *  Добавляет записи в конец текущего списка (не заменяя).
 *  Перед импортом сохраняет undo-состояние.
 */
static void import_csv(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) { set_status("Ошибка: не удалось открыть файл для импорта"); return; }
    save_undo_state();
    char line[512];
    int imported = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (len == 0) continue;

        char *spec = line;
        char *p = strchr(spec, ';');  if (!p) continue; *p++ = '\0';
        char *grp_s = p;
        p = strchr(grp_s, ';');       if (!p) continue; *p++ = '\0';
        char *name = p;
        p = strchr(name, ';');        if (!p) continue; *p++ = '\0';
        char *form = p;
        p = strchr(form, ';');        if (!p) continue; *p++ = '\0';
        int g1 = atoi(p);
        p = strchr(p, ';');           if (!p) continue; *p++ = '\0';
        int g2 = atoi(p);
        p = strchr(p, ';');           if (!p) continue; *p++ = '\0';
        int g3 = atoi(p);
        p = strchr(p, ';');           if (!p) continue; *p++ = '\0';
        int g4 = atoi(p);

        int group = atoi(grp_s);
        Student *s = newStudent(spec, group, name, form, g1, g2, g3, g4);
        if (s) { listPushBack(&g_list, s); imported++; }
    }
    fclose(f);
    clear_input_fields();
    g_selected_idx = -1;
    g_focus = -1;
    apply_filter();
    char msg[64];
    snprintf(msg, sizeof(msg), "Импортировано %d записей из %s", imported, filename);
    set_status(msg);
}

/*
 *  export_csv – экспортирует текущий отображаемый список
 *  (g_display) в CSV-файл. Формат: специальность;группа;ФИО;форма;оценка1;…;оценка4.
 */
static void export_csv(const char *filename, Student **display, int count) {
    FILE *f = fopen(filename, "w");
    if (!f) { set_status("Ошибка: не удалось создать файл для экспорта"); return; }
    for (int i = 0; i < count; i++) {
        Student *s = display[i];
        fprintf(f, "%s;%d;%s;%s;%d;%d;%d;%d\n",
                s->speciality, s->group, s->full_name, s->form,
                s->grades[0], s->grades[1], s->grades[2], s->grades[3]);
    }
    fclose(f);
    char msg[64];
    snprintf(msg, sizeof(msg), "Экспортировано %d записей в %s", count, filename);
    set_status(msg);
}

/*
 *  export_txt – экспортирует текущий отображаемый список
 *  в текстовый файл с форматированием (колонки, заголовки).
 */
static void export_txt(const char *filename, Student **display, int count) {
    FILE *f = fopen(filename, "w");
    if (!f) { set_status("Ошибка: не удалось создать файл для экспорта"); return; }
    fprintf(f, "%-4s %-8s %-30s %-30s %-12s %-16s %s\n",
            "№", "Группа", "ФИО", "Специальность", "Форма", "Оценки", "Ср.балл");
    for (int i = 0; i < 101; i++) fputc('-', f);
    fputc('\n', f);
    for (int i = 0; i < count; i++) {
        Student *s = display[i];
        char grades[32];
        snprintf(grades, sizeof(grades), "%d %d %d %d",
                 s->grades[0], s->grades[1], s->grades[2], s->grades[3]);
        fprintf(f, "%-4d %-8d %-30s %-30s %-12s %-16s %.2f\n",
                i + 1, s->group, s->full_name, s->speciality, s->form,
                grades, s->avg_score);
    }
    fclose(f);
    char msg[64];
    snprintf(msg, sizeof(msg), "Отчёт экспортирован в %s", filename);
    set_status(msg);
}

/* ==========================================================
 *  УПРАВЛЕНИЕ ОТОБРАЖЕНИЕМ
 * ========================================================== */

/*
 *  rebuild_display – перестраивает массив g_display
 *  из текущего списка g_list. Освобождает старый массив,
 *  выделяет новый, заполняет указателями.
 *  Сбрасывает режим на VIEW_ALL.
 */
static void rebuild_display(void) {
    if (g_display) { free(g_display); g_display = NULL; }
    g_display_count = g_list.count;
    if (g_display_count > 0) {
        g_display = (Student **)malloc(g_display_count * sizeof(Student *));
        Student *cur = g_list.head;
        for (int i = 0; i < g_display_count; i++) {
            g_display[i] = cur;
            cur = cur->next;
        }
    }
    g_view = VIEW_ALL;
    g_scroll = 0.0f;
}

/* ==========================================================
 *  ФУНКЦИИ СРАВНЕНИЯ (для qsort)
 * ========================================================== */

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

/*
 *  str_icontains – проверяет, содержит ли строка str
 *  подстроку substr (без учёта регистра).
 */
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

/* ==========================================================
 *  ДЕЙСТВИЯ
 * ========================================================== */

/*
 *  action_sort – сортирует отображаемые записи
 *  по номеру группы и ФИО.
 */
static void action_sort(void) {
    rebuild_display();
    if (g_display_count > 0)
        qsort(g_display, g_display_count, sizeof(Student *), cmp_group_name);
    g_view = VIEW_SORTED;
    g_scroll = 0.0f;
    g_selected_idx = -1;
}

/*
 *  action_excellent_paid – фильтр: отличники (все оценки >= 9)
 *  с платной формой обучения.
 */
static void action_excellent_paid(void) {
    rebuild_display();
    if (g_display) free(g_display);
    g_display = NULL;
    g_display_count = 0;

    Student *cur = g_list.head;
    while (cur) {
        int excellent = 1;
        for (int k = 0; k < GRADES; k++)
            if (cur->grades[k] < 9) { excellent = 0; break; }
        if (excellent && strcmp(cur->form, "платная") == 0)
            g_display_count++;
        cur = cur->next;
    }

    if (g_display_count > 0) {
        g_display = (Student **)malloc(g_display_count * sizeof(Student *));
        int idx = 0;
        cur = g_list.head;
        while (cur) {
            int excellent = 1;
            for (int k = 0; k < GRADES; k++)
                if (cur->grades[k] < 9) { excellent = 0; break; }
            if (excellent && strcmp(cur->form, "платная") == 0)
                g_display[idx++] = cur;
            cur = cur->next;
        }
    }
    g_view = VIEW_EXCELLENT_PAID;
    g_scroll = 0.0f;
    g_selected_idx = -1;
}

/*
 *  action_by_form – формирует два списка (бюджет / плат),
 *  отсортированных по убыванию среднего балла.
 */
static void action_by_form(void) {
    int bc = 0, pc = 0;
    Student *cur = g_list.head;
    while (cur) {
        if (strcmp(cur->form, "бюджетная") == 0) bc++;
        else pc++;
        cur = cur->next;
    }

    Student **budget = bc > 0 ? (Student **)malloc(bc * sizeof(Student *)) : NULL;
    Student **paid   = pc > 0 ? (Student **)malloc(pc * sizeof(Student *)) : NULL;
    bc = 0; pc = 0;
    cur = g_list.head;
    while (cur) {
        if (strcmp(cur->form, "бюджетная") == 0)
            budget[bc++] = cur;
        else
            paid[pc++] = cur;
        cur = cur->next;
    }

    if (budget) qsort(budget, bc, sizeof(Student *), cmp_avg_desc);
    if (paid)   qsort(paid,   pc, sizeof(Student *), cmp_avg_desc);

    if (g_display) free(g_display);
    g_display_count = bc + pc;
    g_display = (Student **)malloc(g_display_count * sizeof(Student *));
    int idx = 0;
    for (int i = 0; i < bc; i++) g_display[idx++] = budget[i];
    for (int i = 0; i < pc; i++) g_display[idx++] = paid[i];

    free(budget);
    free(paid);

    g_view = VIEW_BY_FORM;
    g_scroll = 0.0f;
    g_selected_idx = -1;
}

/*
 *  matches_filter – проверяет, проходит ли студент через текущий
 *  фильтр по полям ввода и форме обучения.
 */
static int matches_filter(const Student *s) {
    if (g_inp_spec_len > 0 && !str_icontains(s->speciality, g_inp_spec))
        return 0;
    if (g_inp_group_len > 0) {
        int fg = atoi(g_inp_group);
        if (fg > 0 && s->group != fg) return 0;
    }
    if (g_inp_name_len > 0 && !str_icontains(s->full_name, g_inp_name))
        return 0;
    if (g_inp_form_len > 0 && strcmp(s->form, g_inp_form) != 0)
        return 0;
    if (g_filter_form == 1 && strcmp(s->form, "бюджетная") != 0)
        return 0;
    if (g_filter_form == 2 && strcmp(s->form, "платная") != 0)
        return 0;
    return 1;
}

/*
 *  apply_filter – перестраивает g_display по текущему фильтру
 *  из полей ввода и радиокнопок.
 */
static void apply_filter(void) {
    if (g_display) { free(g_display); g_display = NULL; }
    g_display_count = 0;
    g_selected_idx = -1;

    Student *cur = g_list.head;
    while (cur) {
        if (matches_filter(cur)) g_display_count++;
        cur = cur->next;
    }

    if (g_display_count > 0) {
        g_display = (Student **)malloc(g_display_count * sizeof(Student *));
        int idx = 0;
        cur = g_list.head;
        while (cur) {
            if (matches_filter(cur)) g_display[idx++] = cur;
            cur = cur->next;
        }
    }

    g_view = VIEW_ALL;
    g_scroll = 0.0f;
}

/*
 *  clear_input_fields – очищает все поля ввода.
 */
static void clear_input_fields(void) {
    g_inp_spec[0] = '\0';  g_inp_spec_len = 0;
    g_inp_group[0] = '\0'; g_inp_group_len = 0;
    g_inp_name[0] = '\0';  g_inp_name_len = 0;
    g_inp_form[0] = '\0';  g_inp_form_len = 0;
    for (int i = 0; i < GRADES; i++) {
        g_inp_grades[i][0] = '\0';
        g_inp_grades_len[i] = 0;
    }
}

/*
 *  fill_input_from_student – заполняет поля ввода
 *  данными из выбранного студента (для редактирования).
 */
static void fill_input_from_student(const Student *s) {
    strncpy(g_inp_spec, s->speciality, MAX_SPEC);
    g_inp_spec[MAX_SPEC] = '\0';
    g_inp_spec_len = (int)strlen(g_inp_spec);

    snprintf(g_inp_group, 8, "%d", s->group);
    g_inp_group_len = (int)strlen(g_inp_group);

    strncpy(g_inp_name, s->full_name, MAX_NAME);
    g_inp_name[MAX_NAME] = '\0';
    g_inp_name_len = (int)strlen(g_inp_name);

    strncpy(g_inp_form, s->form, MAX_FORM);
    g_inp_form[MAX_FORM] = '\0';
    g_inp_form_len = (int)strlen(g_inp_form);

    for (int i = 0; i < GRADES; i++) {
        snprintf(g_inp_grades[i], GRADE_STR, "%d", s->grades[i]);
        g_inp_grades_len[i] = (int)strlen(g_inp_grades[i]);
    }
}

/*
 *  validate_input – проверяет корректность полей ввода.
 *  Возвращает 1 если данные корректны, иначе 0.
 */
static int validate_input(void) {
    if (g_inp_spec_len == 0) { set_status("Ошибка: введите специальность"); return 0; }
    if (g_inp_name_len == 0) { set_status("Ошибка: введите ФИО"); return 0; }
    if (g_inp_group_len == 0 || atoi(g_inp_group) <= 0) {
        set_status("Ошибка: группа должна быть положительным числом"); return 0;
    }
    if (g_inp_form_len == 0) { set_status("Ошибка: введите форму обучения"); return 0; }
    if (strcmp(g_inp_form, "бюджетная") != 0 && strcmp(g_inp_form, "платная") != 0) {
        set_status("Ошибка: форма обучения — 'бюджетная' или 'платная'"); return 0;
    }
    for (int i = 0; i < GRADES; i++) {
        int v = atoi(g_inp_grades[i]);
        if (v < 1 || v > 10) {
            set_status("Ошибка: оценки должны быть от 1 до 10"); return 0;
        }
    }
    return 1;
}

/*
 *  action_add – добавляет нового студента в список.
 *  Считывает данные из полей ввода, создаёт узел,
 *  добавляет в конец списка, обновляет отображение.
 */
static void action_add(void) {
    if (!validate_input()) return;
    save_undo_state();
    int group = atoi(g_inp_group);
    int g1 = atoi(g_inp_grades[0]);
    int g2 = atoi(g_inp_grades[1]);
    int g3 = atoi(g_inp_grades[2]);
    int g4 = atoi(g_inp_grades[3]);

    Student *s = newStudent(g_inp_spec, group, g_inp_name, g_inp_form,
                            g1, g2, g3, g4);
    if (!s) { set_status("Ошибка: не удалось выделить память"); return; }
    listPushBack(&g_list, s);
    clear_input_fields();
    g_focus = -1;
    apply_filter();
    set_status("Студент добавлен");
}

/*
 *  copy_student_data – копирует данные из src в dst (без next).
 */
static void copy_student_data(Student *dst, const Student *src) {
    strncpy(dst->speciality, src->speciality, MAX_SPEC);
    dst->speciality[MAX_SPEC] = '\0';
    dst->group = src->group;
    strncpy(dst->full_name, src->full_name, MAX_NAME);
    dst->full_name[MAX_NAME] = '\0';
    strncpy(dst->form, src->form, MAX_FORM);
    dst->form[MAX_FORM] = '\0';
    for (int i = 0; i < GRADES; i++) dst->grades[i] = src->grades[i];
    dst->avg_score = src->avg_score;
}

/*
 *  action_edit_mode – входит в режим редактирования.
 *  Сохраняет резервную копию и заполняет поля ввода.
 */
static void action_edit_mode(void) {
    if (g_selected_idx < 0 || g_selected_idx >= g_display_count) {
        set_status("Ошибка: выберите студента из таблицы");
        return;
    }
    copy_student_data(&g_edit_backup, g_display[g_selected_idx]);
    fill_input_from_student(g_display[g_selected_idx]);
    g_editing = 1;
    g_focus = 0;
    set_status("Режим редактирования — изменения видны в таблице");
}

/*
 *  action_confirm_edit – сохраняет изменения из полей ввода
 *  в выбранного студента и выходит из режима редактирования.
 */
static void action_confirm_edit(void) {
    if (!g_editing) return;
    if (!validate_input()) return;
    save_undo_state();

    Student *s = g_display[g_selected_idx];
    strncpy(s->speciality, g_inp_spec, MAX_SPEC);
    s->speciality[MAX_SPEC] = '\0';
    s->group = atoi(g_inp_group);
    strncpy(s->full_name, g_inp_name, MAX_NAME);
    s->full_name[MAX_NAME] = '\0';
    strncpy(s->form, g_inp_form, MAX_FORM);
    s->form[MAX_FORM] = '\0';
    s->grades[0] = atoi(g_inp_grades[0]);
    s->grades[1] = atoi(g_inp_grades[1]);
    s->grades[2] = atoi(g_inp_grades[2]);
    s->grades[3] = atoi(g_inp_grades[3]);
    s->avg_score = calc_avg(s->grades);

    g_editing = 0;
    g_focus = -1;
    apply_filter();
    for (int i = 0; i < g_display_count; i++) {
        if (g_display[i] == s) { g_selected_idx = i; break; }
    }
    set_status("Изменения сохранены");
}

/*
 *  action_cancel_edit – отменяет редактирование, восстанавливая
 *  исходные данные студента.
 */
static void action_cancel_edit(void) {
    if (!g_editing) return;
    if (g_selected_idx >= 0 && g_selected_idx < g_display_count) {
        Student *s = g_display[g_selected_idx];
        copy_student_data(s, &g_edit_backup);
    }
    g_editing = 0;
    g_focus = -1;
    clear_input_fields();
    g_filter_form = 0;
    apply_filter();
    set_status("Редактирование отменено");
}

/*
 *  action_delete – удаляет выбранного студента из списка.
 */
static void action_delete(void) {
    if (g_selected_idx < 0 || g_selected_idx >= g_display_count) {
        set_status("Ошибка: выберите студента из таблицы");
        return;
    }
    if (g_editing) {
        if (g_selected_idx >= 0) copy_student_data(g_display[g_selected_idx], &g_edit_backup);
        g_editing = 0;
        g_focus = -1;
    }
    Student *target = g_display[g_selected_idx];
    int list_idx = listFind(&g_list, target);
    if (list_idx < 0) { set_status("Ошибка: студент не найден в списке"); return; }
    save_undo_state();
    listRemove(&g_list, list_idx);
    clear_input_fields();
    g_selected_idx = -1;
    g_focus = -1;
    apply_filter();
    set_status("Студент удалён");
}

/*
 *  action_clear – очищает поля ввода и сбрасывает фильтр.
 */
static void action_clear(void) {
    clear_input_fields();
    g_selected_idx = -1;
    g_focus = -1;
    g_filter_form = 0;
    apply_filter();
    set_status("Фильтр сброшен");
}

/*
 *  action_undo – отменяет последнее действие (добавление,
 *  редактирование, удаление, загрузку).
 */
static void action_undo(void) {
    if (g_undo_list.count == 0) {
        set_status("Нет действий для отмены");
        return;
    }
    listClear(&g_list);
    Student *cur = g_undo_list.head;
    while (cur) {
        Student *s = newStudent(cur->speciality, cur->group, cur->full_name,
                                cur->form, cur->grades[0], cur->grades[1],
                                cur->grades[2], cur->grades[3]);
        if (s) listPushBack(&g_list, s);
        cur = cur->next;
    }
    listClear(&g_undo_list);
    clear_input_fields();
    g_selected_idx = -1;
    g_focus = -1;
    g_filter_form = 0;
    apply_filter();
    set_status("Действие отменено");
}

/* ==========================================================
 *  UTF-8 ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ========================================================== */

/*
 *  utf8_append – добавляет Unicode-символ (codepoint)
 *  в UTF-8 буфер.
 */
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

/*
 *  utf8_pop – удаляет последний UTF-8 символ из буфера.
 */
static void utf8_pop(char *buf, int *len) {
    if (*len <= 0) return;
    int i = *len - 1;
    while (i > 0 && ((unsigned char)buf[i] & 0xC0) == 0x80) i--;
    *len = i;
    buf[*len] = '\0';
}

/* ==========================================================
 *  ОТРИСОВКА ЭЛЕМЕНТОВ
 * ========================================================== */

/*
 *  btn – рисует кнопку с закруглёнными углами.
 *  Возвращает 1 при нажатии.
 */
static int btn(int x, int y, int w, int h, const char *text,
               Color col, Color hover) {
    Rectangle r = { (float)x, (float)y, (float)w, (float)h };
    Vector2 m = GetMousePosition();
    Color c = CheckCollisionPointRec(m, r) ? hover : col;
    DrawRectangleRounded(r, 0.15f, 6, c);
    DrawRectangleRoundedLines(r, 0.15f, 6, (Color){ 0, 0, 0, 30 });
    Vector2 sz = MeasureTextEx(g_font, text, 16, 1);
    DrawTextEx(g_font, text,
               (Vector2){ r.x + (w - sz.x) / 2, r.y + (h - sz.y) / 2 },
               16, 1, BTN_TEXT);
    return CheckCollisionPointRec(m, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/*
 *  textbox – рисует поле ввода с меткой.
 *  Показывает текст и курсор, если поле в фокусе.
 *  Возвращает 1 при клике на поле.
 */
static int textbox(int x, int y, int w, int h, const char *label,
                   const char *buf, int focused) {
    Vector2 ls = MeasureTextEx(g_font, label, 14, 1);
    int lw = (int)ls.x + 6;
    DrawTextEx(g_font, label, (Vector2){ (float)x, y + (h - 14) / 2 },
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
               (Vector2){ r.x + 4, y + (h - 14) / 2 }, 14, 1, TEXT_COLOR);
    if (focused && ((int)(GetTime() * 2) % 2 == 0)) {
        float cx = r.x + 4 + ts.x;
        DrawLineV((Vector2){ cx, r.y + 4 }, (Vector2){ cx, r.y + h - 4 },
                  (Color){ 30, 60, 180, 200 });
    }
    return CheckCollisionPointRec(GetMousePosition(), r) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/*
 *  radio_group – рисует группу радиокнопок.
 *  Возвращает индекс выбранного элемента.
 */
static int radio_group(int x, int y, int h, const char *items[],
                       int count, int selected) {
    int cx = x;
    for (int i = 0; i < count; i++) {
        int tw = (int)MeasureTextEx(g_font, items[i], 14, 1).x;
        int bw = tw + 22;
        Rectangle r = { (float)cx, (float)y, (float)bw, (float)h };
        Color c = (i == selected) ? RADIO_ACT : RADIO_INACT;
        DrawCircle(cx + 10, y + h / 2, 7, c);
        if (i == selected)
            DrawCircle(cx + 10, y + h / 2, 3, (Color){ 255, 255, 255, 255 });
        DrawTextEx(g_font, items[i],
                   (Vector2){ (float)(cx + 20), y + (h - 14) / 2 },
                   14, 1, LABEL_COLOR);
        if (CheckCollisionPointRec(GetMousePosition(), r) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            selected = i;
        cx += bw + 4;
    }
    return selected;
}

/* Позиции колонок таблицы (сдвинуты на MAIN_X) */
static const int COL_X[] = { 10+MAIN_X, 45+MAIN_X, 115+MAIN_X,
                             410+MAIN_X, 690+MAIN_X, 840+MAIN_X, 970+MAIN_X };
static const int COL_W[] = { 35, 70, 295, 280, 150, 130, 230 };

/*
 *  draw_col_text – рисует текст в колонке таблицы.
 *  При необходимости обрезает текст с добавлением "...",
 *  корректно обрабатывая UTF-8 границы.
 */
static void draw_col_text(int col, int y, int font_sz,
                          const char *text, Color color) {
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

/*
 *  draw_header – рисует заголовок таблицы.
 */
static void draw_header(void) {
    DrawRectangle(0, HEADER_Y, SCREEN_W, ROW_H, HEADER_BG);
    const char *titles[] = {
        "#", "Группа", "ФИО", "Специальность",
        "Форма", "Оценки", "Ср. балл"
    };
    for (int i = 0; i < 7; i++)
        DrawTextEx(g_font, titles[i],
                   (Vector2){ (float)COL_X[i], HEADER_Y + (ROW_H - 16) / 2 },
                   16, 1, HEADER_TEXT);
}

/*
 *  draw_row – рисует одну строку таблицы.
 */
static void draw_row(int idx, int y, const Student *s, int selected) {
    DrawRectangle(0, y, SCREEN_W, ROW_H,
                  selected ? ROW_SELECTED : ((idx % 2 == 0) ? ROW_EVEN : ROW_ODD));
    char b[7][64];
    snprintf(b[0], sizeof(b[0]), "%d", idx + 1);
    if (g_editing && selected) {
        snprintf(b[1], sizeof(b[1]), "%s", g_inp_group);
        strcpy(b[2], g_inp_name);
        strcpy(b[3], g_inp_spec);
        strcpy(b[4], g_inp_form);
        int gv[GRADES];
        for (int i = 0; i < GRADES; i++) gv[i] = atoi(g_inp_grades[i]);
        snprintf(b[5], sizeof(b[5]), "%d %d %d %d", gv[0], gv[1], gv[2], gv[3]);
        float avg = (g_inp_grades_len[0] && g_inp_grades_len[1] &&
                     g_inp_grades_len[2] && g_inp_grades_len[3])
                    ? (gv[0]+gv[1]+gv[2]+gv[3])/4.0f : s->avg_score;
        snprintf(b[6], sizeof(b[6]), "%.2f", avg);
    } else {
        snprintf(b[1], sizeof(b[1]), "%d", s->group);
        strcpy(b[2], s->full_name);
        strcpy(b[3], s->speciality);
        strcpy(b[4], s->form);
        snprintf(b[5], sizeof(b[5]), "%d %d %d %d",
                 s->grades[0], s->grades[1], s->grades[2], s->grades[3]);
        snprintf(b[6], sizeof(b[6]), "%.2f", s->avg_score);
    }
    Color cc[] = {
        TEXT_COLOR, TEXT_COLOR, TEXT_COLOR, TEXT_COLOR,
        (strcmp(s->form, "бюджетная") == 0) ? TITLE_BUDGET : TITLE_PAID,
        TEXT_COLOR, TEXT_COLOR
    };
    for (int i = 0; i < 7; i++)
        draw_col_text(i, y + (ROW_H - 14) / 2, 14, b[i], cc[i]);
}

/*
 *  draw_section – рисует полоску-заголовок секции.
 */
static void draw_section(int y, const char *title, Color bg) {
    DrawRectangle(0, y, SCREEN_W, ROW_H, bg);
    Vector2 sz = MeasureTextEx(g_font, title, 15, 1);
    DrawTextEx(g_font, title,
               (Vector2){ (SCREEN_W - sz.x) / 2, y + (ROW_H - 15) / 2 },
               15, 1, (Color){ 255, 255, 255, 255 });
}

/*
 *  draw_table – рисует таблицу студентов с прокруткой.
 */
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
                draw_row(ri, y, g_display[i], i == g_selected_idx);
            y += ROW_H; ri++;
        }
        if (split < g_display_count) {
            draw_section(y, "--- ПЛАТНАЯ ФОРМА ---", TITLE_PAID);
            y += ROW_H; ri++;
        }
        for (int i = split; i < g_display_count; i++) {
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, g_display[i], i == g_selected_idx);
            y += ROW_H; ri++;
        }
    } else {
        for (int i = 0; i < g_display_count; i++) {
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, g_display[i], i == g_selected_idx);
            y += ROW_H; ri++;
        }
    }
    EndScissorMode();
}

/* ==========================================================
 *  ОБРАБОТКА ВВОДА
 * ========================================================== */

/*
 *  handle_keys – обрабатывает ввод символов и backspace
 *  для активного поля ввода (g_focus). Поддерживает
 *  кириллицу и служебные клавиши.
 */
static void handle_keys(void) {
    /* Режим ввода имени файла */
    if (g_waiting_filename) {
        int c = GetCharPressed();
        while (c > 0) {
            if (c >= 32 && c <= 126 && g_temp_filename_len < FILENAME_BUF - 1) {
                g_temp_filename[g_temp_filename_len++] = (char)c;
                g_temp_filename[g_temp_filename_len] = '\0';
            }
            c = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && g_temp_filename_len > 0) {
            g_temp_filename[--g_temp_filename_len] = '\0';
        }
        if (IsKeyPressed(KEY_ENTER) && g_temp_filename_len > 0) {
            if (g_filename_mode == 0) import_csv(g_temp_filename);
            else if (g_filename_mode == 1) export_csv(g_temp_filename, g_display, g_display_count);
            else export_txt(g_temp_filename, g_display, g_display_count);
            g_waiting_filename = 0;
            g_temp_filename[0] = '\0';
            g_temp_filename_len = 0;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            g_waiting_filename = 0;
            g_temp_filename[0] = '\0';
            g_temp_filename_len = 0;
        }
        return;
    }

    if (g_focus < 0) return;
    int changed = 0;
    int c = GetCharPressed();
    while (c > 0) {
        int printable = (c >= 32 && c <= 126) ||
                        (c >= 0x400 && c <= 0x4FF) ||
                        (c >= 0x500 && c <= 0x52F);
        if (printable) {
            if (g_focus == 0)
                { utf8_append(g_inp_spec, &g_inp_spec_len, MAX_SPEC, c); changed = 1; }
            else if (g_focus == 1 && c >= '0' && c <= '9' && g_inp_group_len < 6)
                { utf8_append(g_inp_group, &g_inp_group_len, 6, c); changed = 1; }
            else if (g_focus == 2)
                { utf8_append(g_inp_name, &g_inp_name_len, MAX_NAME, c); changed = 1; }
            else if (g_focus == 3)
                { utf8_append(g_inp_form, &g_inp_form_len, MAX_FORM, c); changed = 1; }
            else if (g_focus >= 4 && g_focus <= 7) {
                int gi = g_focus - 4;
                if (c >= '0' && c <= '9' && g_inp_grades_len[gi] < 2)
                    { utf8_append(g_inp_grades[gi], &g_inp_grades_len[gi], 2, c); changed = 1; }
            }
        }
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (g_focus == 0) { utf8_pop(g_inp_spec, &g_inp_spec_len); changed = 1; }
        else if (g_focus == 1) { utf8_pop(g_inp_group, &g_inp_group_len); changed = 1; }
        else if (g_focus == 2) { utf8_pop(g_inp_name, &g_inp_name_len); changed = 1; }
        else if (g_focus == 3) { utf8_pop(g_inp_form, &g_inp_form_len); changed = 1; }
        else if (g_focus >= 4 && g_focus <= 7)
            { utf8_pop(g_inp_grades[g_focus - 4], &g_inp_grades_len[g_focus - 4]); changed = 1; }
    }
    if (changed && !g_editing) apply_filter();
}

/*
 *  get_row_at_click – определяет индекс строки таблицы
 *  по координате Y мыши с учётом прокрутки.
 *  Возвращает -1 если клик вне области строк.
 */
static int get_row_at_click(int mouse_y) {
    if (mouse_y < ROW_Y_START || mouse_y >= ROW_Y_START + TABLE_H)
        return -1;
    int idx = (mouse_y - ROW_Y_START + (int)g_scroll) / ROW_H;
    /* Учитываем заголовки секций в VIEW_BY_FORM */
    if (g_view == VIEW_BY_FORM) {
        int split = 0;
        for (int i = 0; i < g_display_count; i++)
            if (strcmp(g_display[i]->form, "платная") == 0) { split = i; break; }
        int visual_idx = idx;
        if (split > 0) visual_idx--;
        if (visual_idx < 0) return -1;
        if (visual_idx < split) return visual_idx;
        visual_idx--;
        if (visual_idx >= 0 && visual_idx < g_display_count - split)
            return split + visual_idx;
        return -1;
    }
    return (idx >= 0 && idx < g_display_count) ? idx : -1;
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

    /* Загружаем базу из CSV или создаём тестовые данные */
    {
        FILE *test = fopen(CSV_FILE, "r");
        if (test) {
            fclose(test);
            loadDbCsv(CSV_FILE, &g_list);
        } else {
            struct {
                const char *spec; int grp; const char *name; const char *form;
                int g1, g2, g3, g4;
            } data[] = {
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
            for (int i = 0; i < 10; i++) {
                Student *s = newStudent(data[i].spec, data[i].grp, data[i].name,
                                        data[i].form, data[i].g1, data[i].g2,
                                        data[i].g3, data[i].g4);
                if (s) listPushBack(&g_list, s);
            }
            set_status("Загружены тестовые данные (10 студентов)");
            saveDbCsv(CSV_FILE, &g_list);
        }
    }
    rebuild_display();

    while (!WindowShouldClose()) {
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

        /* Обработка кликов */
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 m = GetMousePosition();

            /* Поля редактирования (8 шт.) */
            int inp_x[] = { MAIN_X+10, MAIN_X+260, MAIN_X+380, MAIN_X+670, MAIN_X+808, MAIN_X+858, MAIN_X+908, MAIN_X+958 };
            int inp_w[] = { 240, 110, 280, 130, 46, 46, 46, 46 };
            g_focus = -1;
            for (int i = 0; i < 8; i++) {
                Rectangle r = { (float)inp_x[i], INPUT_Y, (float)inp_w[i], INPUT_H };
                if (CheckCollisionPointRec(m, r)) { g_focus = i; break; }
            }

            /* Клик по строке таблицы */
            if (g_focus < 0) {
                int ri = get_row_at_click((int)m.y);
                if (ri >= 0 && ri < g_display_count) {
                    if (g_editing) action_confirm_edit();
                    g_selected_idx = ri;
                    fill_input_from_student(g_display[ri]);
                }
            }

            /* Отмена выделения при клике в пустую область таблицы */
            if (g_focus < 0 && m.y >= ROW_Y_START && m.y < ROW_Y_START + TABLE_H) {
                int ri = get_row_at_click((int)m.y);
                if (ri < 0 || ri >= g_display_count) g_selected_idx = -1;
            }
        }

        /* ====================================================
         *  ОТРИСОВКА
         * ==================================================== */

        BeginDrawing();
        ClearBackground(BG_COLOR);

        /* Фон панели редактирования (если не режим ввода имени) */
        if (!g_waiting_filename) {
            Rectangle pr = { (float)MAIN_X+8, (float)PANEL_Y, SCREEN_W - MAIN_X - 16, PANEL_H };
            DrawRectangleRounded(pr, 0.08f, 6, (Color){ 235, 235, 250, 255 });
            DrawRectangleRoundedLines(pr, 0.08f, 6, (Color){ 200, 200, 225, 255 });
        }

        /* Кнопки действий (верхняя панель) */
        if (btn(MAIN_X+10, BTN_Y, 100, BTN_H, "Добавить",  BTN_ADD, BTN_ADD_HOVER))
            { if (g_editing) action_confirm_edit(); action_add(); }
        if (g_editing) {
            if (btn(MAIN_X+120, BTN_Y, 80, BTN_H, "Готово", (Color){50,150,50,255}, (Color){70,190,70,255}))
                action_confirm_edit();
            if (btn(MAIN_X+208, BTN_Y, 36, BTN_H, "X",   (Color){180,50,50,255}, (Color){220,70,70,255}))
                action_cancel_edit();
        } else {
            if (btn(MAIN_X+120, BTN_Y, 100, BTN_H, "Изменить", BTN_COLOR, BTN_HOVER))
                action_edit_mode();
        }
        {   int dx = g_editing ? 22 : 0;
            if (btn(MAIN_X+230+dx, BTN_Y, 90,  BTN_H, "Удалить",   BTN_DEL, BTN_DEL_HOVER))
                action_delete();
            if (btn(MAIN_X+330+dx, BTN_Y, 80,  BTN_H, "Сорт.",     BTN_COLOR, BTN_HOVER))
                action_sort();
            if (btn(MAIN_X+420+dx, BTN_Y, 80,  BTN_H, "Отл.+",     BTN_COLOR, BTN_HOVER))
                action_excellent_paid();
            if (btn(MAIN_X+510+dx, BTN_Y, 110, BTN_H, "По форме",  BTN_COLOR, BTN_HOVER))
                action_by_form();
        }

        /* Поля редактирования или ввод имени файла */
        if (g_waiting_filename) {
            DrawRectangle(MAIN_X+4, PANEL_Y, SCREEN_W-MAIN_X-8, PANEL_H+10,
                          (Color){ 240, 240, 250, 255 });
            DrawRectangleLines(MAIN_X+4, PANEL_Y, SCREEN_W-MAIN_X-8, PANEL_H+10,
                               (Color){ 200, 200, 225, 255 });
            const char *hint = (g_filename_mode==0) ? "Введите имя файла для импорта:"
                              : (g_filename_mode==1) ? "Введите имя файла для экспорта CSV:"
                              : "Введите имя файла для экспорта TXT:";
            DrawTextEx(g_font, hint,
                       (Vector2){ MAIN_X+12, PANEL_Y+8 }, 14, 1, LABEL_COLOR);
            textbox(MAIN_X+12, PANEL_Y+30, 300, INPUT_H, "Файл:",
                    g_temp_filename, 1);
            if (btn(MAIN_X+320, PANEL_Y+30, 80, INPUT_H, "OK",
                    (Color){50,150,50,255}, (Color){70,190,70,255})) {
                if (g_temp_filename_len > 0) {
                    if (g_filename_mode == 0) import_csv(g_temp_filename);
                    else if (g_filename_mode == 1) export_csv(g_temp_filename, g_display, g_display_count);
                    else export_txt(g_temp_filename, g_display, g_display_count);
                }
                g_waiting_filename = 0;
            }
            if (btn(MAIN_X+408, PANEL_Y+30, 80, INPUT_H, "Отмена",
                    (Color){180,50,50,255}, (Color){220,70,70,255}))
                g_waiting_filename = 0;
        } else {
            textbox(MAIN_X+10, INPUT_Y, 240, INPUT_H, "Спец.:", g_inp_spec, g_focus == 0);
            textbox(MAIN_X+260, INPUT_Y, 110, INPUT_H, "Груп.:", g_inp_group, g_focus == 1);
            textbox(MAIN_X+380, INPUT_Y, 280, INPUT_H, "ФИО:",   g_inp_name,  g_focus == 2);
            textbox(MAIN_X+670, INPUT_Y, 130, INPUT_H, "Форма:", g_inp_form,  g_focus == 3);

            const char *grade_labels[] = { "1:", "2:", "3:", "4:" };
            int gx = MAIN_X+808;
            for (int i = 0; i < GRADES; i++) {
                textbox(gx, INPUT_Y, 46, INPUT_H, grade_labels[i],
                        g_inp_grades[i], g_focus == 4 + i);
                gx += 50;
            }

            const char *form_items[] = { "Все", "Бюджет", "Плат" };
            {
                int old_form = g_filter_form;
                g_filter_form = radio_group(MAIN_X+1010, INPUT_Y, INPUT_H, form_items, 3, g_filter_form);
                if (g_filter_form != old_form && !g_editing) apply_filter();
            }
        }

        draw_table();

        /* Строка состояния */
        const char *mode = "";
        switch (g_view) {
            case VIEW_ALL:            mode = "Все студенты"; break;
            case VIEW_SORTED:         mode = "Сортировка по группам и ФИО"; break;
            case VIEW_EXCELLENT_PAID: mode = "Отличники (платная форма)"; break;
            case VIEW_BY_FORM:        mode = "Списки по форме обучения"; break;
        }
        char info[128];
        snprintf(info, sizeof(info), "Режим: %s  |  Показано: %d из %d",
                 mode, g_display_count, g_list.count);
        DrawTextEx(g_font, info, (Vector2){ (float)MAIN_X, (float)SCREEN_H - 22 },
                   14, 1, LABEL_COLOR);

        /* Сообщение статуса */
        double elapsed = GetTime() - g_status_time;
        if (elapsed < 4.0 && g_status[0]) {
            Color sc = (strncmp(g_status, "Ошибка", 6) == 0) ? STATUS_ERR : STATUS_OK;
            if (elapsed > 3.0) sc.a = (unsigned char)(255 - (int)((elapsed - 3.0) * 255));
            Vector2 sz = MeasureTextEx(g_font, g_status, 14, 1);
            DrawTextEx(g_font, g_status,
                       (Vector2){ (float)(SCREEN_W - sz.x - 10), (float)SCREEN_H - 22 },
                       14, 1, sc);
        }

        /* === Боковая панель (поверх всего) === */
        DrawRectangle(0, 0, SIDE_PANEL_W, SCREEN_H, (Color){ 230, 230, 240, 255 });
        DrawLine(SIDE_PANEL_W, 0, SIDE_PANEL_W, SCREEN_H, (Color){ 200, 200, 210, 255 });
        {
            int sy = 10;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Сохранить CSV", BTN_SAVE, BTN_SAVE_HOVER))
                saveDbCsv(CSV_FILE, &g_list);
            sy += BTN_H + 5;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Загрузить CSV", BTN_SAVE, BTN_SAVE_HOVER))
                { save_undo_state(); listClear(&g_list); loadDbCsv(CSV_FILE, &g_list); clear_input_fields(); g_selected_idx = -1; apply_filter(); }
            sy += BTN_H + 5;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Импорт CSV", BTN_COLOR, BTN_HOVER))
                { g_temp_filename[0]='\0'; g_temp_filename_len=0; g_filename_mode=0; g_waiting_filename=1; }
            sy += BTN_H + 5;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Экспорт CSV", BTN_COLOR, BTN_HOVER))
                { g_temp_filename[0]='\0'; g_temp_filename_len=0; g_filename_mode=1; g_waiting_filename=1; }
            sy += BTN_H + 5;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Экспорт TXT", BTN_COLOR, BTN_HOVER))
                { g_temp_filename[0]='\0'; g_temp_filename_len=0; g_filename_mode=2; g_waiting_filename=1; }
            sy += BTN_H + 5;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Очистить", BTN_CLEAR, BTN_CLEAR_HOVER))
                action_clear();
            sy += BTN_H + 5;
            if (btn(5, sy, SIDE_PANEL_W-10, BTN_H, "Отменить", BTN_UNDO, BTN_UNDO_HOVER))
                action_undo();
        }

        EndDrawing();
    }

    /* Автосохранение при выходе */
    saveDbCsv(CSV_FILE, &g_list);

    /* Очистка памяти */
    listClear(&g_list);
    listClear(&g_undo_list);
    if (g_display) free(g_display);

    UnloadFont(g_font);
    CloseWindow();
    return 0;
}
