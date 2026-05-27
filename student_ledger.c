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
 *    3. Фильтр: отличники бюджетной формы (ср. балл >= 8.0)
 *    4. Списки по форме обучения (бюджет / плат),
 *       отсортированные по возрастанию среднего балла
 *    5. Поиск по ФИО (частично, без учёта регистра),
 *       номеру группы (точно), форме обучения (точно)
 *    6. Сохранение и загрузка базы в/из CSV-файла
 *       (students.csv, поля разделены ';')
 *    7. Автосохранение при выходе
 *
 *  Сборка (MSYS2 MinGW64/UCRT64):
 *    gcc student_ledger.c -o student_ledger.exe ^
 *        -IC:/msys64/mingw64/include          ^
 *        -LC:/msys64/mingw64/lib              ^
 *        -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32
 * ============================================================
 */

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifdef _WIN32
/* Минимальные объявления WinAPI — только типы из <windows.h>,
   которых нет в <raylib.h> (windef.h + winbase.h не конфликтуют).
   <commdlg.h> не включаем, GetOpenFileNameA объявляем вручную. */
#include <windef.h>
#include <winbase.h>

/* Структура OPENFILENAMEA для GetOpenFileNameA (определена в commdlg.h,
   но мы не включаем этот заголовок, чтобы избежать конфликтов raylib). */
typedef struct {
    unsigned long  lStructSize;
    void*          hwndOwner;
    void*          hInstance;
    const char*    lpstrFilter;
    char*          lpstrCustomFilter;
    unsigned long  nMaxCustFilter;
    unsigned long  nFilterIndex;
    char*          lpstrFile;
    unsigned long  nMaxFile;
    char*          lpstrFileTitle;
    unsigned long  nMaxFileTitle;
    const char*    lpstrInitialDir;
    const char*    lpstrTitle;
    unsigned long  Flags;
    unsigned short nFileOffset;
    unsigned short nFileExtension;
    const char*    lpstrDefExt;
    long long      lCustData;
    void*          lpfnHook;
    const char*    lpTemplateName;
    void*          pvReserved;
    unsigned long  dwReserved;
    unsigned long  FlagsEx;
} OPENFILENAMEA;

/* Флаги для OPENFILENAMEA (из commdlg.h) */
#define OFN_PATHMUSTEXIST  0x00000800
#define OFN_FILEMUSTEXIST  0x00001000
#define OFN_HIDEREADONLY   0x00000004

__declspec(dllimport) int __stdcall GetOpenFileNameA(OPENFILENAMEA*);
#endif

/* Путь к папке исполняемого файла (для всех файловых операций) */
static char executableDirectoryPath[MAX_PATH] = "";

/* Авто-имена для экспорта */
#define EXPORT_CSV_FILE "students_export.csv"
#define EXPORT_TXT_FILE "students_export.txt"

/* Привязываем относительные файлы к папке исполняемого файла,
   чтобы разные конфигурации запуска (cwd) не ломали импорт/экспорт. */
static void set_workdir_to_exe_dir(void) {
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof(exe_path));
    if (len == 0 || len >= sizeof(exe_path)) return;

    char *last_slash = strrchr(exe_path, '\\');
    char *last_fslash = strrchr(exe_path, '/');
    char *last = last_slash;
    if (last_fslash && (!last_slash || last_fslash > last_slash)) last = last_fslash;
    if (!last) return;
    *last = '\0';

    strncpy(executableDirectoryPath, exe_path, MAX_PATH - 1);
    executableDirectoryPath[MAX_PATH - 1] = '\0';
    SetCurrentDirectoryA(exe_path);
#endif
}

/* Собирает полный путь: если filename относительный, 
   добавляет executableDirectoryPath в начале, иначе оставляет как есть. */
