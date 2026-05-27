#include "student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ============================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ РАБОТЫ С БУФЕРАМИ
 * ============================================================ */

char* getGradeBuffer(int gi) {
    switch (gi) {
        case 0: return g_textBox.buffer5;
        case 1: return g_textBox.buffer6;
        case 2: return g_textBox.buffer7;
        case 3: return g_textBox.buffer8;
        case 4: return g_textBox.buffer9;
        case 5: return g_textBox.buffer10;
        case 6: return g_textBox.buffer11;
        case 7: return g_textBox.buffer12;
        case 8: return g_textBox.buffer13;
        case 9: return g_textBox.buffer14;
        default: return NULL;
    }
}

/* Предварительные объявления */
static int cmp_group_name(const void* a, const void* b);
static int cmp_avg_asc(const void* a, const void* b);

/* ============================================================
 *  СТАТУС И UNDO
 * ============================================================ */

void setStatus(const char* text) {
    strncpy(g_statusMsg, text, sizeof(g_statusMsg) - 1);
    g_statusMsg[sizeof(g_statusMsg) - 1] = '\0';
    g_statusTime = GetTime();
}

void saveUndoState(void) {
    if (g_undoTop < UNDO_DEPTH - 1) {
        g_undoTop++;
    } else {
        clearList(&g_undoStack[0]);
        for (int i = 0; i < UNDO_DEPTH - 1; i++)
            g_undoStack[i] = g_undoStack[i + 1];
        g_undoStack[UNDO_DEPTH - 1] = NULL;
    }
    clearList(&g_undoStack[g_undoTop]);
    StudentNode* cur = g_head;
    while (cur) {
        StudentNode* node = createStudent(cur->student.fullName,
                                          cur->student.groupNumber,
                                          cur->student.specialty,
                                          cur->student.educationForm,
                                          cur->student.grades,
                                          cur->student.gradeCount);
        if (node) pushBack(&g_undoStack[g_undoTop], node);
        cur = cur->next;
    }
}

/* ============================================================
 *  str_icontains
 * ============================================================ */

