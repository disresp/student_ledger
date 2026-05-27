#include "student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windef.h>
#include <winbase.h>

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

#define OFN_PATHMUSTEXIST  0x00000800
#define OFN_FILEMUSTEXIST  0x00001000
#define OFN_HIDEREADONLY   0x00000004

__declspec(dllimport) int __stdcall GetOpenFileNameA(OPENFILENAMEA*);
#endif

static char g_exe_dir[MAX_PATH] = "";

/*
 *  set_workdir_to_exe_dir — привязывает CWD к папке exe-файла.
 */
void set_workdir_to_exe_dir(void) {
#ifdef _WIN32
    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, (DWORD)sizeof(exe_path));
    if (len == 0 || len >= sizeof(exe_path)) return;

    char* last = strrchr(exe_path, '\\');
    char* last_f = strrchr(exe_path, '/');
    if (last_f && (!last || last_f > last)) last = last_f;
    if (!last) return;
    *last = '\0';

    strncpy(g_exe_dir, exe_path, MAX_PATH - 1);
    g_exe_dir[MAX_PATH - 1] = '\0';
    SetCurrentDirectoryA(exe_path);
#endif
}

/*
 *  build_full_path — если путь относительный, добавляет g_exe_dir.
 */
void build_full_path(char* out, size_t out_sz, const char* filename) {
    if (g_exe_dir[0] && filename[0] &&
        filename[0] != '/' && filename[0] != '\\' &&
        !(filename[0] && filename[1] == ':')) {
        snprintf(out, out_sz, "%s\\%s", g_exe_dir, filename);
    } else {
        strncpy(out, filename, out_sz - 1);
        out[out_sz - 1] = '\0';
    }
}

/*
 *  file_dialog_open — диалог выбора CSV-файла (WinAPI).
 */
int file_dialog_open(char* out, size_t out_sz) {
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

/* ============================================================
 *  ПАРСИНГ ДЕМО-ДАННЫХ
 * ============================================================ */

/*
 *  parseGradesFromMarks — преобразует 4 целочисленные оценки
 *  в массив grades[10] с gradeCount=4 (для совместимости со старым CSV).
 */
static void parseGradesFromMarks(int m1, int m2, int m3, int m4, int* grades, int* gradeCount) {
    grades[0] = m1;
    grades[1] = m2;
    grades[2] = m3;
    grades[3] = m4;
    *gradeCount = 4;
}

/* ============================================================
 *  CSV ФУНКЦИИ
 * ============================================================ */

/*
 *  saveDbCsv — сохраняет базу в CSV.
 *  Формат: специальность;группа;ФИО;форма;оценка1;...;оценка10
 */
void saveDbCsv(const char* filename, StudentNode* head) {
    char full[MAX_PATH];
    build_full_path(full, sizeof(full), filename);
    FILE* f = fopen(full, "w");
    if (!f) { setStatus("Ошибка: не удалось открыть файл для записи"); return; }

    StudentNode* cur = head;
    while (cur) {
        fprintf(f, "%s;%s;%s;%s", cur->student.specialty, cur->student.groupNumber,
                cur->student.fullName, cur->student.educationForm);
        for (int i = 0; i < cur->student.gradeCount; i++)
            fprintf(f, ";%d", cur->student.grades[i]);
        fprintf(f, "\n");
        cur = cur->next;
    }
    fclose(f);
    setStatus("База сохранена в students.csv");
}

/*
 *  loadDbCsv — загружает базу из CSV.
 *  Старый формат: спец;группа(число);ФИО;форма;оц1;оц2;оц3;оц4
 *  Новый формат: спец;группа(строка);ФИО;форма;оц1;...;оц10
 */
void loadDbCsv(const char* filename, StudentNode** head) {
    char full[MAX_PATH];
    build_full_path(full, sizeof(full), filename);
    FILE* f = fopen(full, "r");
    if (!f) { setStatus("Файл students.csv не найден, создан пустой список"); return; }

    char line[512];
    int loaded = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        char* ptr = line;
        if ((unsigned char)ptr[0] == 0xEF && (unsigned char)ptr[1] == 0xBB &&
            (unsigned char)ptr[2] == 0xBF) ptr += 3;

        char* fields[15];
        int fc = 0;
        fields[fc++] = ptr;
        while (*ptr && fc < 15) {
            if (*ptr == ';') { *ptr++ = '\0'; fields[fc++] = ptr; }
            else ptr++;
        }

        if (fc < 5) continue;

        char* specialtyField = fields[0];
        char* groupField = fields[1];
        char* fullNameField = fields[2];
        char* educationFormField = fields[3];

        int grades[10];
        int gc = fc - 4;
        if (gc > 10) gc = 10;
        for (int i = 0; i < gc; i++)
            grades[i] = atoi(fields[4 + i]);

        StudentNode* node = createStudent(fullNameField, groupField, specialtyField,
                                           educationFormField, grades, gc);
        if (node) { pushBack(head, node); loaded++; }
    }
    fclose(f);

    char msg[64];
    snprintf(msg, sizeof(msg), "Загружено %d записей из students.csv", loaded);
    setStatus(msg);
}