static void build_full_path(char *out, size_t out_sz, const char *filename) {
    if (executableDirectoryPath[0] && filename[0] &&
        filename[0] != '/' && filename[0] != '\\' &&
        !(filename[0] && filename[1] == ':')) {
        snprintf(out, out_sz, "%s\\%s", executableDirectoryPath, filename);
    } else {
        strncpy(out, filename, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}

/* Открывает диалог выбора файла для импорта CSV, возвращает 1 если файл выбран */
static int file_dialog_open(char *out, size_t out_sz) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "CSV Files\0*.csv\0All Files\0*.*\0";
    ofn.lpstrFile = out;
    ofn.nMaxFile = (DWORD)out_sz;
    ofn.lpstrTitle = "Выберите CSV-файл для импорта";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    out[0] = '\0';
    return GetOpenFileNameA(&ofn);
}

/* ==========================================================
 *  КОНСТАНТЫ
 * ========================================================== */

#define WINDOW_WIDTH           1400
#define WINDOW_HEIGHT           700
#define SIDE_PANEL_WIDTH        160
#define MAIN_CONTENT_OFFSET_X   180   /* сдвиг основного контента вправо */

#define FONT_SIZE_INTERFACE     18
#define DEFAULT_PADDING         10

#define MAJOR_LEN       50
#define NAME_LEN       100
#define GROUP_LEN        8
#define FORM_LEN        24
#define MARK_COUNT       4
#define MARK_TEXT_LEN    4

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
#define TABLE_H     (WINDOW_HEIGHT - ROW_Y_START - 28)

/* Цвета интерфейса — тёмная тема */
#define BG_COLOR        CLITERAL(Color){  24,  24,  24, 255 }
#define SIDE_BG         CLITERAL(Color){  30,  30,  35, 255 }
#define SIDE_LINE       CLITERAL(Color){  50,  50,  55, 255 }
#define PANEL_BG        CLITERAL(Color){  34,  34,  40, 255 }
#define PANEL_BORDER    CLITERAL(Color){  50,  50,  58, 255 }
#define HEADER_BG       CLITERAL(Color){  50,  55,  70, 255 }
#define HEADER_TEXT     CLITERAL(Color){ 210, 215, 230, 255 }
#define ROW_EVEN        CLITERAL(Color){  30,  30,  35, 255 }
#define ROW_ODD         CLITERAL(Color){  35,  35,  42, 255 }
#define ROW_SELECTED    CLITERAL(Color){  50,  60,  85, 255 }
#define BTN_COLOR       CLITERAL(Color){  60,  90, 180, 255 }
#define BTN_HOVER       CLITERAL(Color){  75, 110, 210, 255 }
#define BTN_ADD          BTN_COLOR
#define BTN_ADD_HOVER    BTN_HOVER
#define BTN_DEL         CLITERAL(Color){ 180,  45,  45, 255 }
#define BTN_DEL_HOVER   CLITERAL(Color){ 220,  60,  60, 255 }
#define BTN_SAVE         BTN_COLOR
#define BTN_SAVE_HOVER   BTN_HOVER
#define BTN_CLEAR       CLITERAL(Color){  55,  55,  60, 255 }
#define BTN_CLEAR_HOVER CLITERAL(Color){  70,  70,  80, 255 }
#define BTN_UNDO        CLITERAL(Color){  55,  55,  65, 255 }
#define BTN_UNDO_HOVER  CLITERAL(Color){  70,  70,  80, 255 }
#define BTN_TEXT        CLITERAL(Color){ 220, 220, 230, 255 }
#define INPUT_BG        CLITERAL(Color){  40,  40,  46, 255 }
#define INPUT_BORDER    CLITERAL(Color){  70,  70,  80, 255 }
#define FOCUS_BORDER    CLITERAL(Color){  80, 120, 220, 255 }
#define RADIO_ACT       CLITERAL(Color){  80, 120, 220, 255 }
#define RADIO_INACT     CLITERAL(Color){  80,  80,  90, 255 }
#define TEXT_COLOR      CLITERAL(Color){ 200, 200, 210, 255 }
#define LABEL_COLOR     CLITERAL(Color){ 170, 170, 180, 255 }
#define TITLE_BUDGET    CLITERAL(Color){  60, 160,  80, 255 }
#define TITLE_PAID      CLITERAL(Color){ 220, 130,  50, 255 }
#define STATUS_OK       CLITERAL(Color){  60, 180,  80, 255 }
#define STATUS_ERR      CLITERAL(Color){ 220,  60,  60, 255 }

/* ==========================================================
 *  СТРУКТУРЫ
 * ========================================================== */

/*
 *  Student – узел однонаправленного списка.
 *  Содержит все данные об одном студенте и указатель на
 *  следующий элемент списка (next).
 */
typedef struct Student {
    char  specialty[MAJOR_LEN + 1];
    int   groupNumber;
    char  fullName[NAME_LEN + 1];
    char  educationForm[FORM_LEN + 1];
    int   marks[MARK_COUNT];
    float averageMark;
    struct Student *nextStudent;
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


#define PAGE_TABLE  0
#define PAGE_FILE   1
static int currentUiPageIndex = PAGE_TABLE;
#define UNDO_DEPTH  10
static List mainStudentList = { NULL, NULL, 0 };      /* основной список */
static List undoHistoryStacks[UNDO_DEPTH];            /* стек для многоуровневой отмены */
static int  undoHistoryTopIndex = -1;                 /* вершина стека (-1 = пусто) */
static Student **displayStudentPointers = NULL;     /* массив указателей для отображения */
static int displayRecordCount = 0;                  /* количество отображаемых записей */
static ViewMode currentViewMode = VIEW_ALL;         /* текущий режим просмотра */
static float tableScrollOffset = 0.0f;              /* смещение прокрутки таблицы */
static int selectedTableRowIndex = -1;              /* индекс выбранной строки */
static int isRecordEditMode = 0;                    /* 1 = режим редактирования активен */
static Student editModeBackupRecord;                /* резервная копия при входе в режим */

/* Буферы ввода для редактирования / добавления */
static char inputSpecialtyText[MAJOR_LEN + 1] = "";
static int  inputSpecialtyTextLength = 0;
static char inputGroupNumberText[GROUP_LEN + 1] = "";
static int  inputGroupNumberTextLength = 0;
static char inputFullNameText[NAME_LEN + 1] = "";
static int  inputFullNameTextLength = 0;
static char inputEducationFormText[FORM_LEN + 1] = "";
static int  inputEducationFormTextLength = 0;
static char inputMarkTexts[MARK_COUNT][MARK_TEXT_LEN] = { "" };
static int  inputMarkTextLengths[MARK_COUNT] = { 0, 0, 0, 0 };

/* Фильтр по форме: 0 = все, 1 = бюджет, 2 = плат */
static int filterEducationFormSelection = 0;

/* Фокус ввода: -1 = нет, 0-3 = поля ввода (спец,группа,ФИО,форма), 4-7 = оценки */
static int activeInputFieldIndex = -1;

/* Строка состояния */
static char statusMessageText[128] = "";
static double statusMessageShowTime = 0.0;

static Font applicationFont;

/* Предварительные объявления */
static void clear_input_fields(void);
static void apply_filter(void);

/* ==========================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
 * ========================================================== */

/*
 *  calculateAverageMark – вычисление среднего балла по четырём оценкам.
 */
static float calculateAverageMark(const int markValues[MARK_COUNT]) {
    int markSum = 0;
    for (int markIndex = 0; markIndex < MARK_COUNT; markIndex++)
        markSum += markValues[markIndex];
    return (float)markSum / (float)MARK_COUNT;
}

/*
 *  set_status – устанавливает текст в строке состояния.
 *  color_flag: 0 = обычный, 1 = успех, 2 = ошибка (для окраски).
 */
static void set_status(const char *text) {
    strncpy(statusMessageText, text, sizeof(statusMessageText) - 1);
    statusMessageText[sizeof(statusMessageText) - 1] = '\0';
    statusMessageShowTime = GetTime();
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
static Student *newStudent(const char *specialtyText, int groupNumber,
                           const char *fullNameText, const char *educationFormText,
                           int mark1, int mark2, int mark3, int mark4) {
    Student *studentNode = (Student *)malloc(sizeof(Student));
    if (!studentNode) return NULL;
    strncpy(studentNode->specialty, specialtyText, MAJOR_LEN);
    studentNode->specialty[MAJOR_LEN] = '\0';
    studentNode->groupNumber = groupNumber;
    strncpy(studentNode->fullName, fullNameText, NAME_LEN);
    studentNode->fullName[NAME_LEN] = '\0';
    strncpy(studentNode->educationForm, educationFormText, FORM_LEN);
    studentNode->educationForm[FORM_LEN] = '\0';
    studentNode->marks[0] = mark1;
    studentNode->marks[1] = mark2;
    studentNode->marks[2] = mark3;
    studentNode->marks[3] = mark4;
    studentNode->averageMark = calculateAverageMark(studentNode->marks);
    studentNode->nextStudent = NULL;
    return studentNode;
}

/*
 *  listPushBack – добавляет узел в конец списка.
 */
static void listPushBack(List *targetList, Student *studentNode) {
    if (!targetList->head) {
        targetList->head = studentNode;
        targetList->tail = studentNode;
    } else {
        targetList->tail->nextStudent = studentNode;
        targetList->tail = studentNode;
    }
    targetList->count++;
}

/*
 *  listRemove – удаляет узел по индексу.
 *  Освобождает память, перелинковывает список.
 *  Возвращает 1 при успехе, 0 если индекс вне диапазона.
 */
static int listRemove(List *targetList, int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= targetList->count) return 0;
    Student *previousNode = NULL;
    Student *currentStudent = targetList->head;
    for (int walkIndex = 0; walkIndex < nodeIndex; walkIndex++) {
        previousNode = currentStudent;
        currentStudent = currentStudent->nextStudent;
    }
    if (!previousNode)
        targetList->head = currentStudent->nextStudent;
    else
        previousNode->nextStudent = currentStudent->nextStudent;
    if (currentStudent == targetList->tail)
        targetList->tail = previousNode;
    free(currentStudent);
    targetList->count--;
    return 1;
}

/*
 *  listClear – удаляет все узлы списка, освобождает память.
 */
static void listClear(List *targetList) {
    Student *currentStudent = targetList->head;
    while (currentStudent) {
        Student *nextStudent = currentStudent->nextStudent;
        free(currentStudent);
        currentStudent = nextStudent;
    }
    targetList->head = NULL;
    targetList->tail = NULL;
    targetList->count = 0;
}

/*
 *  listFind – возвращает индекс узла в списке по указателю,
 *  или -1 если не найден.
 */
static int listFind(const List *targetList, const Student *searchStudent) {
    Student *currentStudent = targetList->head;
    for (int walkIndex = 0; walkIndex < targetList->count; walkIndex++) {
        if (currentStudent == searchStudent) return walkIndex;
        currentStudent = currentStudent->nextStudent;
    }
    return -1;
}

/*
 *  listCopy – глубокое копирование списка.
 */
static void listCopy(List *destinationList, const List *sourceList) {
    listClear(destinationList);
    Student *currentStudent = sourceList->head;
    while (currentStudent) {
        Student *studentNode = newStudent(
            currentStudent->specialty, currentStudent->groupNumber,
            currentStudent->fullName, currentStudent->educationForm,
            currentStudent->marks[0], currentStudent->marks[1],
            currentStudent->marks[2], currentStudent->marks[3]);
        if (studentNode) listPushBack(destinationList, studentNode);
        currentStudent = currentStudent->nextStudent;
    }
}

/*
 *  save_undo_state – сохраняет копию mainStudentList в стек отмены.
 */
static void save_undo_state(void) {
    if (undoHistoryTopIndex < UNDO_DEPTH - 1) {
        undoHistoryTopIndex++;
    } else {
        listClear(&undoHistoryStacks[0]);
        for (int i = 0; i < UNDO_DEPTH - 1; i++)
            undoHistoryStacks[i] = undoHistoryStacks[i + 1];
    }
    listCopy(&undoHistoryStacks[undoHistoryTopIndex], &mainStudentList);
}

/* ==========================================================
 *  ФУНКЦИИ РАБОТЫ С CSV
 * ========================================================== */

/*
 *  saveDbCsv – сохраняет базу студентов в CSV-файл.
 *  Формат: специальность;группа;ФИО;форма;оценка1;оценка2;оценка3;оценка4
 *  Каждая запись на отдельной строке.
 */
static void saveDbCsv(const char *databaseFileName, const List *studentList) {
    char full[MAX_PATH];
    build_full_path(full, sizeof(full), databaseFileName);
    FILE *fileHandle = fopen(full, "w");
    if (!fileHandle) { set_status("Ошибка: не удалось открыть файл для записи"); return; }
    Student *currentStudent = studentList->head;
    while (currentStudent) {
        fprintf(fileHandle, "%s;%d;%s;%s;%d;%d;%d;%d\n",
                currentStudent->specialty, currentStudent->groupNumber,
                currentStudent->fullName, currentStudent->educationForm,
                currentStudent->marks[0], currentStudent->marks[1],
                currentStudent->marks[2], currentStudent->marks[3]);
        currentStudent = currentStudent->nextStudent;
    }
    fclose(fileHandle);
    set_status("База сохранена в students.csv");
}

/*
 *  loadDbCsv – загружает базу студентов из CSV-файла.
 *  Парсит строки, разделённые ';', создаёт узлы.
 *  Вычисляет средний балл для каждой записи.
 */
static void loadDbCsv(const char *databaseFileName, List *studentList) {
    char full[MAX_PATH];
    build_full_path(full, sizeof(full), databaseFileName);
    FILE *fileHandle = fopen(full, "r");
    if (!fileHandle) { set_status("Файл students.csv не найден, создан пустой список"); return; }
    char line[512];
    int loadedRecordCount = 0;
    while (fgets(line, sizeof(line), fileHandle)) {
        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[len - 1] = '\0';
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
        if (len == 0) continue;

        char *specialtyField = line;
        /* Убираем UTF-8 BOM в начале файла/строки */
        if ((unsigned char)specialtyField[0] == 0xEF &&
            (unsigned char)specialtyField[1] == 0xBB &&
            (unsigned char)specialtyField[2] == 0xBF) {
            specialtyField += 3;
        }
        char *delimiter = strchr(specialtyField, ';');  if (!delimiter) continue; *delimiter++ = '\0';
        char *groupField = delimiter;
        delimiter = strchr(groupField, ';');       if (!delimiter) continue; *delimiter++ = '\0';
        char *fullNameField = delimiter;
        delimiter = strchr(fullNameField, ';');    if (!delimiter) continue; *delimiter++ = '\0';
        char *educationFormField = delimiter;
        delimiter = strchr(educationFormField, ';'); if (!delimiter) continue; *delimiter++ = '\0';
        int mark1 = atoi(delimiter);
        delimiter = strchr(delimiter, ';');        if (!delimiter) continue; *delimiter++ = '\0';
        int mark2 = atoi(delimiter);
        delimiter = strchr(delimiter, ';');        if (!delimiter) continue; *delimiter++ = '\0';
        int mark3 = atoi(delimiter);
        delimiter = strchr(delimiter, ';');        if (!delimiter) continue; *delimiter++ = '\0';
        int mark4 = atoi(delimiter);

        int groupNumber = atoi(groupField);
        Student *studentNode = newStudent(specialtyField, groupNumber, fullNameField,
                                          educationFormField, mark1, mark2, mark3, mark4);
        if (studentNode) { listPushBack(studentList, studentNode); loadedRecordCount++; }
    }
    fclose(fileHandle);
    char msg[64];
    snprintf(msg, sizeof(msg), "Загружено %d записей из students.csv", loadedRecordCount);
    set_status(msg);
}

/*
 *  import_csv – импортирует студентов из произвольного CSV-файла.
 *  Добавляет записи в конец текущего списка (не заменяя).
 *  Перед импортом сохраняет undo-состояние.
 */
static void import_csv(const char *sourceFileName) {
    char full[MAX_PATH];
    build_full_path(full, sizeof(full), sourceFileName);
    FILE *fileHandle = fopen(full, "r");
    if (!fileHandle) { set_status("Ошибка: не удалось открыть файл для импорта"); return; }
    save_undo_state();
    char line[512];
    int importedRecordCount = 0;
    while (fgets(line, sizeof(line), fileHandle)) {
        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[len - 1] = '\0';
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
        if (len == 0) continue;

        char *specialtyField = line;
        if ((unsigned char)specialtyField[0] == 0xEF &&
            (unsigned char)specialtyField[1] == 0xBB &&
            (unsigned char)specialtyField[2] == 0xBF) {
            specialtyField += 3;
        }
        char *delimiter = strchr(specialtyField, ';');  if (!delimiter) continue; *delimiter++ = '\0';
        char *groupField = delimiter;
        delimiter = strchr(groupField, ';');       if (!delimiter) continue; *delimiter++ = '\0';
        char *fullNameField = delimiter;
        delimiter = strchr(fullNameField, ';');    if (!delimiter) continue; *delimiter++ = '\0';
        char *educationFormField = delimiter;
        delimiter = strchr(educationFormField, ';'); if (!delimiter) continue; *delimiter++ = '\0';
        int mark1 = atoi(delimiter);
        delimiter = strchr(delimiter, ';');        if (!delimiter) continue; *delimiter++ = '\0';
        int mark2 = atoi(delimiter);
        delimiter = strchr(delimiter, ';');        if (!delimiter) continue; *delimiter++ = '\0';
        int mark3 = atoi(delimiter);
        delimiter = strchr(delimiter, ';');        if (!delimiter) continue; *delimiter++ = '\0';
        int mark4 = atoi(delimiter);

        int groupNumber = atoi(groupField);
        Student *studentNode = newStudent(specialtyField, groupNumber, fullNameField,
                                          educationFormField, mark1, mark2, mark3, mark4);
        if (studentNode) { listPushBack(&mainStudentList, studentNode); importedRecordCount++; }
    }
    fclose(fileHandle);
    clear_input_fields();
    selectedTableRowIndex = -1;
    activeInputFieldIndex = -1;
    apply_filter();
    char msg[64];
    snprintf(msg, sizeof(msg), "Импортировано %d записей из %s", importedRecordCount, sourceFileName);
    set_status(msg);
}

/*
 *  export_csv – экспортирует текущий отображаемый список
 *  (displayStudentPointers) в CSV-файл. Формат: специальность;группа;ФИО;форма;оценка1;…;оценка4.
 */
static void export_csv(const char *exportFileName, Student **displayArray, int recordCount) {
    FILE *fileHandle = fopen(exportFileName, "w");
    if (!fileHandle) { set_status("Ошибка: не удалось создать файл для экспорта"); return; }
    {
        const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, fileHandle);
    }
    for (int rowIndex = 0; rowIndex < recordCount; rowIndex++) {
        Student *studentRecord = displayArray[rowIndex];
        fprintf(fileHandle, "%s;%d;%s;%s;%d;%d;%d;%d\n",
                studentRecord->specialty, studentRecord->groupNumber,
                studentRecord->fullName, studentRecord->educationForm,
                studentRecord->marks[0], studentRecord->marks[1],
                studentRecord->marks[2], studentRecord->marks[3]);
    }
    fclose(fileHandle);
    char msg[64];
    snprintf(msg, sizeof(msg), "Экспортировано %d записей в %s", recordCount, exportFileName);
    set_status(msg);
}

/*
 *  export_txt – экспортирует текущий отображаемый список
 *  в текстовый файл с форматированием (колонки, заголовки).
 */
static void export_txt(const char *exportFileName, Student **displayArray, int recordCount) {
    FILE *fileHandle = fopen(exportFileName, "w");
    if (!fileHandle) { set_status("Ошибка: не удалось создать файл для экспорта"); return; }
    {
        const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
        fwrite(bom, 1, 3, fileHandle);
    }
    fprintf(fileHandle, "%-4s %-8s %-30s %-30s %-12s %-16s %s\n",
            "№", "Группа", "ФИО", "Специальность", "Форма", "Оценки", "Ср.балл");
    for (int separatorIndex = 0; separatorIndex < 101; separatorIndex++) fputc('-', fileHandle);
    fputc('\n', fileHandle);
    for (int rowIndex = 0; rowIndex < recordCount; rowIndex++) {
        Student *studentRecord = displayArray[rowIndex];
        char marksText[32];
        snprintf(marksText, sizeof(marksText), "%d %d %d %d",
                 studentRecord->marks[0], studentRecord->marks[1],
                 studentRecord->marks[2], studentRecord->marks[3]);
        fprintf(fileHandle, "%-4d %-8d %-30s %-30s %-12s %-16s %.2f\n",
                rowIndex + 1, studentRecord->groupNumber, studentRecord->fullName,
                studentRecord->specialty, studentRecord->educationForm,
                marksText, studentRecord->averageMark);
    }
    fclose(fileHandle);
    char msg[64];
    snprintf(msg, sizeof(msg), "Отчёт экспортирован в %s", exportFileName);
    set_status(msg);
}

/* ==========================================================
 *  УПРАВЛЕНИЕ ОТОБРАЖЕНИЕМ
 * ========================================================== */

/*
 *  rebuild_display – перестраивает массив displayStudentPointers
 *  из текущего списка mainStudentList. Освобождает старый массив,
 *  выделяет новый, заполняет указателями.
 *  Сбрасывает режим на VIEW_ALL.
 */
static void rebuild_display(void) {
    if (displayStudentPointers) { free(displayStudentPointers); displayStudentPointers = NULL; }
    displayRecordCount = mainStudentList.count;
    if (displayRecordCount > 0) {
        displayStudentPointers = (Student **)malloc(displayRecordCount * sizeof(Student *));
        Student *currentStudent = mainStudentList.head;
        for (int rowIndex = 0; rowIndex < displayRecordCount; rowIndex++) {
            displayStudentPointers[rowIndex] = currentStudent;
            currentStudent = currentStudent->nextStudent;
        }
    }
    currentViewMode = VIEW_ALL;
    tableScrollOffset = 0.0f;
}

/* ==========================================================
 *  ФУНКЦИИ СРАВНЕНИЯ (для qsort)
 * ========================================================== */

static int cmp_group_name(const void *pointerA, const void *pointerB) {
    const Student *studentA = *(const Student **)pointerA;
    const Student *studentB = *(const Student **)pointerB;
    if (studentA->groupNumber != studentB->groupNumber)
        return studentA->groupNumber - studentB->groupNumber;
    return stricmp(studentA->fullName, studentB->fullName);
}

static int cmp_avg_asc(const void *pointerA, const void *pointerB) {
    const Student *studentA = *(const Student **)pointerA;
    const Student *studentB = *(const Student **)pointerB;
    if (fabs(studentA->averageMark - studentB->averageMark) > 0.001f)
        return (studentA->averageMark < studentB->averageMark) ? -1 : 1;
    return stricmp(studentA->fullName, studentB->fullName);
}

/*
 *  isExcellentBudget – отличник на бюджетной форме (средний балл >= 8.0).
 */
static int isExcellentBudget(const Student *studentRecord) {
    return strcmp(studentRecord->educationForm, "бюджетная") == 0 &&
           studentRecord->averageMark >= 8.0f;
}

static int showOnlyExcellentBudget = 0;

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
    if (displayRecordCount > 0)
        qsort(displayStudentPointers, displayRecordCount, sizeof(Student *), cmp_group_name);
    currentViewMode = VIEW_SORTED;
    tableScrollOffset = 0.0f;
    selectedTableRowIndex = -1;
}