int str_icontains(const char* str, const char* substr) {
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

/* ============================================================
 *  ФИЛЬТР
 * ============================================================ */

static int matches_filter(const Student* s) {
    if (strlen(g_textBox.buffer1) > 0 &&
        !str_icontains(s->specialty, g_textBox.buffer1))
        return 0;
    if (strlen(g_textBox.buffer2) > 0 &&
        strcmp(s->groupNumber, g_textBox.buffer2) != 0)
        return 0;
    if (strlen(g_textBox.buffer3) > 0 &&
        !str_icontains(s->fullName, g_textBox.buffer3))
        return 0;
    if (strlen(g_textBox.buffer4) > 0 &&
        strcmp(s->educationForm, g_textBox.buffer4) != 0)
        return 0;
    if (g_filterForm == 1 && strcmp(s->educationForm, "Бюджет") != 0)
        return 0;
    if (g_filterForm == 2 && strcmp(s->educationForm, "Платное") != 0)
        return 0;
    return 1;
}

void rebuildDisplay(void) {
    if (g_displayArray) { free(g_displayArray); g_displayArray = NULL; }
    g_displayCount = 0;
    g_selectedRowIndex = -1;

    StudentNode* cur = g_head;
    while (cur) {
        if (matches_filter(&cur->student)) g_displayCount++;
        cur = cur->next;
    }

    if (g_displayCount > 0) {
        g_displayArray = (StudentNode**)malloc(g_displayCount * sizeof(StudentNode*));
        int di = 0;
        cur = g_head;
        while (cur) {
            if (matches_filter(&cur->student))
                g_displayArray[di++] = cur;
            cur = cur->next;
        }
    }

    g_scrollOffset = 0.0f;
}

void apply_filter(void) {
    rebuildDisplay();
}

/* ============================================================
 *  ПОЛЯ ВВОДА
 * ============================================================ */

void clearInputFields(void) {
    g_textBox.buffer1[0] = '\0';
    g_textBox.buffer2[0] = '\0';
    g_textBox.buffer3[0] = '\0';
    g_textBox.buffer4[0] = '\0';
    for (int i = 0; i < MAX_GRADES; i++) {
        char* buf = getGradeBuffer(i);
        if (buf) buf[0] = '\0';
    }
    g_filterForm = 0;
}

void fillInputFromStudent(const Student* s) {
    strncpy(g_textBox.buffer1, s->specialty, 99); g_textBox.buffer1[99] = '\0';
    strncpy(g_textBox.buffer2, s->groupNumber, 19); g_textBox.buffer2[19] = '\0';
    strncpy(g_textBox.buffer3, s->fullName, 99); g_textBox.buffer3[99] = '\0';
    strncpy(g_textBox.buffer4, s->educationForm, 49); g_textBox.buffer4[49] = '\0';
    for (int i = 0; i < MAX_GRADES; i++) {
        char* buf = getGradeBuffer(i);
        if (buf) {
            if (i < s->gradeCount)
                snprintf(buf, 4, "%d", s->grades[i]);
            else
                buf[0] = '\0';
        }
    }
}

int validateInput(void) {
    if (strlen(g_textBox.buffer1) == 0) { setStatus("Ошибка: введите специальность"); return 0; }
    if (strlen(g_textBox.buffer3) == 0) { setStatus("Ошибка: введите ФИО"); return 0; }
    if (strlen(g_textBox.buffer2) == 0) { setStatus("Ошибка: введите номер группы"); return 0; }
    if (strlen(g_textBox.buffer4) == 0) { setStatus("Ошибка: введите форму обучения"); return 0; }
    if (strcmp(g_textBox.buffer4, "Бюджет") != 0 && strcmp(g_textBox.buffer4, "Платное") != 0) {
        setStatus("Ошибка: форма обучения — 'Бюджет' или 'Платное'");
        return 0;
    }
    int hasGrade = 0;
    for (int i = 0; i < MAX_GRADES; i++) {
        char* buf = getGradeBuffer(i);
        if (buf && buf[0]) {
            hasGrade = 1;
            int v = atoi(buf);
            if (v < 1 || v > 10) {
                setStatus("Ошибка: оценки должны быть от 1 до 10");
                return 0;
            }
        }
    }
    if (!hasGrade) { setStatus("Ошибка: введите хотя бы одну оценку"); return 0; }
    return 1;
}

/* ============================================================
 *  ДЕЙСТВИЯ
 * ============================================================ */

void action_add(void) {
    if (!validateInput()) return;
    saveUndoState();

    int grades[10] = {0};
    int gc = 0;
    for (int i = 0; i < MAX_GRADES; i++) {
        char* buf = getGradeBuffer(i);
        if (buf && buf[0]) grades[gc++] = atoi(buf);
    }

    StudentNode* node = createStudent(g_textBox.buffer3, g_textBox.buffer2,
                                       g_textBox.buffer1, g_textBox.buffer4,
                                       grades, gc);
    if (!node) { setStatus("Ошибка: не удалось выделить память"); return; }
    pushBack(&g_head, node);
    clearInputFields();
    g_textBox.activeField = -1;
    rebuildDisplay();
    setStatus("Студент добавлен");
}

void action_edit_mode(void) {
    if (g_selectedRowIndex < 0 || g_selectedRowIndex >= g_displayCount) {
        setStatus("Ошибка: выберите студента из таблицы");
        return;
    }
    g_editParams.editingNode = g_displayArray[g_selectedRowIndex];
    fillInputFromStudent(&g_editParams.editingNode->student);
    g_editParams.isEditMode = 1;
    g_textBox.activeField = 0;
    setStatus("Режим редактирования — изменения видны в таблице");
}

void action_confirm_edit(void) {
    if (!g_editParams.isEditMode) return;
    if (!validateInput()) return;
    saveUndoState();

    Student* s = &g_editParams.editingNode->student;
    strncpy(s->specialty, g_textBox.buffer1, 99); s->specialty[99] = '\0';
    strncpy(s->groupNumber, g_textBox.buffer2, 19); s->groupNumber[19] = '\0';
    strncpy(s->fullName, g_textBox.buffer3, 99); s->fullName[99] = '\0';
    strncpy(s->educationForm, g_textBox.buffer4, 49); s->educationForm[49] = '\0';
    s->gradeCount = 0;
    for (int i = 0; i < MAX_GRADES; i++) {
        char* buf = getGradeBuffer(i);
        if (buf && buf[0]) s->grades[s->gradeCount++] = atoi(buf);
    }

    g_editParams.isEditMode = 0;
    g_textBox.activeField = -1;
    rebuildDisplay();

    for (int i = 0; i < g_displayCount; i++) {
        if (g_displayArray[i] == g_editParams.editingNode) {
            g_selectedRowIndex = i;
            break;
        }
    }
    setStatus("Изменения сохранены");
}

void action_cancel_edit(void) {
    if (!g_editParams.isEditMode) return;
    g_editParams.isEditMode = 0;
    g_textBox.activeField = -1;
    clearInputFields();
    rebuildDisplay();
    setStatus("Редактирование отменено");
}

void action_delete(void) {
    if (g_selectedRowIndex < 0 || g_selectedRowIndex >= g_displayCount) {
        setStatus("Ошибка: выберите студента из таблицы");
        return;
    }
    if (g_editParams.isEditMode) {
        g_editParams.isEditMode = 0;
        g_textBox.activeField = -1;
    }

    StudentNode* target = g_displayArray[g_selectedRowIndex];
    int listIdx = findNode(g_head, target);
    if (listIdx < 0) { setStatus("Ошибка: студент не найден в списке"); return; }

    saveUndoState();
    removeNode(&g_head, listIdx);
    clearInputFields();
    g_selectedRowIndex = -1;
    g_textBox.activeField = -1;
    rebuildDisplay();
    setStatus("Студент удалён");
}

void action_clear(void) {
    clearInputFields();
    g_selectedRowIndex = -1;
    g_textBox.activeField = -1;
    g_filterForm = 0;
    g_showOnlyExcellentBudget = 0;
    g_currentViewMode = VIEW_ALL;
    rebuildDisplay();
    setStatus("Фильтр сброшен");
}

void action_undo(void) {
    if (g_undoTop < 0) {
        setStatus("Нет действий для отмены");
        return;
    }
    clearList(&g_head);
    StudentNode* cur = g_undoStack[g_undoTop];
    while (cur) {
        StudentNode* node = createStudent(cur->student.fullName,
                                          cur->student.groupNumber,
                                          cur->student.specialty,
                                          cur->student.educationForm,
                                          cur->student.grades,
                                          cur->student.gradeCount);
        if (node) pushBack(&g_head, node);
        cur = cur->next;
    }
    clearList(&g_undoStack[g_undoTop]);
    g_undoTop--;
    clearInputFields();
    g_selectedRowIndex = -1;
    g_textBox.activeField = -1;
    g_filterForm = 0;
    rebuildDisplay();
    setStatus("Действие отменено");
}

/* ============================================================
 *  ФУНКЦИИ СРАВНЕНИЯ
 * ============================================================ */

static int cmp_group_name(const void* a, const void* b) {
    const StudentNode* na = *(const StudentNode**)a;
    const StudentNode* nb = *(const StudentNode**)b;
    int ga = atoi(na->student.groupNumber);
    int gb = atoi(nb->student.groupNumber);
    if (ga != gb) return ga - gb;
    return stricmp(na->student.fullName, nb->student.fullName);
}

static int cmp_avg_asc(const void* a, const void* b) {
    const StudentNode* na = *(const StudentNode**)a;
    const StudentNode* nb = *(const StudentNode**)b;
    float aa = calculateAverageGrade(na->student);
    float ab = calculateAverageGrade(nb->student);
    if (fabs(aa - ab) > 0.001f) return (aa < ab) ? -1 : 1;
    return stricmp(na->student.fullName, nb->student.fullName);
}

void action_sort(void) {
    rebuildDisplay();
    if (g_displayCount > 0)
        qsort(g_displayArray, g_displayCount, sizeof(StudentNode*), cmp_avg_asc);
    g_currentViewMode = VIEW_SORTED;
    g_scrollOffset = 0.0f;
    g_selectedRowIndex = -1;
}

void action_excellent_paid(void) {
    g_showOnlyExcellentBudget = !g_showOnlyExcellentBudget;
    g_currentViewMode = g_showOnlyExcellentBudget ? VIEW_EXCELLENT_PAID : VIEW_ALL;
    g_scrollOffset = 0.0f;
    g_selectedRowIndex = -1;
    setStatus(g_showOnlyExcellentBudget
               ? "Фильтр: отличники бюджетной формы (ср. балл >= 8.0)"
               : "Фильтр отличников отключён");
}

void action_by_form(void) {
    int budgetCount = 0, paidCount = 0;
    StudentNode* cur = g_head;
    while (cur) {
        if (strcmp(cur->student.educationForm, "Бюджет") == 0) budgetCount++;
        else paidCount++;
        cur = cur->next;
    }

    StudentNode** budget = budgetCount > 0
        ? (StudentNode**)malloc(budgetCount * sizeof(StudentNode*)) : NULL;
    StudentNode** paid = paidCount > 0
        ? (StudentNode**)malloc(paidCount * sizeof(StudentNode*)) : NULL;

    budgetCount = 0; paidCount = 0;
    cur = g_head;
    while (cur) {
        if (strcmp(cur->student.educationForm, "Бюджет") == 0)
            budget[budgetCount++] = cur;
        else
            paid[paidCount++] = cur;
        cur = cur->next;
    }

    if (budget) qsort(budget, budgetCount, sizeof(StudentNode*), cmp_avg_asc);
    if (paid)   qsort(paid, paidCount, sizeof(StudentNode*), cmp_avg_asc);

    if (g_displayArray) free(g_displayArray);
    g_displayCount = budgetCount + paidCount;
    g_displayArray = (StudentNode**)malloc(g_displayCount * sizeof(StudentNode*));
    int di = 0;
    for (int i = 0; i < budgetCount; i++) g_displayArray[di++] = budget[i];
    for (int i = 0; i < paidCount; i++) g_displayArray[di++] = paid[i];

    free(budget);
    free(paid);

    g_currentViewMode = VIEW_BY_FORM;
    g_scrollOffset = 0.0f;
    g_selectedRowIndex = -1;
}
