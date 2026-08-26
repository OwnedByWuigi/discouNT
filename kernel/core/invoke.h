#ifndef DISCOUNT_INVOKE_H
#define DISCOUNT_INVOKE_H
#include <stdint.h>
int KeInvokeMain(void *entry, void *stack, uint32_t stack_size);
int KeInvokeMainArgs(void *entry, const char *image_path, const char *command_line,
                     void *stack, uint32_t stack_size);
int KeInvokeWMain(void *entry, const char *image_path, const uint16_t *command_line,
                  void *stack, uint32_t stack_size);
int KeInvokeWinMain(void *entry, void *image, const char *command_line,
                    void *stack, uint32_t stack_size);
int KeInvokeWWinMain(void *entry, void *image, const uint32_t *command_line,
                     void *stack, uint32_t stack_size);
#endif
