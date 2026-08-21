#ifndef DISCOUNT_CTYPE_H
#define DISCOUNT_CTYPE_H

static inline int isdigit(int ch) {
    return ch >= '0' && ch <= '9';
}

static inline int isxdigit(int ch) {
    return isdigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static inline int tolower(int ch) {
    return (ch >= 'A' && ch <= 'Z') ? (ch - 'A' + 'a') : ch;
}

static inline int toupper(int ch) {
    return (ch >= 'a' && ch <= 'z') ? (ch - 'a' + 'A') : ch;
}
static inline int iswspace(WCHAR ch) { return ch==L' '||ch==L'\t'||ch==L'\n'||ch==L'\r'||ch==L'\f'||ch==L'\v'; }

#endif
