#include "student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USERS_FILE "users.dat"

/* Структура для хранения пары логин/пароль */
typedef struct {
    char username[50];
    char password[50];
} UserRecord;

static UserRecord* g_users = NULL;
static int g_userCount = 0;

/* Внутренние функции для ввода пароля (скрытый вывод) */
static char g_passwordBuffer[256] = "";
static int g_passwordLen = 0;
static int g_loginError = 0;
static char g_errorMessage[128] = "";

static void clearPassword(void) {
    g_passwordBuffer[0] = '\0';
    g_passwordLen = 0;
}

/*
 *  auth_init — загружает users.dat в память.
 */
void auth_init(void) {
    FILE* f = fopen(USERS_FILE, "rb");
    if (!f) return;

    int count;
    if (fread(&count, sizeof(int), 1, f) != 1) { fclose(f); return; }
    if (count <= 0 || count > 1000) { fclose(f); return; }

    g_users = (UserRecord*)malloc(count * sizeof(UserRecord));
    if (!g_users) { fclose(f); return; }

    if (fread(g_users, sizeof(UserRecord), count, f) != (size_t)count) {
        free(g_users);
        g_users = NULL;
        fclose(f);
        return;
    }

    g_userCount = count;
    fclose(f);
}

int userExists(const char* username) {
    for (int i = 0; i < g_userCount; i++)
        if (strcmp(g_users[i].username, username) == 0)
            return 1;
    return 0;
}

int registerUser(const char* username, const char* password) {
    if (!username || !*username || !password || !*password) return 0;
    if (userExists(username)) return 0;
    if (strlen(username) >= 50 || strlen(password) >= 50) return 0;

    UserRecord* newUsers = (UserRecord*)realloc(g_users, (g_userCount + 1) * sizeof(UserRecord));
    if (!newUsers) return 0;
    g_users = newUsers;

    strncpy(g_users[g_userCount].username, username, 49);
    g_users[g_userCount].username[49] = '\0';
    strncpy(g_users[g_userCount].password, password, 49);
    g_users[g_userCount].password[49] = '\0';
    g_userCount++;

    saveUsers();
    return 1;
}

int loginUser(const char* username, const char* password) {
    if (!username || !*username || !password) return 0;
    for (int i = 0; i < g_userCount; i++)
        if (strcmp(g_users[i].username, username) == 0 &&
            strcmp(g_users[i].password, password) == 0)
            return 1;
    return 0;
}

void saveUsers(void) {
    FILE* f = fopen(USERS_FILE, "wb");
    if (!f) return;
    fwrite(&g_userCount, sizeof(int), 1, f);
    fwrite(g_users, sizeof(UserRecord), g_userCount, f);
    fclose(f);
}

/*
 *  drawLoginForm — отрисовывает форму входа/регистрации.
 *  Блокирует главное окно, пока g_authParams.isLoggedIn == 0.
 *  Цветовая схема — тёмная тема в стиле основного интерфейса.
 */
