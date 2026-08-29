#include <stdint.h>
#include <stddef.h>
#include "wdf_transport.h"
typedef struct {
    uint8_t used;
    uint32_t next_token;
    WDF_TRANSPORT_PACKET packets[WDF_TRANSPORT_QUEUE_DEPTH];
    uint8_t ready[WDF_TRANSPORT_QUEUE_DEPTH];
    void *completion_buffer[WDF_TRANSPORT_QUEUE_DEPTH];
    uint32_t completion_buffer_length[WDF_TRANSPORT_QUEUE_DEPTH];
    uint8_t completed[WDF_TRANSPORT_QUEUE_DEPTH];
    int32_t completion_status[WDF_TRANSPORT_QUEUE_DEPTH];
    uint32_t completion_information[WDF_TRANSPORT_QUEUE_DEPTH];
} WDF_TRANSPORT_ENDPOINT;
static WDF_TRANSPORT_ENDPOINT endpoints[WDF_TRANSPORT_MAX_ENDPOINTS];

uint32_t WudfTransportRegisterEndpoint(void) {
    uint32_t i;
    for (i = 0; i < WDF_TRANSPORT_MAX_ENDPOINTS; ++i) if (!endpoints[i].used) {
        uint32_t slot;
        endpoints[i].used = 1; endpoints[i].next_token = 1;
        for (slot = 0; slot < WDF_TRANSPORT_QUEUE_DEPTH; ++slot) {
            endpoints[i].ready[slot] = 0;
            endpoints[i].completed[slot] = 0;
        }
        return i + 1;
    }
    return 0;
}
int WudfTransportUnregisterEndpoint(uint32_t endpoint) {
    if (!endpoint || endpoint > WDF_TRANSPORT_MAX_ENDPOINTS || !endpoints[endpoint - 1].used) return 0;
    endpoints[endpoint - 1].used = 0; return 1;
}
int WudfTransportSubmit(uint32_t endpoint, uint32_t major_function, uint32_t ioctl_code,
                        const void *data, uint32_t input_length, uint32_t output_length,
                        uint32_t *token) {
    WDF_TRANSPORT_ENDPOINT *queue;
    uint32_t i, length = input_length;
    if (!endpoint || endpoint > WDF_TRANSPORT_MAX_ENDPOINTS || !data || !token) return 0;
    queue = &endpoints[endpoint - 1]; if (!queue->used) return 0;
    if (length > WDF_TRANSPORT_BUFFER_SIZE) length = WDF_TRANSPORT_BUFFER_SIZE;
    for (i = 0; i < WDF_TRANSPORT_QUEUE_DEPTH; ++i) if (!queue->ready[i] && !queue->completed[i]) {
        WDF_TRANSPORT_PACKET *packet = &queue->packets[i]; uint32_t j;
        packet->token = queue->next_token++; packet->major_function = major_function;
        packet->ioctl_code = ioctl_code; packet->input_length = input_length;
        packet->output_length = output_length; packet->data_length = length;
        for (j = 0; j < length; ++j) packet->data[j] = ((const uint8_t *)data)[j];
        queue->completion_buffer[i] = (void *)data;
        queue->completion_buffer_length[i] = output_length;
        queue->ready[i] = 1; *token = packet->token; return 1;
    }
    return 0;
}
int WudfTransportPoll(uint32_t endpoint, WDF_TRANSPORT_PACKET *packet) {
    WDF_TRANSPORT_ENDPOINT *queue; uint32_t i;
    if (!endpoint || endpoint > WDF_TRANSPORT_MAX_ENDPOINTS || !packet) return 0;
    queue = &endpoints[endpoint - 1]; if (!queue->used) return 0;
    for (i = 0; i < WDF_TRANSPORT_QUEUE_DEPTH; ++i) if (queue->ready[i]) {
        *packet = queue->packets[i]; return 1;
    }
    return 0;
}
int WudfTransportComplete(uint32_t endpoint, uint32_t token, int32_t status,
                          uint32_t information, const void *data, uint32_t data_length) {
    WDF_TRANSPORT_ENDPOINT *queue; uint32_t i;
    (void)status; (void)information;
    if (!endpoint || endpoint > WDF_TRANSPORT_MAX_ENDPOINTS) return 0;
    queue = &endpoints[endpoint - 1]; if (!queue->used) return 0;
    for (i = 0; i < WDF_TRANSPORT_QUEUE_DEPTH; ++i)
        if (queue->ready[i] && queue->packets[i].token == token) {
            uint32_t copy_length = data_length;
            uint32_t j;
            if (copy_length > queue->completion_buffer_length[i]) copy_length = queue->completion_buffer_length[i];
            if (copy_length > WDF_TRANSPORT_BUFFER_SIZE) copy_length = WDF_TRANSPORT_BUFFER_SIZE;
            if (queue->completion_buffer[i] != NULL && data != NULL)
                for (j = 0; j < copy_length; ++j) ((uint8_t *)queue->completion_buffer[i])[j] = ((const uint8_t *)data)[j];
            queue->completion_buffer[i] = NULL;
            queue->completion_buffer_length[i] = 0;
            queue->completion_status[i] = status;
            queue->completion_information[i] = information;
            queue->completed[i] = 1;
            queue->ready[i] = 0;
            return 1;
        }
    return 0;
}

int WudfTransportGetCompletion(uint32_t endpoint, uint32_t token,
                               int32_t *status, uint32_t *information) {
    WDF_TRANSPORT_ENDPOINT *queue; uint32_t i;
    if (!endpoint || endpoint > WDF_TRANSPORT_MAX_ENDPOINTS) return 0;
    queue = &endpoints[endpoint - 1]; if (!queue->used) return 0;
    for (i = 0; i < WDF_TRANSPORT_QUEUE_DEPTH; ++i)
        if (queue->completed[i] && queue->packets[i].token == token) {
            if (status) *status = queue->completion_status[i];
            if (information) *information = queue->completion_information[i];
            queue->completed[i] = 0;
            return 1;
        }
    return 0;
}
