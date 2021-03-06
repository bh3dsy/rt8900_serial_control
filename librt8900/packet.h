/**
 * @file packet.h
 * \brief Data representation of a single byte in a packet.
 */

#ifndef RT8900_SERIAL_PACKET_H
#define RT8900_SERIAL_PACKET_H

#define NUM_DATA_BITS 8
#define NUM_STOP_BITS 1
#define NUM_PARITY_BITS 0


///The last bit in a packet byte.
/// 1 if it is the first in the packet.
enum check_num_values {
    SBZ = 0X00,
    SBO = 0X01
};

///The highest and lowest possible values that can be held in the data section of the packet
enum common_7bit_data_values {
    DATA_MAX_NUM = 0x7F,
    DATA_MIN_NUM = 0X00
};

#pragma pack(1) //as we don't want space between our bits here
typedef struct {
    unsigned int data: 7;  // 7 more bits of the byte
    unsigned int check_num: 1;  // the last bit int he byte (1st in packet is 1 else 0)
} FT8900BYTE;
///Used to store 1 byte of data from the packet
typedef union {
    FT8900BYTE section;
    unsigned char raw;
} PACKET_BYTE;
#pragma pack() //undo this once we are done (IF THIS LINE IS REMOVED LIBS WILL STOP WORKING FOR YOU SUDDENLY)

void print_char(char byte);
int find_packet_start(unsigned char buffer[], size_t length);

#endif //RT8900_SERIAL_PACKET_H