void drawLoginForm(void) {
    int cx = SCRW / 2;
    int cy = SCRH / 2;
    int pw = 360;
    int ph = 300;

    /* Затемнение фона */
    DrawRectangle(0, 0, SCRW, SCRH, BG_COLOR);

    /* Панель */
    Rectangle panel = { (float)(cx - pw/2), (float)(cy - ph/2), (float)pw, (float)ph };
    DrawRectangleRounded(panel, 0.08f, 6, PANEL_BG);
    DrawRectangleRoundedLines(panel, 0.08f, 6, PANEL_BORDER);

    int px = (int)panel.x + 20;
    int py = (int)panel.y + 20;

    const char* title = g_authParams.isRegisterMode
        ? "Регистрация нового пользователя"
        : "Вход в систему";
    Vector2 sz = MeasureTextEx(g_font, title, 20, 1);
    DrawTextEx(g_font, title,
               (Vector2){ (float)(cx - sz.x/2), (float)py }, 20, 1, BTN_TEXT);
    py += 40;

    /* Единая ширина для полей ввода */
    int lw1 = (int)MeasureTextEx(g_font, "Логин:", FONT_SIZE_INTERFACE, 1).x + 6;
    int lw2 = (int)MeasureTextEx(g_font, "Пароль:", FONT_SIZE_INTERFACE, 1).x + 6;
    int labelW = (lw1 > lw2) ? lw1 : lw2;
    int inputW = pw - 40 - labelW;

    /* Поле логина */
    {
        DrawTextEx(g_font, "Логин:",
                   (Vector2){ (float)px, py + (34 - FONT_SIZE_INTERFACE) / 2.0f },
                   FONT_SIZE_INTERFACE, 1, LABEL_COLOR);
        Rectangle inp = { (float)(px + labelW), (float)py, (float)inputW, 34 };
        DrawRectangleRec(inp, INPUT_BG);
        DrawRectangleLinesEx(inp, g_textBox.activeField == 0 ? 2 : 1,
                             g_textBox.activeField == 0 ? FOCUS_BORDER : INPUT_BORDER);
        DrawTextEx(g_font, g_textBox.buffer1,
                   (Vector2){ inp.x + 4, py + (34 - FONT_SIZE_INTERFACE) / 2.0f },
                   FONT_SIZE_INTERFACE, 1, TEXT_COLOR);
        if (g_textBox.activeField == 0 && ((int)(GetTime() * 2) % 2 == 0)) {
            float cx2 = inp.x + 4 + MeasureTextEx(g_font, g_textBox.buffer1, FONT_SIZE_INTERFACE, 1).x;
            DrawLineV((Vector2){ cx2, inp.y + 4 },
                      (Vector2){ cx2, inp.y + 30 },
                      (Color){ 30, 60, 180, 200 });
        }
        if (CheckCollisionPointRec(GetMousePosition(), inp) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            g_textBox.activeField = 0;
    }
    py += 44;

    /* Поле пароля (скрытый ввод) */
    {
        char hidden[256] = "";
        for (int i = 0; i < g_passwordLen; i++)
            hidden[i] = '*';
        hidden[g_passwordLen] = '\0';
        const char* display = g_passwordLen > 0 ? hidden : g_passwordBuffer;

        DrawTextEx(g_font, "Пароль:",
                   (Vector2){ (float)px, py + (34 - FONT_SIZE_INTERFACE) / 2.0f },
                   FONT_SIZE_INTERFACE, 1, LABEL_COLOR);
        Rectangle inp = { (float)(px + labelW), (float)py, (float)inputW, 34 };
        DrawRectangleRec(inp, INPUT_BG);
        DrawRectangleLinesEx(inp, g_textBox.activeField == 1 ? 2 : 1,
                             g_textBox.activeField == 1 ? FOCUS_BORDER : INPUT_BORDER);
        DrawTextEx(g_font, display,
                   (Vector2){ inp.x + 4, py + (34 - FONT_SIZE_INTERFACE) / 2.0f },
                   FONT_SIZE_INTERFACE, 1, TEXT_COLOR);
        if (g_textBox.activeField == 1 && ((int)(GetTime() * 2) % 2 == 0)) {
            float cx2 = inp.x + 4 + MeasureTextEx(g_font, display, FONT_SIZE_INTERFACE, 1).x;
            DrawLineV((Vector2){ cx2, inp.y + 4 },
                      (Vector2){ cx2, inp.y + 30 },
                      (Color){ 30, 60, 180, 200 });
        }
        if (CheckCollisionPointRec(GetMousePosition(), inp) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            g_textBox.activeField = 1;
    }
    py += 50;

    /* Кнопки (одинакового размера, по центру) */
    int btnW = 120;
    if (drawButton(cx - btnW - 5, py, btnW, BTN_H,
                   g_authParams.isRegisterMode ? "Регистрация" : "Войти",
                   BTN_COLOR, BTN_HOVER)) {
        const char* username = g_textBox.buffer1;
        const char* password = g_passwordBuffer;

        if (!username[0] || !password[0]) {
            strcpy(g_errorMessage, "Введите логин и пароль");
            g_loginError = 1;
        } else if (g_authParams.isRegisterMode) {
            if (strlen(password) < 3) {
                strcpy(g_errorMessage, "Пароль должен быть минимум 3 символа");
                g_loginError = 1;
            } else if (registerUser(username, password)) {
                g_authParams.isLoggedIn = 1;
                strncpy(g_authParams.currentUsername, username, 49);
                g_authParams.currentUsername[49] = '\0';
                clearPassword();
                g_textBox.buffer1[0] = '\0';
                g_loginError = 0;
            } else {
                strcpy(g_errorMessage, "Пользователь уже существует");
                g_loginError = 1;
            }
        } else {
            if (loginUser(username, password)) {
                g_authParams.isLoggedIn = 1;
                strncpy(g_authParams.currentUsername, username, 49);
                g_authParams.currentUsername[49] = '\0';
                clearPassword();
                g_textBox.buffer1[0] = '\0';
                g_loginError = 0;
            } else {
                strcpy(g_errorMessage, "Неверный логин или пароль");
                g_loginError = 1;
            }
        }
    }

    /* Кнопка смены режима */
    if (drawButton(cx + 5, py, btnW, BTN_H,
                   g_authParams.isRegisterMode ? "Войти" : "Регистрация",
                   (Color){55,58,68,255}, (Color){70,74,85,255})) {
        g_authParams.isRegisterMode = !g_authParams.isRegisterMode;
        g_loginError = 0;
        g_errorMessage[0] = '\0';
    }

    /* Сообщение об ошибке */
    if (g_loginError && g_errorMessage[0]) {
        Vector2 esz = MeasureTextEx(g_font, g_errorMessage, 16, 1);
        DrawTextEx(g_font, g_errorMessage,
                   (Vector2){ (float)(cx - esz.x/2), (float)(py + 46) },
                   16, 1, STATUS_ERR);
    }

    /* Обработка клавиш для полей логина и пароля */
    int c = GetCharPressed();
    while (c > 0) {
        int printable = (c >= 32 && c <= 126) ||
                        (c >= 0x400 && c <= 0x4FF) ||
                        (c >= 0x500 && c <= 0x52F);
        if (printable) {
            if (g_textBox.activeField == 0) {
                size_t plen = strlen(g_textBox.buffer1);
                if (plen < 49) {
                    char buf[8]; int len = 0;
                    if (c < 0x80) { buf[0] = (char)c; len = 1; }
                    else if (c < 0x800) { buf[0] = (char)(0xC0|(c>>6)); buf[1] = (char)(0x80|(c&0x3F)); len = 2; }
                    else { buf[0] = (char)(0xE0|(c>>12)); buf[1] = (char)(0x80|((c>>6)&0x3F)); buf[2] = (char)(0x80|(c&0x3F)); len = 3; }
                    buf[len] = '\0';
                    strcat(g_textBox.buffer1, buf);
                }
            } else if (g_textBox.activeField == 1 && c >= 32 && c <= 126 && g_passwordLen < 49) {
                g_passwordBuffer[g_passwordLen++] = (char)c;
                g_passwordBuffer[g_passwordLen] = '\0';
            }
        }
        c = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (g_textBox.activeField == 0) {
            size_t plen = strlen(g_textBox.buffer1);
            if (plen > 0) {
                int i = (int)plen - 1;
                while (i > 0 && ((unsigned char)g_textBox.buffer1[i] & 0xC0) == 0x80) i--;
                g_textBox.buffer1[i] = '\0';
            }
        } else if (g_textBox.activeField == 1 && g_passwordLen > 0) {
            g_passwordBuffer[--g_passwordLen] = '\0';
        }
    }

    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ENTER)) {
        g_textBox.activeField = (g_textBox.activeField == 0) ? 1 : 0;
    }
}
