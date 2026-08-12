#include <windows.h>

int __cdecl wmain(int argc, WCHAR **argv);

int main(void) {
    WCHAR *argv[1] = { 0 };
    return wmain(0, argv);
}
