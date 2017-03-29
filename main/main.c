//
// Created by cormac on 16/02/17.
//
#include "main.h"

#include <pthread.h>

//reference taken from https://www.gnu.org/software/libc/manual/html_node/Argp-Example-3.htmlf
const char *argp_program_version = "0.0.1";
const char *argp_program_bug_address = "<cormac.brady@hotmai.co.uk>";
static char rt8900_doc[] = "Provides serial control for the YAESU FT-8900R Transceiver.";
static char rt8900_args_doc[] = "<serial port path>";

static struct argp_option rt8900options[] = {
        {"rts-on", 'r', 0, OPTION_ARG_OPTIONAL,
                "Use the RTS pin of the serial connection as a power button for the rig. (REQUIRES compatible hardware)"},
        {"verbose", 'v', "LEVEL", OPTION_ARG_OPTIONAL,
                "Produce verbose output add a number to select level (1 = ERROR, 2= WARNING, 3=INFO, 4=ERROR, 5=DEBUG) output default is 'warning'."},
        {"hard-emulation", 991, 0, OPTION_ARG_OPTIONAL,
                "Exactly emulates the radio head instead of being lazy_sending (worse performance, no observed benefit, only useful for debugging)"},
        { 0 }
};

///Parse options
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
        /* Get the input argument from argp_parse, which we
           know is a pointer to our arguments structure. */
        SERIAL_CFG *cfg = state->input;
        int i;
        switch (key)
        {
        case 991:
                cfg->send.lazy_sending = arg ? true : false;
                break;
        case 'v':
                set_log_level( (enum rt8900_logging_level) atoi(arg));
                break;
        case 'r':
                cfg->receive.rts_pin_as_on = true;
                break;
        case ARGP_KEY_ARG:
                if (state->arg_num >= 1)
                        /* Too many arguments. */
                        argp_usage(state);

                cfg->serial_path = arg;
                break;

        case ARGP_KEY_END:
                if (state->arg_num < 1)
                        /* Not enough arguments. */
                        argp_usage(state);
                break;
        default:
                return ARGP_ERR_UNKNOWN;
        }
        return 0;
}
static struct argp argp = { rt8900options, parse_opt, rt8900_args_doc, rt8900_doc };

static SERIAL_CFG *g_conf = NULL;

///shutdown the program gracefully
void graceful_shutdown(int signal)
{
        //use strsignal(signal) to get the name from int if we want to add others latter
        log_msg(RT8900_INFO, "\nShutting down...\n");
        if (g_conf != NULL) {
                g_conf->send.keep_alive = false;
                g_conf->receive.keep_alive = false;
                if (g_conf->serial_fd != 0) {
                        tcflush(g_conf->serial_fd, TCIOFLUSH); //flush in/out buffers
                        close(g_conf->serial_fd);
                }
                fclose(stdin); //we close std so that user prompt (get_char) will stop blocking
        }
}

void init_graceful_shutdown(SERIAL_CFG *c)
{
        g_conf = c;
        signal(SIGINT, graceful_shutdown);
}

char *read_prompt_line(void)
{
        // inspired by https://github.com/brenns10/lsh/blob/407938170e8b40d231781576e05282a41634848c/src/main.c
        int buffer_size = PROMPT_BUFFER_SIZE;
        char *buffer = malloc(sizeof(char) * buffer_size);
        if (!buffer) {
                log_msg(RT8900_ERROR, "Failed to allocate buffer");
                exit(EXIT_FAILURE);
        }

        int i;
        for (i = 0; ; i++) {

                if (i >= buffer_size) {
                        buffer_size = buffer_size * 2;
                        buffer = realloc(buffer, buffer_size);
                        if (!buffer) {
                                log_msg(RT8900_ERROR, "Failed to increase buffer size");
                                exit(EXIT_FAILURE);
                        }
                }

                int ch = getchar();

                if (ch == EOF || ch == '\n') {
                        buffer[i] = '\0';
                        return buffer;
                } else {
                        buffer[i] = ch;
                }
        }
}


/// Split string into arg array
char **split_line_args(char *line)
{
        // inspired by https://github.com/brenns10/lsh/blob/407938170e8b40d231781576e05282a41634848c/src/main.c
        int buffer_size = PROMPT_BUFFER_SIZE * 2;
        char **args = malloc(buffer_size * sizeof(char*));
        char *token;
        int i;

        token = strtok(line, " ");
        for (i = 0; (token != NULL || i < buffer_size) ; i++) {
                args[i] = token;
                token = strtok(NULL, " ");
        }
        args[i] = NULL;
        return args;
}

void print_invalid_command()
{
        printf("%s%s%s\n", ANSI_COLOR_YELLOW, "Invalid command", ANSI_COLOR_RESET);
}

