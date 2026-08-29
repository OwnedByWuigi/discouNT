#ifndef DISCOUNT_WDF_TRANSPORT_H
#define DISCOUNT_WDF_TRANSPORT_H
#include <stdint.h>
#define WDF_TRANSPORT_MAX_ENDPOINTS 16u
#define WDF_TRANSPORT_QUEUE_DEPTH 16u
#define WDF_TRANSPORT_BUFFER_SIZE 4096u
typedef struct _WDF_TRANSPORT_PACKET {
    uint32_t token, major_function, ioctl_code, input_length, output_length, data_length;
    uint8_t data[WDF_TRANSPORT_BUFFER_SIZE];
} WDF_TRANSPORT_PACKET;
uint32_t WudfTransportRegisterEndpoint(void);
int WudfTransportUnregisterEndpoint(uint32_t endpoint);
int WudfTransportSubmit(uint32_t endpoint, uint32_t major_function, uint32_t ioctl_code,
                        const void *data, uint32_t input_length, uint32_t output_length,
                        uint32_t *token);
int WudfTransportPoll(uint32_t endpoint, WDF_TRANSPORT_PACKET *packet);
int WudfTransportComplete(uint32_t endpoint, uint32_t token, int32_t status,
                          uint32_t information, const void *data, uint32_t data_length);
int WudfTransportGetCompletion(uint32_t endpoint, uint32_t token,
                               int32_t *status, uint32_t *information);
#endif
