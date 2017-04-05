//
// Created by cormac on 14/03/17.
//
#include <unistd.h>
#include <sys/ioctl.h>

#include "display_packet.h"
#include "packet.h"

/// Write to the packet in the correct order.
/// For example the packet may start at index 10. Assumes buffer array length is DISPLAY_PACKET_SIZE (42)
void insert_shifted_packet(DISPLAY_PACKET packet, unsigned char buffer[], size_t buffer_length, int start_of_packet_index)
{
        // Starting from index to the end
        int i = 0;
        int packet_index = 0;
        for (i = start_of_packet_index; i < buffer_length; i++, packet_index++) {
                packet[packet_index].raw = buffer[i];
        }
        //now from the beginning to the index
        for (i = 0; i < start_of_packet_index; i++, packet_index++) { ;
                packet[packet_index].raw = buffer[i];
        }
}

///Gets busy state from display_packet
void read_busy(DISPLAY_PACKET packet, struct radio_state *state)
{
        state->left.busy  = display_packet_read(packet, LEFT_BUISY);
        state->right.busy = display_packet_read(packet, RIGHT_BUISY);
}

///Sets the main correct pointer to the correct radio, NULL if nether selected
void read_main(DISPLAY_PACKET packet, struct radio_state *state)
{
        int left = display_packet_read(packet, LEFT_MAIN);
        int right = display_packet_read(packet, RIGHT_MAIN);

        if (left == right) {
                log_msg(RT8900_ERROR, "Could not determine 'main' radio  from PACKET\n");
                state->main = NULL;
        } else if (left) {
                state->main = &state->left;
        } else {
                state->main = &(state->right);
        }
}

/// Gets the power levels of the radios only using reads,
// todo Currently there is no way to know if med1 or med2 is set
// todo create a seprate non fussy function that will be called to toggel power button 4 times (a circle) and then read the screen
void read_power_fuzzy(DISPLAY_PACKET packet, struct radio_state *state)
{
        if (display_packet_read(packet, LEFT_POWER_LOW)) {
                state->left.power_level = POWER_LOW;
        } else if (display_packet_read(packet, LEFT_POWER_MEDIUM)) {
                state->left.power_level = POWER_MEDIUM_FUZZY;
        } else {
                state->left.power_level = POWER_HIGH;
        }

        if (display_packet_read(packet, RIGHT_POWER_LOW)) {
                state->right.power_level = POWER_LOW;
        } else if (display_packet_read(packet, RIGHT_POWER_MEDIUM)) {
                state->right.power_level = POWER_MEDIUM_FUZZY;
        } else {
                state->right.power_level = POWER_HIGH;
        }
}

//todo this
//truth table for a 13 segment display
/*      A       B       C       D       E       F       G       H       I       J       K       L       M       hex
 * 0    1	1	1	1	1	1	0	0	1	0	1	1	1       1F97
 * 1    1	0	1	0	0	0	0	0	0	0	0	0	0       1400
 * 2    0	1	1	0	1	1	0	0	1	0	0	1	0       D92
 * 3    1       1       1       0       1       1       0       0       1       0       0       0       0
 * 4    1       1       1       0       0       1       0       0       0       0       0       0       1
 * 5
 * 6
 * 7
 * 8
 * 9
 * macros from truth table */
//todo convert to hex else we are stuck to gcc
#define THIRTEEN_SEG_0 0b1011100010111
#define THIRTEEN_SEG_1 0b1010000000000
#define THIRTEEN_SEG_2 0b0110110010010
#define THIRTEEN_SEG_3 0b1110110010000
#define THIRTEEN_SEG_4 0b1110010000001
#define THIRTEEN_SEG_5 0b1100110010001
#define THIRTEEN_SEG_6 0b1100110010011
#define THIRTEEN_SEG_7 0b1010100000000
#define THIRTEEN_SEG_8 0b1110110010011
#define THIRTEEN_SEG_9 0b1110110010001

///Takes a bitfireld and matches to knowen numbers an char of bits (ordered as described above)
/// @returns 0-9 and -1 on error
int segment_to_int(int segment_bitfield)
{
        switch (segment_bitfield) {
        case 0: //left digets that are 0 are not showen
        case THIRTEEN_SEG_0:
                return 0;
        case THIRTEEN_SEG_1:
                return 1;
        case THIRTEEN_SEG_2:
                return 2;
        case THIRTEEN_SEG_3:
                return 3;
        case THIRTEEN_SEG_4:
                return 4;
        case THIRTEEN_SEG_5:
                return 5;
        case THIRTEEN_SEG_6:
                return 6;
        case THIRTEEN_SEG_7:
                return 7;
        case THIRTEEN_SEG_8:
                return 8;
        case THIRTEEN_SEG_9:
                return 9;
        default:
                return -1;
        }
}

