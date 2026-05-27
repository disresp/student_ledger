#ifndef STUDENT_H
#define STUDENT_H

#include <raylib.h>
#include <stddef.h>

/* ============================================================
 *  КОНСТАНТЫ
 * ============================================================ */

#define INIT_WIDTH              1400
#define INIT_HEIGHT              700
#define SIDE_PANEL_WIDTH        160
#define MAIN_CONTENT_OFFSET_X   180

#define SCRW GetScreenWidth()
#define SCRH GetScreenHeight()

#define FONT_SIZE_INTERFACE     20
#define DEFAULT_PADDING         10

#define MAX_GRADES              10
#define NAME_LEN               100
#define GROUP_LEN               20
#define SPECIALTY_LEN          100
#define FORM_LEN                50

#define CSV_FILE    "students.csv"
#define EXPORT_CSV_FILE "students_export.csv"
#define EXPORT_TXT_FILE "students_export.txt"

#define UNDO_DEPTH  10

/* Зоны экрана */
#define BTN_Y       10
#define BTN_H       40
#define PANEL_Y     55
#define PANEL_H     94
#define INPUT_Y     (PANEL_Y + 6)
#define INPUT_Y2    (INPUT_Y + INPUT_H + 6)
#define INPUT_H     38

#define HEADER_Y    (PANEL_Y + PANEL_H + 8)
#define ROW_H       26
#define ROW_Y_START (HEADER_Y + ROW_H + 2)
#define TABLE_H     (SCRH - ROW_Y_START - 28)

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

/* ============================================================
 *  СТРУКТУРЫ ДАННЫХ (строго по отчёту)
 * ============================================================ */

/* Базовая структура студента */
typedef struct {
    char fullName[100];
    char groupNumber[20];
    char specialty[100];
    char educationForm[50];
    int  grades[10];
    int  gradeCount;
} Student;

/* Узел динамического списка */
typedef struct StudentNode {
    Student student;
    struct StudentNode* next;
} StudentNode;

/* ============================================================
 *  СТРУКТУРЫ СОСТОЯНИЯ ИНТЕРФЕЙСА
 * ============================================================ */

typedef struct {
    char buffer1[256];
    char buffer2[256];
    char buffer3[256];
    char buffer4[256];
    char buffer5[256];
    char buffer6[256];
    char buffer7[256];
    char buffer8[256];
    char buffer9[256];
    char buffer10[256];
    char buffer11[256];
    char buffer12[256];
    char buffer13[256];
    char buffer14[256];
    char buffer15[256];
    char buffer16[256];
    int  activeField;
} TextBoxState;

typedef struct {
    int  isLoggedIn;
    char currentUsername[50];
    int  isRegisterMode;
} AuthParameters;

typedef struct {
    int  showAddStudent;
    int  isEditMode;
    StudentNode* editingNode;
} EditParameters;

typedef struct {
    char query[256];
    int  searchBy;  /* 0=ФИО, 1=группа, 2=форма */
} SearchParameters;

/* ============================================================
 *  РЕЖИМЫ ПРОСМОТРА
 * ============================================================ */

typedef enum {
    VIEW_ALL,
    VIEW_SORTED,
    VIEW_EXCELLENT_PAID,
    VIEW_BY_FORM,
} ViewMode;

/* ============================================================
 *  СТРАНИЦЫ
 * ============================================================ */

#define PAGE_TABLE  0
#define PAGE_FILE   1

/* ============================================================
 *  ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (определены в main.c)
 * ============================================================ */

extern StudentNode* g_head;
extern TextBoxState g_textBox;
extern AuthParameters g_authParams;
extern EditParameters g_editParams;
extern SearchParameters g_searchParams;
extern int g_currentPage;
extern ViewMode g_currentViewMode;
extern int g_showOnlyExcellentBudget;
extern int g_selectedRowIndex;
extern float g_scrollOffset;
extern StudentNode** g_displayArray;
extern int g_displayCount;
extern char g_statusMsg[128];
extern double g_statusTime;
extern Font g_font;
extern int g_filterForm;
extern StudentNode* g_undoStack[UNDO_DEPTH];
extern int g_undoTop;

/* Позиции колонок таблицы */
extern const int COL_X[7];
extern const int COL_W[7];

/* ============================================================
 *  ПРЕДВАРИТЕЛЬНЫЕ ОБЪЯВЛЕНИЯ (все модули)
 * ============================================================ */

/* --- list.h --- */
StudentNode* createStudent(const char* fullName, const char* groupNumber,
                           const char* specialty, const char* educationForm,
                           const int* grades, int gradeCount);
void pushBack(StudentNode** head, StudentNode* node);
void pushFront(StudentNode** head, StudentNode* node);
int removeNode(StudentNode** head, int index);
void clearList(StudentNode** head);
int findNode(StudentNode* head, StudentNode* target);
int countNodes(StudentNode* head);
StudentNode* getNodeAt(StudentNode* head, int index);
void reverseList(StudentNode** head);
void sortStudents(StudentNode** head);
float calculateAverageGrade(Student s);
int isExcellentBudget(Student student);
void copyStudentData(Student* dst, const Student* src);
void saveToFile(StudentNode* head, const char* filename);
void loadFromFile(StudentNode** head, const char* filename);

/* --- csv.h --- */
void set_workdir_to_exe_dir(void);
void build_full_path(char* out, size_t out_sz, const char* filename);
void saveDbCsv(const char* filename, StudentNode* head);
void loadDbCsv(const char* filename, StudentNode** head);
void import_csv(const char* sourceFileName, StudentNode** head);
void export_csv(const char* exportFileName, StudentNode** displayArray, int recordCount);
void export_txt(const char* exportFileName, StudentNode** displayArray, int recordCount);

/* --- auth.h --- */
void auth_init(void);
int userExists(const char* username);
int registerUser(const char* username, const char* password);
int loginUser(const char* username, const char* password);
void saveUsers(void);
void drawLoginForm(void);

/* --- ui.h --- */
int drawButton(int x, int y, int w, int h, const char* text, Color normal, Color hover);
int drawTextBox(int x, int y, int w, int h, const char* label,
                const char* buffer, int focused);
int drawRadioGroup(int x, int y, int h, const char* labels[], int count, int selected);
void drawColText(int col, int y, int font_sz, const char* text, Color color);
void drawHeader(void);
void drawRow(int idx, int y, const Student* s, int selected);
void drawSection(int y, const char* title, Color bg);
void drawTable(void);
void handleKeys(void);
int getRowAtClick(int mouse_y);
void drawSidebar(void);
void drawFilePage(void);
void drawMainWindow(void);

/* --- actions.h --- */
void rebuildDisplay(void);
void setStatus(const char* text);
void saveUndoState(void);
void action_add(void);
void action_edit_mode(void);
void action_confirm_edit(void);
void action_cancel_edit(void);
void action_delete(void);
void action_clear(void);
void action_undo(void);
void action_sort(void);
void action_excellent_paid(void);
void action_by_form(void);
char* getGradeBuffer(int gi);
void clearInputFields(void);
void fillInputFromStudent(const Student* s);
void apply_filter(void);
int validateInput(void);

#endif /* STUDENT_H */