/*
 *  action_excellent_paid – переключение фильтра отличников бюджетной формы.
 */
static void action_excellent_paid(void) {
    showOnlyExcellentBudget = !showOnlyExcellentBudget;
    currentViewMode = showOnlyExcellentBudget ? VIEW_EXCELLENT_PAID : VIEW_ALL;
    tableScrollOffset = 0.0f;
    selectedTableRowIndex = -1;
    set_status(showOnlyExcellentBudget
               ? "Фильтр: отличники бюджетной формы (ср. балл >= 8.0)"
               : "Фильтр отличников отключён");
}

/*
 *  action_by_form – формирует два списка (бюджет / плат),
 *  отсортированных по возрастанию среднего балла.
 */
static void action_by_form(void) {
    int budgetCount = 0, paidCount = 0;
    Student *currentStudent = mainStudentList.head;
    while (currentStudent) {
        if (strcmp(currentStudent->educationForm, "бюджетная") == 0) budgetCount++;
        else paidCount++;
        currentStudent = currentStudent->nextStudent;
    }

    Student **budgetStudents = budgetCount > 0
        ? (Student **)malloc(budgetCount * sizeof(Student *)) : NULL;
    Student **paidStudents = paidCount > 0
        ? (Student **)malloc(paidCount * sizeof(Student *)) : NULL;
    budgetCount = 0;
    paidCount = 0;
    currentStudent = mainStudentList.head;
    while (currentStudent) {
        if (strcmp(currentStudent->educationForm, "бюджетная") == 0)
            budgetStudents[budgetCount++] = currentStudent;
        else
            paidStudents[paidCount++] = currentStudent;
        currentStudent = currentStudent->nextStudent;
    }

    if (budgetStudents) qsort(budgetStudents, budgetCount, sizeof(Student *), cmp_avg_asc);
    if (paidStudents)   qsort(paidStudents, paidCount, sizeof(Student *), cmp_avg_asc);

    if (displayStudentPointers) free(displayStudentPointers);
    displayRecordCount = budgetCount + paidCount;
    displayStudentPointers = (Student **)malloc(displayRecordCount * sizeof(Student *));
    int displayIndex = 0;
    for (int rowIndex = 0; rowIndex < budgetCount; rowIndex++)
        displayStudentPointers[displayIndex++] = budgetStudents[rowIndex];
    for (int rowIndex = 0; rowIndex < paidCount; rowIndex++)
        displayStudentPointers[displayIndex++] = paidStudents[rowIndex];

    free(budgetStudents);
    free(paidStudents);

    currentViewMode = VIEW_BY_FORM;
    tableScrollOffset = 0.0f;
    selectedTableRowIndex = -1;
}

