#include "usb_msc.h"
#include "util.h"
#include "serial.h"

#define MSC_CBW_SIGNATURE 0x43425355U
#define MSC_CSW_SIGNATURE 0x53425355U
#define SCSI_READ_CAPACITY_10 0x25
#define SCSI_READ_10 0x28

typedef struct __attribute__((packed)) {
    uint32_t signature, tag, transfer_length;
    uint8_t flags, lun, command_length, command[16];
} MSC_CBW;
typedef struct __attribute__((packed)) {
    uint32_t signature, tag, residue;
    uint8_t status;
} MSC_CSW;
typedef struct {
    void *context;
    USB_MSC_BULK_TRANSFER transfer;
    uint8_t bulk_in, bulk_out;
    uint32_t block_size, block_count, next_tag;
} MSC_DEVICE;

static IO_DRIVER_OBJECT *msc_driver;
static uint32_t disk_count;
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void put_be32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}

static int bot_command(MSC_DEVICE*d,const uint8_t*cdb,uint8_t cdb_length,
                       void*data,uint32_t length,int input) {
    MSC_CBW cbw; MSC_CSW csw;
    memset(&cbw,0,sizeof(cbw)); cbw.signature=MSC_CBW_SIGNATURE;
    cbw.tag=++d->next_tag; cbw.transfer_length=length;
    cbw.flags=input?0x80:0; cbw.command_length=cdb_length;
    memcpy(cbw.command,cdb,cdb_length);
    if(!d->transfer(d->context,d->bulk_out,&cbw,sizeof(cbw)))return 0;
    if(length&&!d->transfer(d->context,input?d->bulk_in:d->bulk_out,data,length))return 0;
    if(!d->transfer(d->context,d->bulk_in,&csw,sizeof(csw)))return 0;
    return csw.signature==MSC_CSW_SIGNATURE&&csw.tag==cbw.tag&&csw.status==0;
}

static int msc_read(IO_DEVICE_OBJECT*device,IO_REQUEST*request) {
    MSC_DEVICE*d=(MSC_DEVICE*)device->device_extension;
    uint64_t offset=request->parameters.read_write.offset;
    if(!request->buffer||!request->length||offset%d->block_size||request->length%d->block_size)
        return IO_STATUS_INVALID_PARAMETER;
    uint32_t lba=(uint32_t)(offset/d->block_size),blocks=request->length/d->block_size;
    if(lba>=d->block_count||blocks>d->block_count-lba||blocks>0xFFFF)return IO_STATUS_INVALID_PARAMETER;
    uint8_t cdb[10]={SCSI_READ_10,0,0,0,0,0,0,0,0,0};
    put_be32(cdb+2,lba);cdb[7]=blocks>>8;cdb[8]=blocks;
    if(!bot_command(d,cdb,10,request->buffer,request->length,1))return IO_STATUS_DEVICE_ERROR;
    request->io_status.information=request->length;return IO_STATUS_SUCCESS;
}

void UsbMscInitialize(IO_DRIVER_OBJECT*driver){msc_driver=driver;disk_count=0;if(driver)driver->major_function[IO_MJ_READ]=msc_read;}

IO_DEVICE_OBJECT *UsbMscAttach(void*context,USB_MSC_BULK_TRANSFER transfer,
                               uint8_t bulk_in,uint8_t bulk_out) {
    uint8_t capacity[8],cdb[10]={SCSI_READ_CAPACITY_10};char name[16]="UsbDisk0";
    if(!msc_driver||!transfer||disk_count>9)return 0;
    name[7]=(char)('0'+disk_count);
    IO_DEVICE_OBJECT*object=IoCreateDevice(msc_driver,name,sizeof(MSC_DEVICE));
    if(!object)return 0;MSC_DEVICE*d=object->device_extension;d->context=context;
    d->transfer=transfer;d->bulk_in=bulk_in;d->bulk_out=bulk_out;d->next_tag=0;
    if(!bot_command(d,cdb,10,capacity,sizeof(capacity),1)){IoDeleteDevice(object);return 0;}
    d->block_count=be32(capacity)+1;d->block_size=be32(capacity+4);
    if(!d->block_size){IoDeleteDevice(object);return 0;}
    SerialPutString("[USB MSC] Attached ");SerialPutString(name);SerialPutString(", blocks ");
    SerialPrintDec(d->block_count);SerialPutString(" x ");SerialPrintDec(d->block_size);SerialPutString(" bytes\r\n");
    disk_count++;return object;
}
