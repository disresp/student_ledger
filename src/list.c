#include "student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 *  createStudent — создаёт новый узел StudentNode.
 *  Копирует все поля, вычисляет gradeCount.
 */
StudentNode* createStudent(const char* fullName, const char* groupNumber,
                           const char* specialty, const char* educationForm,
                           const int* grades, int gradeCount) {
    StudentNode* node = (StudentNode*)malloc(sizeof(StudentNode));
    if (!node) return NULL;

    strncpy(node->student.fullName, fullName, 99);
    node->student.fullName[99] = '\0';
    strncpy(node->student.groupNumber, groupNumber, 19);
    node->student.groupNumber[19] = '\0';
    strncpy(node->student.specialty, specialty, 99);
    node->student.specialty[99] = '\0';
    strncpy(node->student.educationForm, educationForm, 49);
    node->student.educationForm[49] = '\0';

    if (gradeCount > 10) gradeCount = 10;
    node->student.gradeCount = gradeCount;
    for (int i = 0; i < gradeCount; i++)
        node->student.grades[i] = grades[i];

    node->next = NULL;
    return node;
}

void pushBack(StudentNode** head, StudentNode* node) {
    if (!*head) {
        *head = node;
        return;
    }
    StudentNode* cur = *head;
    while (cur->next) cur = cur->next;
    cur->next = node;
}

void pushFront(StudentNode** head, StudentNode* node) {
    node->next = *head;
    *head = node;
}

int removeNode(StudentNode** head, int index) {
    if (index < 0 || !*head) return 0;
    if (index == 0) {
        StudentNode* tmp = *head;
        *head = (*head)->next;
        free(tmp);
        return 1;
    }
    StudentNode* prev = *head;
    for (int i = 0; i < index - 1; i++) {
        if (!prev->next) return 0;
        prev = prev->next;
    }
    if (!prev->next) return 0;
    StudentNode* tmp = prev->next;
    prev->next = tmp->next;
    free(tmp);
    return 1;
}

void clearList(StudentNode** head) {
    StudentNode* cur = *head;
    while (cur) {
        StudentNode* next = cur->next;
        free(cur);
        cur = next;
    }
    *head = NULL;
}

int findNode(StudentNode* head, StudentNode* target) {
    int i = 0;
    while (head) {
        if (head == target) return i;
        head = head->next;
        i++;
    }
    return -1;
}

int countNodes(StudentNode* head) {
    int c = 0;
    while (head) { c++; head = head->next; }
    return c;
}

StudentNode* getNodeAt(StudentNode* head, int index) {
    for (int i = 0; head; i++, head = head->next)
        if (i == index) return head;
    return NULL;
}

/*
 *  reverseList — разворот односвязного списка (три указателя).
 */
void reverseList(StudentNode** head) {
    StudentNode* prev = NULL;
    StudentNode* cur = *head;
    StudentNode* next = NULL;
    while (cur) {
        next = cur->next;
        cur->next = prev;
        prev = cur;
        cur = next;
    }
    *head = prev;
}

/*
 *  calculateAverageGrade — средний балл по всем оценкам.
 */
float calculateAverageGrade(Student s) {
    if (s.gradeCount <= 0) return 0.0f;
    int sum = 0;
    for (int i = 0; i < s.gradeCount; i++)
        sum += s.grades[i];
    return (float)sum / (float)s.gradeCount;
}

/*
 *  isExcellentBudget — отличник на бюджете (ср. балл >= 8.0).
 *  Параметр Student по значению, сравнивает "Бюджет".
 */
int isExcellentBudget(Student student) {
    return strcmp(student.educationForm, "Бюджет") == 0 &&
           calculateAverageGrade(student) >= 8.0f;
}

/*
 *  copyStudentData — копирование данных студента (без next).
 */
void copyStudentData(Student* dst, const Student* src) {
    strncpy(dst->fullName, src->fullName, 99); dst->fullName[99] = '\0';
    strncpy(dst->groupNumber, src->groupNumber, 19); dst->groupNumber[19] = '\0';
    strncpy(dst->specialty, src->specialty, 99); dst->specialty[99] = '\0';
    strncpy(dst->educationForm, src->educationForm, 49); dst->educationForm[49] = '\0';
    dst->gradeCount = src->gradeCount;
    for (int i = 0; i < src->gradeCount && i < 10; i++)
        dst->grades[i] = src->grades[i];
}

/* ============================================================
 *  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ MERGE SORT
 * ============================================================ */

static StudentNode* splitList(StudentNode* head) {
    StudentNode* slow = head;
    StudentNode* fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    StudentNode* mid = slow->next;
    slow->next = NULL;
    return mid;
}

static StudentNode* merge(StudentNode* a, StudentNode* b) {
    StudentNode dummy;
    StudentNode* tail = &dummy;
    dummy.next = NULL;

    while (a && b) {
        float avgA = calculateAverageGrade(a->student);
        float avgB = calculateAverageGrade(b->student);

        int cmp;
        if (fabs(avgA - avgB) > 0.001f)
            cmp = (avgA < avgB) ? -1 : 1;
        else {
            int gA = atoi(a->student.groupNumber);
            int gB = atoi(b->student.groupNumber);
            if (gA != gB) cmp = gA - gB;
            else cmp = stricmp(a->student.fullName, b->student.fullName);
        }

        if (cmp <= 0) { tail->next = a; a = a->next; }
        else          { tail->next = b; b = b->next; }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return dummy.next;
}

static StudentNode* mergeSort(StudentNode* head) {
    if (!head || !head->next) return head;
    StudentNode* mid = splitList(head);
    return merge(mergeSort(head), mergeSort(mid));
}

/*
 *  sortStudents — сортировка списка по возрастанию среднего балла.
 *  При равных баллах — по группе (численно), затем по ФИО.
 */
void sortStudents(StudentNode** head) {
    *head = mergeSort(*head);
}

/* ============================================================
 *  БИНАРНОЕ СОХРАНЕНИЕ / ЗАГРУЗКА (.dat)
 * ============================================================ */

/*
 *  saveToFile — сохраняет список в бинарный .dat файл.
 *  Перед сохранением список разворачивается через reverseList,
 *  чтобы при загрузке порядок не инвертировался.
 */
void saveToFile(StudentNode* head, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) return;

    reverseList(&head);

    int count = countNodes(head);
    fwrite(&count, sizeof(int), 1, f);

    StudentNode* cur = head;
    while (cur) {
        fwrite(&cur->student, sizeof(Student), 1, f);
        cur = cur->next;
    }

    fclose(f);

    reverseList(&head);
}

/*
 *  loadFromFile — загружает список из бинарного .dat файла.
 *  Использует pushFront, чтобы скомпенсировать reverseList в saveToFile.
 */
void loadFromFile(StudentNode** head, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return;

    int count;
    if (fread(&count, sizeof(int), 1, f) != 1) { fclose(f); return; }

    for (int i = 0; i < count; i++) {
        Student s;
        if (fread(&s, sizeof(Student), 1, f) != 1) break;
        StudentNode* node = (StudentNode*)malloc(sizeof(StudentNode));
        if (!node) break;
        node->student = s;
        node->next = NULL;
        pushFront(head, node);
    }

    fclose(f);
}
