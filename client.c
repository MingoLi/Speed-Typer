#include <sys/types.h> //mkfifo
#include <sys/stat.h>  //mkfifo
#include <stdio.h>     // printf
#include <fcntl.h>     // open
#include <unistd.h>    // unlink
#include <stdlib.h>    // exit
#include <stdbool.h>   // bool
#include <string.h>    // strlen
#include <pthread.h>
#include <signal.h>


#define BUFFSIZE 64

bool game_over = false;
bool server_down = false;
char PLAYER_FIFO[BUFFSIZE];
char word_to_type[BUFFSIZE];

pthread_mutex_t read_signal_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t write_signal_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t read_pipe_done = PTHREAD_COND_INITIALIZER;
pthread_cond_t read_keyboard_done = PTHREAD_COND_INITIALIZER;

bool read_keyboard_signal = false;
bool write_pipe_signal = false;
int player_fifo;
int server_fifo;
char keyboard_in[BUFFSIZE];

void handler(int arg) {
    if(!server_down)
        write(server_fifo, "e", 2);
    unlink(PLAYER_FIFO);
    pthread_mutex_lock(&print_lock);
    printf("Done unlink FIFO\n");
    printf("Left game, Close client\n");
    pthread_mutex_unlock(&print_lock);
    exit(0);
}

void closeClient() {
    unlink(PLAYER_FIFO);
    pthread_mutex_lock(&print_lock);
    printf("Done unlink FIFO\n");
    printf("Left game, Close client\n");
    pthread_mutex_unlock(&print_lock);
    exit(0);
}

void count_down() {
    char* prompts[] = {
        "ready",
        "set",
        "go"
    };
    int i;
    for(i = 0; i < 3; i++) {
        pthread_mutex_lock(&print_lock);
        printf("%s\n", prompts[i]);
        pthread_mutex_unlock(&print_lock);
        sleep(1);
    }
}

void* read_word(void* args) {
    int result, counter;
    char buf[BUFFSIZE];
    bool need_keyboard;

    // connect to the named pipe
	player_fifo = open(PLAYER_FIFO, O_RDWR);
	if (player_fifo < 0) {
		perror("Unable to open named pipe");
		exit(-1);
	}
    pthread_mutex_lock(&print_lock);
    printf("Player pipe connnected\n");
    pthread_mutex_unlock(&print_lock);

    // Keep reading from pipe
    while(!game_over) {
        need_keyboard = false;
        result = read(player_fifo, &buf, 1);
		if (result < 0) {
			perror("ERROR: Error reading from pipe");
			exit(EXIT_FAILURE);
		} else if ('W' == buf[0]) {
            read(player_fifo, &buf, 1);
            counter = 0;
            while('\0' != buf[0]) {
                word_to_type[counter++] = buf[0];
                read(player_fifo, &buf, 1);
            }
            word_to_type[counter] = '\0';

            pthread_mutex_lock(&print_lock);
            printf("Type: %s\n", word_to_type);
            pthread_mutex_unlock(&print_lock);
            need_keyboard = true;
        } else if ('E' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("Wrong word typed, please try again\n");
            pthread_mutex_unlock(&print_lock);
            need_keyboard = true;
        } else if('L' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("You Loss\n");
            pthread_mutex_unlock(&print_lock);
            closeClient();
        } else if('V' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("You Win\n");
            pthread_mutex_unlock(&print_lock);
            write(server_fifo, "e", 2);
            closeClient();
        } else if('e' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("One player left the match, now exiting match\n");
            pthread_mutex_unlock(&print_lock);
            closeClient();
        } else if('D' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("Server down, enter any key or press ctr+c to quit\n");
            pthread_mutex_unlock(&print_lock);
            server_down = true;
            game_over = true;
            need_keyboard = true;
        } else if('C' == buf[0]) {
            count_down();
        } 

        // Send signal to fgets_thread if need user's input
        if(need_keyboard) {
            pthread_mutex_lock(&read_signal_lock);
            read_keyboard_signal = true;
            pthread_cond_signal(&read_pipe_done);
            pthread_mutex_unlock(&read_signal_lock);
        }        
    }
    pthread_exit(NULL);
}

void* read_keyboard(void* args) {
    while(!game_over) {
        // Wait for signal
        pthread_mutex_lock(&read_signal_lock);
        while(!read_keyboard_signal) {
            pthread_cond_wait(&read_pipe_done, &read_signal_lock);
        }
        read_keyboard_signal = false;
        pthread_mutex_unlock(&read_signal_lock);

        // Read from stdin
        if(!game_over) {
            fgets((char*)&keyboard_in, 64, stdin);

            // Handle empty input
            if(keyboard_in[0] == '\0')
                strcpy(keyboard_in, "empty_word");
        }
        
        // Signal main thread to write to server_pipe
        pthread_mutex_lock(&write_signal_lock);
        write_pipe_signal = true;
        pthread_cond_signal(&read_keyboard_done);
        pthread_mutex_unlock(&write_signal_lock);
    }
    pthread_exit(NULL);
}

int main(){
	const char SERVER_FIFO[] = "server_pipe";
    int errno;
    char msg[BUFFSIZE], typed_word[BUFFSIZE];

    signal(SIGINT, handler);
    signal(SIGTERM, handler);

    // create the player pipe
    char pid[BUFFSIZE], register_info[BUFFSIZE];
    snprintf(pid, BUFFSIZE,"%d",(int)getpid());
    strcpy(PLAYER_FIFO, "player_pipe_");
    strcat(PLAYER_FIFO, pid);
 	
	if (mkfifo(PLAYER_FIFO, 0600)) {
		perror("Unable to create named pipe");
		exit(EXIT_FAILURE);
	}

    // Connect to server
 	server_fifo = open(SERVER_FIFO, O_WRONLY);
	if (server_fifo < 0) {
		perror("Unable to open named pipe");
		exit(-1);
	}

    // Register
    strcpy(register_info, "R");
    strcat(register_info, pid);
    strcat(register_info, "\0");

    errno = write(server_fifo, &register_info, strlen(register_info)+1);
    if (errno < 0) {
        perror("ERROR: Error writing to pipe");
    }

    pthread_mutex_lock(&print_lock);
    printf("Done registeration, game will start once all player stand by\n");
    pthread_mutex_unlock(&print_lock);

    // Retrive word to type
    pthread_t thread_read, thread_fget;
    pthread_create(&thread_read, NULL, read_word, NULL);
    pthread_create(&thread_fget, NULL, read_keyboard, NULL);

    while(!game_over) {
        // Waiting signal from fgets_thread
        pthread_mutex_lock(&write_signal_lock);
        while(!write_pipe_signal) {
            pthread_cond_wait(&read_keyboard_done, &write_signal_lock);
        }
        if(game_over) {
            closeClient();      
        }
        write_pipe_signal = false;
        strcpy(typed_word, keyboard_in);
        pthread_mutex_unlock(&write_signal_lock);
        typed_word[strlen(typed_word)-1] = '\0';
        
        if (strcmp(typed_word, "exit") == 0) {
            game_over = true;
            write(server_fifo, "e", 2);
        } else {
            strcpy(msg, "T");
            strcat(msg, typed_word);
            strcat(msg, "/");
            strcat(msg, pid);
            strcat(msg, "\0");
            
            write(server_fifo, &msg, strlen(msg)+1);
        }
    }

    unlink(PLAYER_FIFO);
    printf("Done unlink FIFO\n");
    printf("Left game, Close client\n");

    return 0;
}