/*
 *  matches_filter – проверяет, проходит ли студент через текущий
 *  фильтр по полям ввода и форме обучения.
 */
static int matches_filter(const Student *studentRecord) {
    if (inputSpecialtyTextLength > 0 &&
        !str_icontains(studentRecord->specialty, inputSpecialtyText))
        return 0;
    if (inputGroupNumberTextLength > 0) {
        int filterGroupNumber = atoi(inputGroupNumberText);
        if (filterGroupNumber > 0 && studentRecord->groupNumber != filterGroupNumber)
            return 0;
    }
    if (inputFullNameTextLength > 0 &&
        !str_icontains(studentRecord->fullName, inputFullNameText))
        return 0;
    if (inputEducationFormTextLength > 0 &&
        strcmp(studentRecord->educationForm, inputEducationFormText) != 0)
        return 0;
    if (filterEducationFormSelection == 1 &&
        strcmp(studentRecord->educationForm, "бюджетная") != 0)
        return 0;
    if (filterEducationFormSelection == 2 &&
        strcmp(studentRecord->educationForm, "платная") != 0)
        return 0;
    return 1;
}

/*
 *  apply_filter – перестраивает displayStudentPointers по текущему фильтру
 *  из полей ввода и радиокнопок.
 */
static void apply_filter(void) {
    if (displayStudentPointers) { free(displayStudentPointers); displayStudentPointers = NULL; }
    displayRecordCount = 0;
    selectedTableRowIndex = -1;

    Student *currentStudent = mainStudentList.head;
    while (currentStudent) {
        if (matches_filter(currentStudent)) displayRecordCount++;
        currentStudent = currentStudent->nextStudent;
    }

    if (displayRecordCount > 0) {
        displayStudentPointers = (Student **)malloc(displayRecordCount * sizeof(Student *));
        int displayIndex = 0;
        currentStudent = mainStudentList.head;
        while (currentStudent) {
            if (matches_filter(currentStudent))
                displayStudentPointers[displayIndex++] = currentStudent;
            currentStudent = currentStudent->nextStudent;
        }
    }

    currentViewMode = VIEW_ALL;
    tableScrollOffset = 0.0f;
}

/*
 *  clear_input_fields – очищает все поля ввода.
 */
static void clear_input_fields(void) {
    inputSpecialtyText[0] = '\0';  inputSpecialtyTextLength = 0;
    inputGroupNumberText[0] = '\0'; inputGroupNumberTextLength = 0;
    inputFullNameText[0] = '\0';  inputFullNameTextLength = 0;
    inputEducationFormText[0] = '\0';  inputEducationFormTextLength = 0;
    for (int i = 0; i < MARK_COUNT; i++) {
        inputMarkTexts[i][0] = '\0';
        inputMarkTextLengths[i] = 0;
    }
}

/*
 *  fill_input_from_student – заполняет поля ввода
 *  данными из выбранного студента (для редактирования).
 */
static void fill_input_from_student(const Student *studentRecord) {
    strncpy(inputSpecialtyText, studentRecord->specialty, MAJOR_LEN);
    inputSpecialtyText[MAJOR_LEN] = '\0';
    inputSpecialtyTextLength = (int)strlen(inputSpecialtyText);

    snprintf(inputGroupNumberText, GROUP_LEN + 1, "%d", studentRecord->groupNumber);
    inputGroupNumberTextLength = (int)strlen(inputGroupNumberText);

    strncpy(inputFullNameText, studentRecord->fullName, NAME_LEN);
    inputFullNameText[NAME_LEN] = '\0';
    inputFullNameTextLength = (int)strlen(inputFullNameText);

    strncpy(inputEducationFormText, studentRecord->educationForm, FORM_LEN);
    inputEducationFormText[FORM_LEN] = '\0';
    inputEducationFormTextLength = (int)strlen(inputEducationFormText);

    for (int markIndex = 0; markIndex < MARK_COUNT; markIndex++) {
        snprintf(inputMarkTexts[markIndex], MARK_TEXT_LEN, "%d",
                 studentRecord->marks[markIndex]);
        inputMarkTextLengths[markIndex] = (int)strlen(inputMarkTexts[markIndex]);
    }
}

/*
 *  validate_input – проверяет корректность полей ввода.
 *  Возвращает 1 если данные корректны, иначе 0.
 */
