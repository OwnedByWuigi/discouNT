#ifndef DISCOUNT_DIALOG_H
#define DISCOUNT_DIALOG_H

#include "windows.h"

typedef struct discount_dialog_control {
    LPCWSTR class_name;
    LPCWSTR title;
    DWORD style;
    DWORD exstyle;
    UINT id;
    int x, y, w, h;
} DISCOUNT_DIALOG_CONTROL;

typedef struct discount_dialog_template {
    UINT id;
    LPCWSTR caption;
    DWORD style;
    int width, height;
    const DISCOUNT_DIALOG_CONTROL *controls;
    int control_count;
} DISCOUNT_DIALOG_TEMPLATE;

BOOL WINAPI User32RegisterDialogTemplate(const DISCOUNT_DIALOG_TEMPLATE *tmpl);

#endif