/*
 *  import_csv — импорт из произвольного CSV-файла с добавлением к списку.
 */
void import_csv(const char* sourceFileName, StudentNode** head) {
    FILE* f = fopen(sourceFileName, "r");
    if (!f) { setStatus("Ошибка: не удалось открыть файл для импорта"); return; }

    saveUndoState();

    char line[512];
    int imported = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;

        char* ptr = line;
        if ((unsigned char)ptr[0] == 0xEF && (unsigned char)ptr[1] == 0xBB &&
            (unsigned char)ptr[2] == 0xBF) ptr += 3;

        char* fields[15];
        int fc = 0;
        fields[fc++] = ptr;
        while (*ptr && fc < 15) {
            if (*ptr == ';') { *ptr++ = '\0'; fields[fc++] = ptr; }
            else ptr++;
        }

        if (fc < 5) continue;

        int grades[10];
        int gc = fc - 4;
        if (gc > 10) gc = 10;
        for (int i = 0; i < gc; i++)
            grades[i] = atoi(fields[4 + i]);

        StudentNode* node = createStudent(fields[2], fields[1], fields[0],
                                           fields[3], grades, gc);
        if (node) { pushBack(head, node); imported++; }
    }
    fclose(f);

    clearInputFields();
    g_selectedRowIndex = -1;
    g_textBox.activeField = -1;
    rebuildDisplay();

    char msg[64];
    snprintf(msg, sizeof(msg), "Импортировано %d записей", imported);
    setStatus(msg);
}

/*
 *  export_csv — экспорт отображаемого списка в CSV.
 */
void export_csv(const char* exportFileName, StudentNode** displayArray, int recordCount) {
    FILE* f = fopen(exportFileName, "w");
    if (!f) { setStatus("Ошибка: не удалось создать файл для экспорта"); return; }

    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);

    for (int i = 0; i < recordCount; i++) {
        Student* s = &displayArray[i]->student;
        fprintf(f, "%s;%s;%s;%s", s->specialty, s->groupNumber, s->fullName, s->educationForm);
        for (int g = 0; g < s->gradeCount; g++)
            fprintf(f, ";%d", s->grades[g]);
        fprintf(f, "\n");
    }
    fclose(f);

    char msg[64];
    snprintf(msg, sizeof(msg), "Экспортировано %d записей в %s", recordCount, exportFileName);
    setStatus(msg);
}

/*
 *  export_txt — экспорт отчёта в текстовый файл.
 */
void export_txt(const char* exportFileName, StudentNode** displayArray, int recordCount) {
    FILE* f = fopen(exportFileName, "w");
    if (!f) { setStatus("Ошибка: не удалось создать файл для экспорта"); return; }

    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, f);

    fprintf(f, "%-4s %-8s %-30s %-30s %-12s %-30s %s\n",
            "№", "Группа", "ФИО", "Специальность", "Форма", "Оценки", "Ср.балл");
    for (int i = 0; i < 115; i++) fputc('-', f);
    fputc('\n', f);

    for (int i = 0; i < recordCount; i++) {
        Student* s = &displayArray[i]->student;

        char gradesText[64] = "";
        for (int g = 0; g < s->gradeCount; g++) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%s%d", g > 0 ? " " : "", s->grades[g]);
            strcat(gradesText, buf);
        }

        fprintf(f, "%-4d %-8s %-30s %-30s %-12s %-30s %.2f\n",
                i + 1, s->groupNumber, s->fullName, s->specialty,
                s->educationForm, gradesText, calculateAverageGrade(*s));
    }
    fclose(f);

    char msg[64];
    snprintf(msg, sizeof(msg), "Отчёт экспортирован в %s", exportFileName);
    setStatus(msg);
}