static int validate_input(void) {
    if (inputSpecialtyTextLength == 0) { set_status("Ошибка: введите специальность"); return 0; }
    if (inputFullNameTextLength == 0) { set_status("Ошибка: введите ФИО"); return 0; }
    if (inputGroupNumberTextLength == 0 || atoi(inputGroupNumberText) <= 0) {
        set_status("Ошибка: группа должна быть положительным числом"); return 0;
    }
    if (inputEducationFormTextLength == 0) { set_status("Ошибка: введите форму обучения"); return 0; }
    if (strcmp(inputEducationFormText, "бюджетная") != 0 && strcmp(inputEducationFormText, "платная") != 0) {
        set_status("Ошибка: форма обучения — 'бюджетная' или 'платная'"); return 0;
    }
    for (int i = 0; i < MARK_COUNT; i++) {
        int v = atoi(inputMarkTexts[i]);
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
    int groupNumber = atoi(inputGroupNumberText);
    int mark1 = atoi(inputMarkTexts[0]);
    int mark2 = atoi(inputMarkTexts[1]);
    int mark3 = atoi(inputMarkTexts[2]);
    int mark4 = atoi(inputMarkTexts[3]);

    Student *studentNode = newStudent(inputSpecialtyText, groupNumber, inputFullNameText,
                                      inputEducationFormText, mark1, mark2, mark3, mark4);
    if (!studentNode) { set_status("Ошибка: не удалось выделить память"); return; }
    listPushBack(&mainStudentList, studentNode);
    clear_input_fields();
    activeInputFieldIndex = -1;
    apply_filter();
    set_status("Студент добавлен");
}

/*
 *  copy_student_data – копирует данные из src в dst (без next).
 */
static void copy_student_data(Student *destination, const Student *source) {
    strncpy(destination->specialty, source->specialty, MAJOR_LEN);
    destination->specialty[MAJOR_LEN] = '\0';
    destination->groupNumber = source->groupNumber;
    strncpy(destination->fullName, source->fullName, NAME_LEN);
    destination->fullName[NAME_LEN] = '\0';
    strncpy(destination->educationForm, source->educationForm, FORM_LEN);
    destination->educationForm[FORM_LEN] = '\0';
    for (int markIndex = 0; markIndex < MARK_COUNT; markIndex++)
        destination->marks[markIndex] = source->marks[markIndex];
    destination->averageMark = source->averageMark;
}

/*
 *  action_edit_mode – входит в режим редактирования.
 *  Сохраняет резервную копию и заполняет поля ввода.
 */
static void action_edit_mode(void) {
    if (selectedTableRowIndex < 0 || selectedTableRowIndex >= displayRecordCount) {
        set_status("Ошибка: выберите студента из таблицы");
        return;
    }
    copy_student_data(&editModeBackupRecord, displayStudentPointers[selectedTableRowIndex]);
    fill_input_from_student(displayStudentPointers[selectedTableRowIndex]);
    isRecordEditMode = 1;
    activeInputFieldIndex = 0;
    set_status("Режим редактирования — изменения видны в таблице");
}

/*
 *  action_confirm_edit – сохраняет изменения из полей ввода
 *  в выбранного студента и выходит из режима редактирования.
 */
static void action_confirm_edit(void) {
    if (!isRecordEditMode) return;
    if (!validate_input()) return;
    save_undo_state();

    Student *editedStudent = displayStudentPointers[selectedTableRowIndex];
    strncpy(editedStudent->specialty, inputSpecialtyText, MAJOR_LEN);
    editedStudent->specialty[MAJOR_LEN] = '\0';
    editedStudent->groupNumber = atoi(inputGroupNumberText);
    strncpy(editedStudent->fullName, inputFullNameText, NAME_LEN);
    editedStudent->fullName[NAME_LEN] = '\0';
    strncpy(editedStudent->educationForm, inputEducationFormText, FORM_LEN);
    editedStudent->educationForm[FORM_LEN] = '\0';
    editedStudent->marks[0] = atoi(inputMarkTexts[0]);
    editedStudent->marks[1] = atoi(inputMarkTexts[1]);
    editedStudent->marks[2] = atoi(inputMarkTexts[2]);
    editedStudent->marks[3] = atoi(inputMarkTexts[3]);
    editedStudent->averageMark = calculateAverageMark(editedStudent->marks);

    isRecordEditMode = 0;
    activeInputFieldIndex = -1;
    apply_filter();
    for (int rowIndex = 0; rowIndex < displayRecordCount; rowIndex++) {
        if (displayStudentPointers[rowIndex] == editedStudent) {
            selectedTableRowIndex = rowIndex;
            break;
        }
    }
    set_status("Изменения сохранены");
}

/*
 *  action_cancel_edit – отменяет редактирование, восстанавливая
 *  исходные данные студента.
 */
static void action_cancel_edit(void) {
    if (!isRecordEditMode) return;
    if (selectedTableRowIndex >= 0 && selectedTableRowIndex < displayRecordCount) {
        Student *editedStudent = displayStudentPointers[selectedTableRowIndex];
        copy_student_data(editedStudent, &editModeBackupRecord);
    }
    isRecordEditMode = 0;
    activeInputFieldIndex = -1;
    clear_input_fields();
    filterEducationFormSelection = 0;
    apply_filter();
    set_status("Редактирование отменено");
}

/*
 *  action_delete – удаляет выбранного студента из списка.
 */
static void action_delete(void) {
    if (selectedTableRowIndex < 0 || selectedTableRowIndex >= displayRecordCount) {
        set_status("Ошибка: выберите студента из таблицы");
        return;
    }
    if (isRecordEditMode) {
        if (selectedTableRowIndex >= 0) copy_student_data(displayStudentPointers[selectedTableRowIndex], &editModeBackupRecord);
        isRecordEditMode = 0;
        activeInputFieldIndex = -1;
    }
    Student *target = displayStudentPointers[selectedTableRowIndex];
    int list_idx = listFind(&mainStudentList, target);
    if (list_idx < 0) { set_status("Ошибка: студент не найден в списке"); return; }
    save_undo_state();
    listRemove(&mainStudentList, list_idx);
    clear_input_fields();
    selectedTableRowIndex = -1;
    activeInputFieldIndex = -1;
    apply_filter();
    set_status("Студент удалён");
}

/*
 *  action_clear – очищает поля ввода и сбрасывает фильтр.
 */
static void action_clear(void) {
    clear_input_fields();
    selectedTableRowIndex = -1;
    activeInputFieldIndex = -1;
    filterEducationFormSelection = 0;
    apply_filter();
    set_status("Фильтр сброшен");
}

/*
 *  action_undo – отменяет последнее действие (добавление,
 *  редактирование, удаление, загрузку).
 */
static void action_undo(void) {
    if (undoHistoryTopIndex < 0) {
        set_status("Нет действий для отмены");
        return;
    }
    listClear(&mainStudentList);
    Student *currentStudent = undoHistoryStacks[undoHistoryTopIndex].head;
    while (currentStudent) {
        Student *studentNode = newStudent(
            currentStudent->specialty, currentStudent->groupNumber,
            currentStudent->fullName, currentStudent->educationForm,
            currentStudent->marks[0], currentStudent->marks[1],
            currentStudent->marks[2], currentStudent->marks[3]);
        if (studentNode) listPushBack(&mainStudentList, studentNode);
        currentStudent = currentStudent->nextStudent;
    }
    listClear(&undoHistoryStacks[undoHistoryTopIndex]);
    undoHistoryTopIndex--;
    clear_input_fields();
    selectedTableRowIndex = -1;
    activeInputFieldIndex = -1;
    filterEducationFormSelection = 0;
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
static int drawButton(int x, int y, int widgetWidth, int widgetHeight,
                      const char *captionText, Color normalColor, Color hoverColor) {
    Rectangle buttonRect = { (float)x, (float)y, (float)widgetWidth, (float)widgetHeight };
    DrawRectangleRounded(buttonRect, 0.15f, 6,
        CheckCollisionPointRec(GetMousePosition(), buttonRect) ? hoverColor : normalColor);
    DrawRectangleRoundedLines(buttonRect, 0.15f, 6, (Color){ 0, 0, 0, 30 });
    DrawTextEx(applicationFont, captionText,
               (Vector2){
                   buttonRect.x + (widgetWidth - MeasureText(captionText, FONT_SIZE_INTERFACE)) / 2.0f,
                   buttonRect.y + (widgetHeight - FONT_SIZE_INTERFACE) / 2.0f,
               },
               FONT_SIZE_INTERFACE, 1, BTN_TEXT);
    return CheckCollisionPointRec(GetMousePosition(), buttonRect) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/*
 *  drawTextBox – рисует поле ввода с меткой.
 *  Показывает текст и курсор, если поле в фокусе.
 *  Возвращает 1 при клике на поле.
 */
static int drawTextBox(int x, int y, int widgetWidth, int widgetHeight,
                       const char *fieldLabel, const char *inputBuffer, int isFieldFocused) {
    int labelWidth = MeasureText(fieldLabel, FONT_SIZE_INTERFACE) + DEFAULT_PADDING - 4;
    DrawTextEx(applicationFont, fieldLabel,
               (Vector2){ (float)x, y + (widgetHeight - FONT_SIZE_INTERFACE) / 2.0f },
               FONT_SIZE_INTERFACE, 1, LABEL_COLOR);
    Rectangle inputRect = { (float)(x + labelWidth), (float)y,
                            (float)(widgetWidth - labelWidth), (float)widgetHeight };
    DrawRectangleRec(inputRect, INPUT_BG);
    DrawRectangleLinesEx(inputRect, isFieldFocused ? 2 : 1,
                         isFieldFocused ? FOCUS_BORDER : INPUT_BORDER);
    const char *visibleText = inputBuffer;
    char truncatedBuffer[128];
    if (MeasureText(visibleText, FONT_SIZE_INTERFACE) > (int)inputRect.width - DEFAULT_PADDING) {
        int byteLength = (int)strlen(visibleText);
        int startOffset = byteLength;
        while (startOffset > 0 &&
               MeasureText(visibleText + startOffset, FONT_SIZE_INTERFACE) >=
               (int)inputRect.width - DEFAULT_PADDING) {
            startOffset--;
        }
        strcpy(truncatedBuffer, visibleText + startOffset);
        visibleText = truncatedBuffer;
    }
    DrawTextEx(applicationFont, visibleText,
               (Vector2){ inputRect.x + 4, y + (widgetHeight - FONT_SIZE_INTERFACE) / 2.0f },
               FONT_SIZE_INTERFACE, 1, TEXT_COLOR);
    if (isFieldFocused && ((int)(GetTime() * 2) % 2 == 0)) {
        float cursorX = inputRect.x + 4 +
                        (float)MeasureText(visibleText, FONT_SIZE_INTERFACE);
        DrawLineV((Vector2){ cursorX, inputRect.y + 4 },
                  (Vector2){ cursorX, inputRect.y + widgetHeight - 4 },
                  (Color){ 30, 60, 180, 200 });
    }
    return CheckCollisionPointRec(GetMousePosition(), inputRect) &&
           IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

/*
 *  drawRadioGroup – рисует группу радиокнопок.
 *  Возвращает индекс выбранного элемента.
 */
static int drawRadioGroup(int x, int y, int widgetHeight,
                          const char *optionLabels[], int optionCount,
                          int selectedOptionIndex) {
    for (int optionIndex = 0, currentX = x; optionIndex < optionCount; optionIndex++) {
        int optionWidth = MeasureText(optionLabels[optionIndex], FONT_SIZE_INTERFACE) + 22;
        Rectangle optionRect = { (float)currentX, (float)y,
                                 (float)optionWidth, (float)widgetHeight };
        DrawCircle(currentX + 10, y + widgetHeight / 2, 7,
                   (optionIndex == selectedOptionIndex) ? RADIO_ACT : RADIO_INACT);
        if (optionIndex == selectedOptionIndex)
            DrawCircle(currentX + 10, y + widgetHeight / 2, 3,
                       (Color){ 255, 255, 255, 255 });
        DrawTextEx(applicationFont, optionLabels[optionIndex],
                   (Vector2){ (float)(currentX + 20), y + (widgetHeight - FONT_SIZE_INTERFACE) / 2.0f },
                   FONT_SIZE_INTERFACE, 1, LABEL_COLOR);
        if (CheckCollisionPointRec(GetMousePosition(), optionRect) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            selectedOptionIndex = optionIndex;
        currentX += optionWidth + 4;
    }
    return selectedOptionIndex;
}

/* Позиции колонок таблицы (сдвинуты на MAIN_CONTENT_OFFSET_X) */
static const int COL_X[] = { 10+MAIN_CONTENT_OFFSET_X, 45+MAIN_CONTENT_OFFSET_X, 115+MAIN_CONTENT_OFFSET_X,
                             410+MAIN_CONTENT_OFFSET_X, 690+MAIN_CONTENT_OFFSET_X, 840+MAIN_CONTENT_OFFSET_X, 970+MAIN_CONTENT_OFFSET_X };
static const int COL_W[] = { 35, 70, 295, 280, 150, 130, 230 };

/*
 *  draw_col_text – рисует текст в колонке таблицы.
 *  При необходимости обрезает текст с добавлением "...",
 *  корректно обрабатывая UTF-8 границы.
 */
static void draw_col_text(int col, int y, int font_sz,
                          const char *text, Color color) {
    Vector2 sz = MeasureTextEx(applicationFont, text, font_sz, 1);
    int max_w = COL_W[col] - 6;
    if (sz.x <= max_w) {
        DrawTextEx(applicationFont, text, (Vector2){ (float)COL_X[col], y },
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
        Vector2 ts = MeasureTextEx(applicationFont, buf, font_sz, 1);
        if (ts.x <= max_w) {
            DrawTextEx(applicationFont, buf, (Vector2){ (float)COL_X[col], y },
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
    DrawRectangle(0, HEADER_Y, WINDOW_WIDTH, ROW_H, HEADER_BG);
    const char *titles[] = {
        "#", "Группа", "ФИО", "Специальность",
        "Форма", "Оценки", "Ср. балл"
    };
    for (int i = 0; i < 7; i++)
        DrawTextEx(applicationFont, titles[i],
                   (Vector2){ (float)COL_X[i], HEADER_Y + (ROW_H - 16) / 2 },
                   16, 1, HEADER_TEXT);
}

/*
 *  draw_row – рисует одну строку таблицы.
 */
static void draw_row(int rowIndex, int rowPositionY, const Student *studentRecord,
                     int isRowSelected) {
    DrawRectangle(0, rowPositionY, WINDOW_WIDTH, ROW_H,
                  isRowSelected ? ROW_SELECTED
                                : ((rowIndex % 2 == 0) ? ROW_EVEN : ROW_ODD));
    char b[7][64];
    snprintf(b[0], sizeof(b[0]), "%d", rowIndex + 1);
    if (isRecordEditMode && isRowSelected) {
        snprintf(b[1], sizeof(b[1]), "%s", inputGroupNumberText);
        strcpy(b[2], inputFullNameText);
        strcpy(b[3], inputSpecialtyText);
        strcpy(b[4], inputEducationFormText);
        int gv[MARK_COUNT];
        for (int i = 0; i < MARK_COUNT; i++) gv[i] = atoi(inputMarkTexts[i]);
        snprintf(b[5], sizeof(b[5]), "%d %d %d %d", gv[0], gv[1], gv[2], gv[3]);
        float avg = (inputMarkTextLengths[0] && inputMarkTextLengths[1] &&
                     inputMarkTextLengths[2] && inputMarkTextLengths[3])
                    ? (gv[0]+gv[1]+gv[2]+gv[3])/4.0f : studentRecord->averageMark;
        snprintf(b[6], sizeof(b[6]), "%.2f", avg);
    } else {
        snprintf(b[1], sizeof(b[1]), "%d", studentRecord->groupNumber);
        strcpy(b[2], studentRecord->fullName);
        strcpy(b[3], studentRecord->specialty);
        strcpy(b[4], studentRecord->educationForm);
        snprintf(b[5], sizeof(b[5]), "%d %d %d %d",
                 studentRecord->marks[0], studentRecord->marks[1],
                 studentRecord->marks[2], studentRecord->marks[3]);
        snprintf(b[6], sizeof(b[6]), "%.2f", studentRecord->averageMark);
    }
    Color cc[] = {
        TEXT_COLOR, TEXT_COLOR, TEXT_COLOR, TEXT_COLOR,
        (strcmp(studentRecord->educationForm, "бюджетная") == 0) ? TITLE_BUDGET : TITLE_PAID,
        TEXT_COLOR, TEXT_COLOR
    };
    for (int columnIndex = 0; columnIndex < 7; columnIndex++)
        draw_col_text(columnIndex, rowPositionY + (ROW_H - 14) / 2, 14,
                      b[columnIndex], cc[columnIndex]);
}

/*
 *  draw_section – рисует полоску-заголовок секции.
 */
static void draw_section(int y, const char *title, Color bg) {
    DrawRectangle(0, y, WINDOW_WIDTH, ROW_H, bg);
    Vector2 sz = MeasureTextEx(applicationFont, title, 15, 1);
    DrawTextEx(applicationFont, title,
               (Vector2){ (WINDOW_WIDTH - sz.x) / 2, y + (ROW_H - 15) / 2 },
               15, 1, (Color){ 255, 255, 255, 255 });
}

/*
 *  draw_table – рисует таблицу студентов с прокруткой.
 */
static void draw_table(void) {
    draw_header();
    BeginScissorMode(0, ROW_Y_START, WINDOW_WIDTH, TABLE_H);
    int y = ROW_Y_START - (int)tableScrollOffset;
    int ri = 0;

    if (currentViewMode == VIEW_BY_FORM) {
        int split = 0;
        for (int i = 0; i < displayRecordCount; i++)
            if (strcmp(displayStudentPointers[i]->educationForm, "платная") == 0) { split = i; break; }
        if (split > 0) {
            draw_section(y, "--- БЮДЖЕТНАЯ ФОРМА ---", TITLE_BUDGET);
            y += ROW_H; ri++;
        }
        for (int i = 0; i < split; i++) {
            if (showOnlyExcellentBudget && !isExcellentBudget(displayStudentPointers[i]))
                continue;
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, displayStudentPointers[i], i == selectedTableRowIndex);
            y += ROW_H; ri++;
        }
        if (split < displayRecordCount) {
            draw_section(y, "--- ПЛАТНАЯ ФОРМА ---", TITLE_PAID);
            y += ROW_H; ri++;
        }
        for (int i = split; i < displayRecordCount; i++) {
            if (showOnlyExcellentBudget && !isExcellentBudget(displayStudentPointers[i]))
                continue;
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, displayStudentPointers[i], i == selectedTableRowIndex);
            y += ROW_H; ri++;
        }
    } else {
        for (int i = 0; i < displayRecordCount; i++) {
            if (showOnlyExcellentBudget && !isExcellentBudget(displayStudentPointers[i]))
                continue;
            if (y + ROW_H >= ROW_Y_START && y < ROW_Y_START + TABLE_H)
                draw_row(ri, y, displayStudentPointers[i], i == selectedTableRowIndex);
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
 *  для активного поля ввода (activeInputFieldIndex). Поддерживает
 *  кириллицу и служебные клавиши.
 */
static void handle_keys(void) {
    if (activeInputFieldIndex < 0) return;
    int changed = 0;
    int c = GetCharPressed();
    while (c > 0) {
        int printable = (c >= 32 && c <= 126) ||
                        (c >= 0x400 && c <= 0x4FF) ||
                        (c >= 0x500 && c <= 0x52F);
        if (printable) {
            if (activeInputFieldIndex == 0)
                { utf8_append(inputSpecialtyText, &inputSpecialtyTextLength, MAJOR_LEN, c); changed = 1; }
            else if (activeInputFieldIndex == 1 && c >= '0' && c <= '9' && inputGroupNumberTextLength < 6)
                { utf8_append(inputGroupNumberText, &inputGroupNumberTextLength, 6, c); changed = 1; }
            else if (activeInputFieldIndex == 2)
                { utf8_append(inputFullNameText, &inputFullNameTextLength, NAME_LEN, c); changed = 1; }
            else if (activeInputFieldIndex == 3)
                { utf8_append(inputEducationFormText, &inputEducationFormTextLength, FORM_LEN, c); changed = 1; }
            else if (activeInputFieldIndex >= 4 && activeInputFieldIndex <= 7) {
                int gi = activeInputFieldIndex - 4;
                if (c >= '0' && c <= '9' && inputMarkTextLengths[gi] < 2)
                    { utf8_append(inputMarkTexts[gi], &inputMarkTextLengths[gi], 2, c); changed = 1; }
            }
        }
        c = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (activeInputFieldIndex == 0) { utf8_pop(inputSpecialtyText, &inputSpecialtyTextLength); changed = 1; }
        else if (activeInputFieldIndex == 1) { utf8_pop(inputGroupNumberText, &inputGroupNumberTextLength); changed = 1; }
        else if (activeInputFieldIndex == 2) { utf8_pop(inputFullNameText, &inputFullNameTextLength); changed = 1; }
        else if (activeInputFieldIndex == 3) { utf8_pop(inputEducationFormText, &inputEducationFormTextLength); changed = 1; }
        else if (activeInputFieldIndex >= 4 && activeInputFieldIndex <= 7)
            { utf8_pop(inputMarkTexts[activeInputFieldIndex - 4], &inputMarkTextLengths[activeInputFieldIndex - 4]); changed = 1; }
    }
    if (changed && !isRecordEditMode) apply_filter();
}

/*
 *  get_row_at_click – определяет индекс строки таблицы
 *  по координате Y мыши с учётом прокрутки.
 *  Возвращает -1 если клик вне области строк.
 */
static int get_row_at_click(int mouse_y) {
    if (mouse_y < ROW_Y_START || mouse_y >= ROW_Y_START + TABLE_H)
        return -1;
    int idx = (mouse_y - ROW_Y_START + (int)tableScrollOffset) / ROW_H;
    /* Учитываем заголовки секций в VIEW_BY_FORM */
    if (currentViewMode == VIEW_BY_FORM) {
        int split = 0;
        for (int i = 0; i < displayRecordCount; i++)
            if (strcmp(displayStudentPointers[i]->educationForm, "платная") == 0) { split = i; break; }
        int visual_idx = idx;
        if (split > 0) visual_idx--;
        if (visual_idx < 0) return -1;
        if (visual_idx < split) return visual_idx;
        visual_idx--;
        if (visual_idx >= 0 && visual_idx < displayRecordCount - split)
            return split + visual_idx;
        return -1;
    }
    return (idx >= 0 && idx < displayRecordCount) ? idx : -1;
}

/* ==========================================================
 *  ГЛАВНАЯ ФУНКЦИЯ
 * ========================================================== */

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);

    /* Важно: фиксируем рабочую директорию на папку exe,
       чтобы все fopen("students.csv") работали одинаково
       при разных конфигурациях запуска. */
    set_workdir_to_exe_dir();

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ведомость об успеваемости студентов");
    SetTargetFPS(60);

    /* Загружаем шрифт с кириллицей */
    {
        int codepoints[192], cp_count = 0;
        for (int i = 32; i < 127; i++) codepoints[cp_count++] = i;
        for (int i = 0x0410; i <= 0x044F; i++) codepoints[cp_count++] = i;
        codepoints[cp_count++] = 0x0401;
        codepoints[cp_count++] = 0x0451;

        applicationFont = LoadFontEx("C:/Windows/Fonts/arial.ttf", 24, codepoints, cp_count);
        SetTextureFilter(applicationFont.texture, TEXTURE_FILTER_BILINEAR);
    }

    /* Загружаем базу из CSV или создаём тестовые данные */
    {
        loadDbCsv(CSV_FILE, &mainStudentList);

        if (mainStudentList.count == 0) {
            struct {
                const char *specialtyText;
                int groupNumber;
                const char *fullNameText;
                const char *educationFormText;
                int mark1, mark2, mark3, mark4;
            } demoRecords[] = {
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
            for (int recordIndex = 0; recordIndex < 10; recordIndex++) {
                Student *studentNode = newStudent(
                    demoRecords[recordIndex].specialtyText,
                    demoRecords[recordIndex].groupNumber,
                    demoRecords[recordIndex].fullNameText,
                    demoRecords[recordIndex].educationFormText,
                    demoRecords[recordIndex].mark1, demoRecords[recordIndex].mark2,
                    demoRecords[recordIndex].mark3, demoRecords[recordIndex].mark4);
                if (studentNode) listPushBack(&mainStudentList, studentNode);
            }
            set_status("Загружены тестовые данные (10 студентов)");
            saveDbCsv(CSV_FILE, &mainStudentList);
        }
    }
    rebuild_display();

    while (!WindowShouldClose()) {
        handle_keys();

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            int extra = (currentViewMode == VIEW_BY_FORM) ? 2 : 0;
            int total_h = (displayRecordCount + extra) * ROW_H;
            int max_scroll = (total_h > TABLE_H) ? total_h - TABLE_H : 0;
            tableScrollOffset -= wheel * 20.0f;
            if (tableScrollOffset < 0) tableScrollOffset = 0;
            if (tableScrollOffset > max_scroll) tableScrollOffset = (float)max_scroll;
        }

        /* Обработка кликов (только вне боковой панели, только на странице таблицы) */
        Vector2 m = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && m.x >= SIDE_PANEL_WIDTH && currentUiPageIndex == PAGE_TABLE) {
            /* Поля редактирования (8 шт.) */
            int inp_x[] = { MAIN_CONTENT_OFFSET_X+10, MAIN_CONTENT_OFFSET_X+260, MAIN_CONTENT_OFFSET_X+380, MAIN_CONTENT_OFFSET_X+670, MAIN_CONTENT_OFFSET_X+808, MAIN_CONTENT_OFFSET_X+858, MAIN_CONTENT_OFFSET_X+908, MAIN_CONTENT_OFFSET_X+958 };
            int inp_w[] = { 240, 110, 280, 130, 46, 46, 46, 46 };
            activeInputFieldIndex = -1;
            for (int i = 0; i < 8; i++) {
                Rectangle r = { (float)inp_x[i], INPUT_Y, (float)inp_w[i], INPUT_H };
                if (CheckCollisionPointRec(m, r)) { activeInputFieldIndex = i; break; }
            }

            /* Клик по строке таблицы */
            if (activeInputFieldIndex < 0) {
                int ri = get_row_at_click((int)m.y);
                if (ri >= 0 && ri < displayRecordCount) {
                    if (isRecordEditMode) action_confirm_edit();
                    selectedTableRowIndex = ri;
                    fill_input_from_student(displayStudentPointers[ri]);
                }
            }

            /* Отмена выделения при клике в пустую область таблицы */
            if (activeInputFieldIndex < 0 && m.y >= ROW_Y_START && m.y < ROW_Y_START + TABLE_H) {
                int ri = get_row_at_click((int)m.y);
                if (ri < 0 || ri >= displayRecordCount) selectedTableRowIndex = -1;
            }
        }

        /* ====================================================
         *  ОТРИСОВКА
         * ==================================================== */

        BeginDrawing();
        ClearBackground(BG_COLOR);

        /* Страница файлов */
        if (currentUiPageIndex == PAGE_FILE) {
            int fx = MAIN_CONTENT_OFFSET_X + 30, fy = PANEL_Y + 10;
            DrawTextEx(applicationFont, "Файловые операции", (Vector2){ (float)fx, (float)fy }, 22, 1, BTN_TEXT);
            fy += 40;
            const char *fnames[] = { "Сохранить CSV","Загрузить CSV","Импорт CSV","Экспорт CSV","Экспорт TXT" };
            const char *fdesc[] = {
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
                        case 0: saveDbCsv(CSV_FILE, &mainStudentList); break;
                        case 1: save_undo_state(); listClear(&mainStudentList); loadDbCsv(CSV_FILE, &mainStudentList); clear_input_fields(); selectedTableRowIndex = -1; apply_filter(); break;
                        case 2: { char fname[MAX_PATH] = ""; if (file_dialog_open(fname, sizeof(fname))) import_csv(fname); } break;
                        case 3: export_csv(EXPORT_CSV_FILE, displayStudentPointers, displayRecordCount); break;
                        case 4: export_txt(EXPORT_TXT_FILE, displayStudentPointers, displayRecordCount); break;
                    }
                }
                DrawTextEx(applicationFont, fdesc[i], (Vector2){ (float)(fx + 220), fy+6 }, 13, 1, LABEL_COLOR);
                fy += BTN_H + 16;
            }
        }

        /* Страница с таблицей */
        if (currentUiPageIndex == PAGE_TABLE) {
            /* Фон панели редактирования */
            Rectangle pr = { (float)MAIN_CONTENT_OFFSET_X+8, (float)PANEL_Y, WINDOW_WIDTH - MAIN_CONTENT_OFFSET_X - 16, PANEL_H };
            DrawRectangleRounded(pr, 0.08f, 6, PANEL_BG);
            DrawRectangleRoundedLines(pr, 0.08f, 6, PANEL_BORDER);

            /* Поля редактирования */
            drawTextBox(MAIN_CONTENT_OFFSET_X+10, INPUT_Y, 240, INPUT_H, "Спец.:", inputSpecialtyText, activeInputFieldIndex == 0);
            drawTextBox(MAIN_CONTENT_OFFSET_X+260, INPUT_Y, 110, INPUT_H, "Груп.:", inputGroupNumberText, activeInputFieldIndex == 1);
            drawTextBox(MAIN_CONTENT_OFFSET_X+380, INPUT_Y, 280, INPUT_H, "ФИО:",   inputFullNameText,  activeInputFieldIndex == 2);
            drawTextBox(MAIN_CONTENT_OFFSET_X+670, INPUT_Y, 130, INPUT_H, "Форма:", inputEducationFormText,  activeInputFieldIndex == 3);

            const char *grade_labels[] = { "1:", "2:", "3:", "4:" };
            int gx = MAIN_CONTENT_OFFSET_X+808;
            for (int i = 0; i < MARK_COUNT; i++) {
                drawTextBox(gx, INPUT_Y, 46, INPUT_H, grade_labels[i],
                        inputMarkTexts[i], activeInputFieldIndex == 4 + i);
                gx += 50;
            }

            const char *form_items[] = { "Все", "Бюджет", "Плат" };
            {
                int old_form = filterEducationFormSelection;
                filterEducationFormSelection = drawRadioGroup(MAIN_CONTENT_OFFSET_X+1010, INPUT_Y, INPUT_H, form_items, 3, filterEducationFormSelection);
                if (filterEducationFormSelection != old_form && !isRecordEditMode) apply_filter();
            }

            draw_table();

            {
                const char *mode = "";
                switch (currentViewMode) {
                    case VIEW_ALL:            mode = "Все студенты"; break;
                    case VIEW_SORTED:         mode = "Сортировка по группам и ФИО"; break;
                    case VIEW_EXCELLENT_PAID: mode = "Отличники (бюджетная форма, ср. балл >= 8.0)"; break;
                    case VIEW_BY_FORM:        mode = "Списки по форме обучения"; break;
                }
                char info[128];
                snprintf(info, sizeof(info), "Режим: %s  |  Показано: %d из %d",
                         mode, displayRecordCount, mainStudentList.count);
                DrawTextEx(applicationFont, info, (Vector2){ (float)MAIN_CONTENT_OFFSET_X, (float)WINDOW_HEIGHT - 22 },
                           14, 1, LABEL_COLOR);

                double elapsed = GetTime() - statusMessageShowTime;
                if (elapsed < 4.0 && statusMessageText[0]) {
                    Color sc = (strncmp(statusMessageText, "Ошибка", 6) == 0) ? STATUS_ERR : STATUS_OK;
                    if (elapsed > 3.0) sc.a = (unsigned char)(255 - (int)((elapsed - 3.0) * 255));
                    Vector2 sz = MeasureTextEx(applicationFont, statusMessageText, 14, 1);
                    DrawTextEx(applicationFont, statusMessageText,
                               (Vector2){ (float)(WINDOW_WIDTH - sz.x - 10), (float)WINDOW_HEIGHT - 22 },
                               14, 1, sc);
                }
            }

        /* Закрываем PAGE_TABLE */
        }

        /* === Боковая панель === */
        DrawRectangle(0, 0, SIDE_PANEL_WIDTH, WINDOW_HEIGHT, SIDE_BG);
        DrawLine(SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, WINDOW_HEIGHT, SIDE_LINE);
        {
            int sy = 8, by;

            /* Nav: Таблица (только на странице файла) */
            if (currentUiPageIndex == PAGE_FILE) {
                if (drawButton(4, sy, SIDE_PANEL_WIDTH-8, BTN_H, "< Таблица",
                        (Color){42,44,52,255}, (Color){55,58,68,255}))
                    currentUiPageIndex = PAGE_TABLE;
                sy += BTN_H + 8;
            }

            /* --- Действия --- */
            {
                int rows = isRecordEditMode ? 3 : 3;
                Rectangle r = { 4, (float)sy, SIDE_PANEL_WIDTH-8, rows*(BTN_H+4)+22 };
                DrawRectangleRounded(r, 0.08f, 6, (Color){ 42, 44, 52, 255 });
                DrawRectangleRoundedLines(r, 0.08f, 6, PANEL_BORDER);
                by = sy + 6;
                DrawTextEx(applicationFont, "ДЕЙСТВИЯ", (Vector2){ 10, by }, 11, 1, LABEL_COLOR);
                by += 18;

                if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Добавить", BTN_ADD, BTN_ADD_HOVER))
                    action_add();
                by += BTN_H + 4;

                if (isRecordEditMode) {
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
            sy += (isRecordEditMode ? 3 : 3)*(BTN_H+4) + 26;

            /* --- Фильтры --- */
            {
                Rectangle r = { 4, (float)sy, SIDE_PANEL_WIDTH-8, 3*(BTN_H+4)+22 };
                DrawRectangleRounded(r, 0.08f, 6, (Color){ 42, 44, 52, 255 });
                DrawRectangleRoundedLines(r, 0.08f, 6, PANEL_BORDER);
                by = sy + 6;
                DrawTextEx(applicationFont, "ФИЛЬТРЫ", (Vector2){ 10, by }, 11, 1, LABEL_COLOR);
                by += 18;
                if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Сортировка", BTN_COLOR, BTN_HOVER))
                    action_sort();
                by += BTN_H + 4;
                if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Отличники", BTN_COLOR, BTN_HOVER))
                    action_excellent_paid();
                by += BTN_H + 4;
                if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "По форме", BTN_COLOR, BTN_HOVER))
                    action_by_form();
            }
            sy += 3*(BTN_H+4) + 26;

            /* Nav: Файл (открывает страницу файлов) */
            {
                Color c = (currentUiPageIndex == PAGE_FILE) ? (Color){55,58,68,255} : (Color){42,44,52,255};
                if (drawButton(4, sy, SIDE_PANEL_WIDTH-8, BTN_H, "> Файл", c, (Color){60,64,75,255}))
                    currentUiPageIndex = PAGE_FILE;
                sy += BTN_H + 8;
            }

            /* --- Система --- */
            {
                Rectangle r = { 4, (float)sy, SIDE_PANEL_WIDTH-8, 2*(BTN_H+4)+22 };
                DrawRectangleRounded(r, 0.08f, 6, (Color){ 42, 44, 52, 255 });
                DrawRectangleRoundedLines(r, 0.08f, 6, PANEL_BORDER);
                by = sy + 6;
                DrawTextEx(applicationFont, "СИСТЕМА", (Vector2){ 10, by }, 11, 1, LABEL_COLOR);
                by += 18;
                if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Очистить", BTN_CLEAR, BTN_CLEAR_HOVER))
                    action_clear();
                by += BTN_H + 4;
                if (drawButton(6, by, SIDE_PANEL_WIDTH-12, BTN_H, "Отменить", BTN_UNDO, BTN_UNDO_HOVER))
                    action_undo();
            }
        }

        EndDrawing();
    }

    /* Автосохранение при выходе */
    saveDbCsv(CSV_FILE, &mainStudentList);

    /* Очистка памяти */
    listClear(&mainStudentList);
    for (int i = 0; i <= undoHistoryTopIndex; i++) listClear(&undoHistoryStacks[i]);
    if (displayStudentPointers) free(displayStudentPointers);

    UnloadFont(applicationFont);
    CloseWindow();
    return 0;
}