//TODO is ment to be a tempory prompt for devlopment. even so it is a real mess and needs refactor!
int run_command(char **cmd, SERIAL_CFG *config, struct control_packet *base_packet)
{
        int num_args = 0;
        for (num_args = 0; cmd[num_args] != NULL; num_args++);

        int left_op;
        int right_op;

        struct radio_state *current_state = malloc(sizeof(*current_state));
        struct display_packet packet;

        get_display_packet(config, &packet);

        switch (num_args){
        case 0:
                print_invalid_command();
                break;
        case 1:
                if (strcmp(cmd[0], "exit") == 0) {
                        printf("%sExiting%s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
                        return 0;
                } else if (strcmp(cmd[0], "b") == 0){
                        read_state_from_packet(&packet, current_state);

                        char* lbusy = (current_state->left.busy ? "Busy" : "Not Busy");
                        char* lmain = (is_main(current_state, &(current_state->left)) ? " - Main" : "");

                        char* rbusy = (current_state->right.busy ? "Busy" : "Not Busy");
                        char* rmain = (is_main(current_state, &(current_state->right)) ? " - Main" : "");

                        printf("Left  radio -> %s%s\nRight radio -> %s%s\n", lbusy, lmain, rbusy, rmain);
                } else {
                        print_invalid_command();
                }
                break;
        case 2:
                left_op = (int) strtoimax(cmd[1], NULL, 10);
                if (strcmp(cmd[0], "F") == 0) {
                        left_op = (int) strtoimax(cmd[1], NULL, 10);
                        printf("%s Setting frequency -> %d %s\n", ANSI_COLOR_GREEN, left_op, ANSI_COLOR_RESET);
                        set_frequency(config, base_packet, left_op);
                } else if (strcmp(cmd[0], "M") == 0) {
                        if (strcmp(cmd[1], "l") == 0) {
                                read_state_from_packet(&packet, current_state);
                                if (!is_main(current_state, &(current_state->left))) {
                                     set_main_radio(config, base_packet, RADIO_LEFT);
                                }
                                printf("%s Setting Main radio to -> left %s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);

                        } else if (strcmp(cmd[1], "r") == 0){
                                if (!is_main(current_state, &(current_state->right))) {
                                       set_main_radio(config, base_packet, RADIO_RIGHT);
                                }
                                printf("%s Setting Main radio to -> right %s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
                        }  else if (strcmp(cmd[1], "P") == 0){
                                printf("%s Pressing power button %s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
                                set_power_button(config);
                        } else {
                                print_invalid_command();
                        }
                } else if (strcmp(cmd[0], "T") == 0) {
                        ptt(base_packet, left_op);
                } else {
                        print_invalid_command();
                }
                break;
        case 3:
                left_op = (int)strtoimax(cmd[1], NULL, 10);
                right_op = (int)strtoimax(cmd[2], NULL, 10);
                if (strcmp(cmd[0], "V") == 0) {
                        printf("%s Setting volume -> %d %d%s\n", ANSI_COLOR_GREEN, left_op, right_op, ANSI_COLOR_RESET);
                        set_volume(base_packet, left_op, right_op);

                } else if (strcmp(cmd[0], "S") == 0){
                        printf("%s Setting squelsh -> %d %d%s\n", ANSI_COLOR_GREEN, left_op, right_op, ANSI_COLOR_RESET);
                        set_squelch(base_packet, left_op, right_op);
                } else {
                        print_invalid_command();
                }
        default:
                break;
        }
        free(current_state);
        current_state = NULL;
        return  1;
}

void user_prompt(SERIAL_CFG *config, struct control_packet *base_packet)
{
        char *line;
        char **command_arr;

        while (config->send.keep_alive) {
                printf("> ");
                line  = read_prompt_line();;
                command_arr = split_line_args(line);

                int dont_exit = run_command(command_arr, config, base_packet);

                free(line);
                free(command_arr);
                if (!dont_exit){
                        break;
                }
        }
}

int main(int argc, char **argv)
{
        //Create our config
        SERIAL_CFG c = {
                .send.lazy_sending = true,
                .receive.rts_pin_as_on = false,
        };
        argp_parse (&argp, argc, argv, 0, 0, &c); //insert user options to config


        //Create a thread to send oud control packets
        pthread_t packet_sender_thread;
        pthread_t packet_receive_thread;
        pthread_barrier_t wait_for_sender;

        pthread_barrier_init(&wait_for_sender, NULL, 2);
        c.send.initialised = &wait_for_sender;

        init_graceful_shutdown(&c);

        pthread_create(&packet_sender_thread, NULL, send_control_packets, &c);
        pthread_barrier_wait(&wait_for_sender); //wait send thread to be ready

        //Setup our initial packet that will be sent
        maloc_control_packet(start_packet)
        memcpy(start_packet, &control_packet_defaults ,sizeof(*start_packet));
        send_new_packet(&c, start_packet, PACKET_ONLY_SEND);

        pthread_create(&packet_receive_thread, NULL, receive_display_packets, &c);

        //if the radio is not already on try to turn it on
        if (check_radio_rx(&c) == 0) {

                if (c.receive.rts_pin_as_on == true) {
                        int give_up = 0;
                        while (set_power_button(&c) != 0 && c.send.keep_alive) {
                                give_up++;
                                log_msg(RT8900_ERROR, "FAILED TO TURN ON THE RADIO AFTER %d trys\n", give_up);
                                if (give_up >= TURN_ON_RADIO_TRYS) {
                                        break;
                                }
                                sleep(1);
                        }
                } else {
                        log_msg(RT8900_INFO, "Waiting for radio to be turned on (no time out)\n");
                        while (check_radio_rx(&c) == 0 && c.send.keep_alive) {};
                }
        }

        if (check_radio_rx(&c) == 1) {
                log_msg(RT8900_INFO, "SUCCESS!\n");
                user_prompt(&c, start_packet);
        }


        //get current state of radio
        //struct display_packet *current_state = malloc(sizeof(struct display_packet));
        //get_display_packet(&c, current_state);
        //todo check if any existing state needs to be transferred to our starting packet


        graceful_shutdown(0);
        pthread_barrier_destroy(&wait_for_sender);
        pthread_join(packet_sender_thread, NULL);
        pthread_join(packet_receive_thread, NULL);
        return 0;
}