/// Takes a 13 arguments each arument is a bit (as int) that represents each of the 13 segments of a diget.
/// @returns the diget between 0-9 and -1 on an unreconised diget
/// A fully blank section will assumed to be 0
int decode_13_segment(int first_segment, ...)
{
        va_list ap;
        va_start(ap, first_segment);

        int bit_field = 0;
        int segment = first_segment;
        int i;

        //add all the arguments together into a bit field
        for (i = 0; i < 13; i++) {
                log_msg(RT8900_TRACE, "%d " , segment);
                bit_field = (bit_field << 1) | segment;
                segment = va_arg(ap, int);
        }
        va_end(ap);

        int diget = segment_to_int(bit_field);
        log_msg(RT8900_TRACE, " -> %d\n", diget);
        if (diget == -1) {
                log_msg(RT8900_ERROR, "UNKNOWN NUMBER\n");
        }
        return diget;
}

void create_bit_field(int *result, char bit)
{
        *result = (*result << 1) | bit;
}

int display_packet_read(DISPLAY_PACKET packet, const enum display_packet_bitmasks bit_number)
{
        int bit = bit_number % 8;
        int byte = bit_number / 8;

        return (packet[byte].raw >> bit) & 0x80 != 0;
};

int read_frequency(DISPLAY_PACKET packet, struct radio_state *state)
{
        //get and decode all the bits
        int left_digits[6] = {
                decode_13_segment(display_packet_read(packet, LEFT_FREQ_1_A),
                                  display_packet_read(packet, LEFT_FREQ_1_B),
                                  display_packet_read(packet, LEFT_FREQ_1_C),
                                  display_packet_read(packet, LEFT_FREQ_1_D),
                                  display_packet_read(packet, LEFT_FREQ_1_E),
                                  display_packet_read(packet, LEFT_FREQ_1_F),
                                  display_packet_read(packet, LEFT_FREQ_1_G),
                                  display_packet_read(packet, LEFT_FREQ_1_H),
                                  display_packet_read(packet, LEFT_FREQ_1_I),
                                  display_packet_read(packet, LEFT_FREQ_1_J),
                                  display_packet_read(packet, LEFT_FREQ_1_K),
                                  display_packet_read(packet, LEFT_FREQ_1_L),
                                  display_packet_read(packet, LEFT_FREQ_1_M)
                ),
                decode_13_segment(display_packet_read(packet, LEFT_FREQ_2_A),
                                  display_packet_read(packet, LEFT_FREQ_2_B),
                                  display_packet_read(packet, LEFT_FREQ_2_C),
                                  display_packet_read(packet, LEFT_FREQ_2_D),
                                  display_packet_read(packet, LEFT_FREQ_2_E),
                                  display_packet_read(packet, LEFT_FREQ_2_F),
                                  display_packet_read(packet, LEFT_FREQ_2_G),
                                  display_packet_read(packet, LEFT_FREQ_2_H),
                                  display_packet_read(packet, LEFT_FREQ_2_I),
                                  display_packet_read(packet, LEFT_FREQ_2_J),
                                  display_packet_read(packet, LEFT_FREQ_2_K),
                                  display_packet_read(packet, LEFT_FREQ_2_L),
                                  display_packet_read(packet, LEFT_FREQ_2_M)
                ),
                decode_13_segment(display_packet_read(packet, LEFT_FREQ_3_A),
                                  display_packet_read(packet, LEFT_FREQ_3_B),
                                  display_packet_read(packet, LEFT_FREQ_3_C),
                                  display_packet_read(packet, LEFT_FREQ_3_D),
                                  display_packet_read(packet, LEFT_FREQ_3_E),
                                  display_packet_read(packet, LEFT_FREQ_3_F),
                                  display_packet_read(packet, LEFT_FREQ_3_G),
                                  display_packet_read(packet, LEFT_FREQ_3_H),
                                  display_packet_read(packet, LEFT_FREQ_3_I),
                                  display_packet_read(packet, LEFT_FREQ_3_J),
                                  display_packet_read(packet, LEFT_FREQ_3_K),
                                  display_packet_read(packet, LEFT_FREQ_3_L),
                                  display_packet_read(packet, LEFT_FREQ_3_M)
                ),
                decode_13_segment(display_packet_read(packet, LEFT_FREQ_4_A),
                                  display_packet_read(packet, LEFT_FREQ_4_B),
                                  display_packet_read(packet, LEFT_FREQ_4_C),
                                  display_packet_read(packet, LEFT_FREQ_4_D),
                                  display_packet_read(packet, LEFT_FREQ_4_E),
                                  display_packet_read(packet, LEFT_FREQ_4_F),
                                  display_packet_read(packet, LEFT_FREQ_4_G),
                                  display_packet_read(packet, LEFT_FREQ_4_H),
                                  display_packet_read(packet, LEFT_FREQ_4_I),
                                  display_packet_read(packet, LEFT_FREQ_4_J),
                                  display_packet_read(packet, LEFT_FREQ_4_K),
                                  display_packet_read(packet, LEFT_FREQ_4_L),
                                  display_packet_read(packet, LEFT_FREQ_4_M)
                ),
                decode_13_segment(display_packet_read(packet, LEFT_FREQ_5_A),
                                  display_packet_read(packet, LEFT_FREQ_5_B),
                                  display_packet_read(packet, LEFT_FREQ_5_C),
                                  display_packet_read(packet, LEFT_FREQ_5_D),
                                  display_packet_read(packet, LEFT_FREQ_5_E),
                                  display_packet_read(packet, LEFT_FREQ_5_F),
                                  display_packet_read(packet, LEFT_FREQ_5_G),
                                  display_packet_read(packet, LEFT_FREQ_5_H),
                                  display_packet_read(packet, LEFT_FREQ_5_I),
                                  display_packet_read(packet, LEFT_FREQ_5_J),
                                  display_packet_read(packet, LEFT_FREQ_5_K),
                                  display_packet_read(packet, LEFT_FREQ_5_L),
                                  display_packet_read(packet, LEFT_FREQ_5_M)
                ),
                decode_13_segment(display_packet_read(packet, LEFT_FREQ_6_A),
                                  display_packet_read(packet, LEFT_FREQ_6_B),
                                  display_packet_read(packet, LEFT_FREQ_6_C),
                                  display_packet_read(packet, LEFT_FREQ_6_D),
                                  display_packet_read(packet, LEFT_FREQ_6_E),
                                  display_packet_read(packet, LEFT_FREQ_6_F),
                                  display_packet_read(packet, LEFT_FREQ_6_G),
                                  display_packet_read(packet, LEFT_FREQ_6_H),
                                  display_packet_read(packet, LEFT_FREQ_6_I),
                                  display_packet_read(packet, LEFT_FREQ_6_J),
                                  display_packet_read(packet, LEFT_FREQ_6_K),
                                  display_packet_read(packet, LEFT_FREQ_6_L),
                                  display_packet_read(packet, LEFT_FREQ_6_M)
                )
        };

        int right_digits[6] = {
                decode_13_segment(display_packet_read(packet, RIGHT_FREQ_1_A),
                                  display_packet_read(packet, RIGHT_FREQ_1_B),
                                  display_packet_read(packet, RIGHT_FREQ_1_C),
                                  display_packet_read(packet, RIGHT_FREQ_1_D),
                                  display_packet_read(packet, RIGHT_FREQ_1_E),
                                  display_packet_read(packet, RIGHT_FREQ_1_F),
                                  display_packet_read(packet, RIGHT_FREQ_1_G),
                                  display_packet_read(packet, RIGHT_FREQ_1_H),
                                  display_packet_read(packet, RIGHT_FREQ_1_I),
                                  display_packet_read(packet, RIGHT_FREQ_1_J),
                                  display_packet_read(packet, RIGHT_FREQ_1_K),
                                  display_packet_read(packet, RIGHT_FREQ_1_L),
                                  display_packet_read(packet, RIGHT_FREQ_1_M)
                ),
                decode_13_segment(display_packet_read(packet, RIGHT_FREQ_2_A),
                                  display_packet_read(packet, RIGHT_FREQ_2_B),
                                  display_packet_read(packet, RIGHT_FREQ_2_C),
                                  display_packet_read(packet, RIGHT_FREQ_2_D),
                                  display_packet_read(packet, RIGHT_FREQ_2_E),
                                  display_packet_read(packet, RIGHT_FREQ_2_F),
                                  display_packet_read(packet, RIGHT_FREQ_2_G),
                                  display_packet_read(packet, RIGHT_FREQ_2_H),
                                  display_packet_read(packet, RIGHT_FREQ_2_I),
                                  display_packet_read(packet, RIGHT_FREQ_2_J),
                                  display_packet_read(packet, RIGHT_FREQ_2_K),
                                  display_packet_read(packet, RIGHT_FREQ_2_L),
                                  display_packet_read(packet, RIGHT_FREQ_2_M)
                ),
                decode_13_segment(display_packet_read(packet, RIGHT_FREQ_3_A),
                                  display_packet_read(packet, RIGHT_FREQ_3_B),
                                  display_packet_read(packet, RIGHT_FREQ_3_C),
                                  display_packet_read(packet, RIGHT_FREQ_3_D),
                                  display_packet_read(packet, RIGHT_FREQ_3_E),
                                  display_packet_read(packet, RIGHT_FREQ_3_F),
                                  display_packet_read(packet, RIGHT_FREQ_3_G),
                                  display_packet_read(packet, RIGHT_FREQ_3_H),
                                  display_packet_read(packet, RIGHT_FREQ_3_I),
                                  display_packet_read(packet, RIGHT_FREQ_3_J),
                                  display_packet_read(packet, RIGHT_FREQ_3_K),
                                  display_packet_read(packet, RIGHT_FREQ_3_L),
                                  display_packet_read(packet, RIGHT_FREQ_3_M)
                ),
                decode_13_segment(display_packet_read(packet, RIGHT_FREQ_4_A),
                                  display_packet_read(packet, RIGHT_FREQ_4_B),
                                  display_packet_read(packet, RIGHT_FREQ_4_C),
                                  display_packet_read(packet, RIGHT_FREQ_4_D),
                                  display_packet_read(packet, RIGHT_FREQ_4_E),
                                  display_packet_read(packet, RIGHT_FREQ_4_F),
                                  display_packet_read(packet, RIGHT_FREQ_4_G),
                                  display_packet_read(packet, RIGHT_FREQ_4_H),
                                  display_packet_read(packet, RIGHT_FREQ_4_I),
                                  display_packet_read(packet, RIGHT_FREQ_4_J),
                                  display_packet_read(packet, RIGHT_FREQ_4_K),
                                  display_packet_read(packet, RIGHT_FREQ_4_L),
                                  display_packet_read(packet, RIGHT_FREQ_4_M)
                ),
                decode_13_segment(display_packet_read(packet, RIGHT_FREQ_5_A),
                                  display_packet_read(packet, RIGHT_FREQ_5_B),
                                  display_packet_read(packet, RIGHT_FREQ_5_C),
                                  display_packet_read(packet, RIGHT_FREQ_5_D),
                                  display_packet_read(packet, RIGHT_FREQ_5_E),
                                  display_packet_read(packet, RIGHT_FREQ_5_F),
                                  display_packet_read(packet, RIGHT_FREQ_5_G),
                                  display_packet_read(packet, RIGHT_FREQ_5_H),
                                  display_packet_read(packet, RIGHT_FREQ_5_I),
                                  display_packet_read(packet, RIGHT_FREQ_5_J),
                                  display_packet_read(packet, RIGHT_FREQ_5_K),
                                  display_packet_read(packet, RIGHT_FREQ_5_L),
                                  display_packet_read(packet, RIGHT_FREQ_5_M)
                ),
                decode_13_segment(display_packet_read(packet, RIGHT_FREQ_6_A),
                                  display_packet_read(packet, RIGHT_FREQ_6_B),
                                  display_packet_read(packet, RIGHT_FREQ_6_C),
                                  display_packet_read(packet, RIGHT_FREQ_6_D),
                                  display_packet_read(packet, RIGHT_FREQ_6_E),
                                  display_packet_read(packet, RIGHT_FREQ_6_F),
                                  display_packet_read(packet, RIGHT_FREQ_6_G),
                                  display_packet_read(packet, RIGHT_FREQ_6_H),
                                  display_packet_read(packet, RIGHT_FREQ_6_I),
                                  display_packet_read(packet, RIGHT_FREQ_6_J),
                                  display_packet_read(packet, RIGHT_FREQ_6_K),
                                  display_packet_read(packet, RIGHT_FREQ_6_L),
                                  display_packet_read(packet, RIGHT_FREQ_6_M)
                )
        };

        //add the ints together in the correct postion going up
        int multiplier  = 10;
        int left_total = 0;
        int right_total = 0;
        for (int i = 5; i >= 0; i--) {
                if (left_digits[i] == -1 || right_digits[i] == -1) {
                        //abort if we never decoded the number
                        return 1;
                }
                left_total += left_digits[i] * multiplier;
                right_total += right_digits[i] * multiplier;
                multiplier *= 10;
        }

        //the last digit caan only show 0 or 5 so it represented at 1 bit
        if (display_packet_read(packet, LEFT_FREQ_7)) {
                left_total += 5;
        }
        if (display_packet_read(packet, RIGHT_FREQ_7)) {
                right_total += 5;
        }

        state->left.frequency = left_total;
        state->right.frequency = right_total;

        return 0;
}

int is_main(struct radio_state *radio, struct radio_state_sides *side)
{
        return (radio->main == side);
}


void read_state_from_packet(DISPLAY_PACKET packet, struct radio_state *state) {
        read_frequency(packet, state);
        read_busy(packet, state);
        read_main(packet, state);
};
