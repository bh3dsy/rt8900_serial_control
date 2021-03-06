#include <stdio.h>
#include <stdlib.h>

#include "control_packet.h"
#include "log.h"

const struct button_transmit_value KEYPAD_BUTTON_NONE = BUTTON_NONE_VALUE;
const struct button_transmit_value KEYPAD_NUMBER_BUTTONS[10] = {
        BUTTON_0_VALUE,
        BUTTON_1_VALUE,
        BUTTON_2_VALUE,
        BUTTON_3_VALUE,
        BUTTON_4_VALUE,
        BUTTON_5_VALUE,
        BUTTON_6_VALUE,
        BUTTON_7_VALUE,
        BUTTON_8_VALUE,
        BUTTON_9_VALUE
};

const struct button_transmit_value KEYPAD_BUTTON_A = BUTTON_A_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_B = BUTTON_B_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_C = BUTTON_C_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_D = BUTTON_D_VALUE;

const struct button_transmit_value KEYPAD_BUTTON_HASH = BUTTON_HASH_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_X = BUTTON_X_VALUE;

const struct button_transmit_value KEYPAD_BUTTON_P1 = BUTTON_P1_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_P2 = BUTTON_P2_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_P3 = BUTTON_P3_VALUE;
const struct button_transmit_value KEYPAD_BUTTON_P4 = BUTTON_P4_VALUE;

/// The recommended defaults for the control packet
const struct control_packet control_packet_defaults = {
        /*There are manny defaults that are 0 so we leave them as "{}"
        The elements are not addressed by name for c++ compatibility so we can test*/

        {.section = {.data = DATA_MIN_NUM, .check_num=SBO}}, //encoder_right| 0 turns
        {},                                         //encoder_left          | 0 turns
        {.section = {.data= DATA_MAX_NUM}},         // ptt                  | set to high (off)
        {.section = {.data = DATA_MAX_NUM}},        //squelch_right         | no squelch muting
        {.section = {.data = DEFAULT_VOLUME}},      // volume_control_right | set to 25% volume
        {.section = {.data = VD_NONE}},// keypad_input_row     | no buttons being pressed
        {.section = {.data = DEFAULT_VOLUME}},      // volume_control_left  | set to 25% volume
        {.section = {.data = DATA_MAX_NUM}},        // squelch_left         | no squelch muting
        {.section = {.data = VD_NONE}},// keypad_input_column  | full is no buttons being pressed
        {.section = {.data = DATA_MAX_NUM}},        // right_buttons        | full is no buttons being pressed
        {.section = {.data = DATA_MAX_NUM}},        // left_buttons         | full is no buttons being pressed
        {.section = {.data = NOT_PRESSED}},         // main_buttons         |
        {},                                         // hyper_mem_buttons    |
};

void set_keypad_button(struct control_packet *packet, const struct button_transmit_value *button)
{
        packet->keypad_input_row.section.data    = (unsigned char) button->row;
        packet->keypad_input_column.section.data = (unsigned char) button->column;
}

void set_main_button(struct control_packet *packet, const enum main_menu_buttons button)
{
        packet->main_buttons.section.data = (unsigned char) button;
}

void set_left_button(struct control_packet *packet, const enum left_menu_buttons button)
{
        packet->left_buttons.section.data = (unsigned char) button;
}

void set_right_button(struct control_packet *packet, const enum right_menu_buttons button)
{
        packet->right_buttons.section.data = (unsigned char) button;
}


const struct button_transmit_value * button_from_int(int i)
{
        if (-1 < i && i < 10) {
                return &KEYPAD_NUMBER_BUTTONS[i];
        } else {
                log_msg(RT8900_WARNING, "WARNING: Invalid range given to button_from_int was: %d (must be 0-10) ", i);
                return &KEYPAD_BUTTON_NONE;
        }
}

/// \brief Takes and returns a int if it can fit into a PACKET_BYTE
///
/// @returns NULL if the number will not fit into the packet (7 bits)
/// else will return the provided int
signed char safe_int_char(int number)
{
        if(number < DATA_MIN_NUM) {
                return (signed char) DATA_MIN_NUM;

        } else if (number > DATA_MAX_NUM) {
                return (signed char) DATA_MAX_NUM;
        }
        return (signed char) number;
}

///set the volume between 0-127. 0 is mute.
int set_volume_left(struct control_packet *packet, int number)
{
        if (packet == NULL) {
                return 1;
        }
        packet->volume_control_left.section.data = safe_int_char(number);
        return 0;
}

int get_volume_left(struct control_packet *packet)
{
        if (packet == NULL) {
                return 0;
        }
        return packet->volume_control_left.section.data;
}

///set the volume between 0-127. 0 is mute.
int set_volume_right(struct control_packet *packet, int number)
{
        if (packet == NULL) {
                return 1;
        }
        packet->volume_control_right.section.data = safe_int_char(number);
        return 0;
}

int get_volume_right(struct control_packet *packet)
{
        if (packet == NULL) {
                return 0;
        }
        return packet->volume_control_right.section.data;
}

/// Set the left and right volume. between 0-127. 0 is mute.
int set_volume(struct control_packet *packet, int left, int right)
{
        int o = 0;
        o += set_volume_left(packet, left);
        o += set_volume_right(packet, right);
        return o;
}

/// Set the left squelch between 0-127.
///127 filters no noise.e.
int set_squelch_left(struct control_packet *packet, int number)
{
        if (packet == NULL) {
                return 1;
        }
        packet->squelch_left.section.data = safe_int_char(number);
        return 0;
}

int get_squelch_left(struct control_packet *packet)
{
        if (packet == NULL) {
                return 0;
        }
        return packet->squelch_left.section.data;
}

/// Set the right squelch between 0-127.
///127 filters no noise.
int set_squelch_right(struct control_packet *packet, int number)
{
        if (packet == NULL) {
                return 1;
        }
        packet->squelch_right.section.data = safe_int_char(number);
        return 0;
}

int get_squelch_right(struct control_packet *packet)
{
        if (packet == NULL) {
                return 0;
        }
        return packet->squelch_right.section.data;
}

/// Set the left and right squelch. between 0-127.
///127 filters no noise.
int set_squelch(struct control_packet *packet, int left, int right)
{
        int o = 0;
        o += set_squelch_left(packet, left);
        o += set_squelch_right(packet, right);
        return o;
}

/// toggle transmission 2 to start 1 to stop
void ptt(struct control_packet *base_packet, int ptt)
{
        switch (ptt){
        case 0:
                log_msg(RT8900_INFO, "Stopping transmission\n");
                base_packet->ptt.section.data = DATA_MAX_NUM;
                break;
        case 1:
                log_msg(RT8900_INFO, "STARTING transmission\n");
                base_packet->ptt.section.data = DATA_MIN_NUM;
                break;
        default:break;
        }
}

void packet_debug(const struct control_packet *packet, CONTROL_PACKET_INDEXED *input_packet_arr)
{
        //Create a array to examine the packet if one was not supplied
        CONTROL_PACKET_INDEXED *packet_array = input_packet_arr;
        if (packet_array == NULL) {
                CONTROL_PACKET_INDEXED new_array = {.as_struct = *packet};
                packet_array = &new_array;
        }

        int i;
        printf("\n");
        printf("--------------------------\n");
        printf("SERIAL CONTROL THREAD\nNew pointer address: %p \nNow sending:\n", packet);
        for (i = 0; i < sizeof((*packet_array).as_array); i++) {
                print_char((*packet_array).as_array[i].raw);
        }
        printf("--------------------------\n");
}
