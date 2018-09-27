#include <sys/types.h> //mkfifo
#include <sys/stat.h>  //mkfifo
#include <stdio.h>     // printf
#include <fcntl.h>     // open
#include <unistd.h>    // unlink
#include <stdlib.h>    // exit
#include <stdbool.h>   // bool
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "words.h"

#define BUFFSIZE 64
#define NUMPLAYERS 2
#define NUMQUIZ 10

pthread_mutex_t signal_lock[NUMPLAYERS];
pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t common_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t npc_signal[NUMPLAYERS];
bool npc_signaled[NUMPLAYERS];
pthread_t threads[NUMPLAYERS];

char PLAYER_FIFO[NUMPLAYERS][255];
const char SERVER_FIFO[] = "server_pipe"; 
int server_fifo;
int player_registerd = 0;
char* random_words[NUMQUIZ];
bool game_over = false;
bool player_left = false;
bool server_down = false;

char* player_id[NUMPLAYERS];

// -1 => no winner, 0 => player 1 win, 1 => player 2 win, etc...
int winner = -1; 
char player_answer[NUMPLAYERS][BUFFSIZE];

void serverDown() {
    printf("Server Down\n");
    int i;
    pthread_mutex_lock(&common_lock);
    server_down = true;
    game_over = true;
    pthread_mutex_unlock(&common_lock);
    for(i = 0; i < player_registerd; i++) {
        pthread_cond_signal(&npc_signal[i]);
        pthread_join(threads[i], NULL);
    }
}

void handler(int arg) {
    serverDown();
    unlink(SERVER_FIFO);
    printf("Unlink server FIFO\n");
    printf("Close server\n");
    
    exit(0);
}

void initializer() {
    int i;
    for(i = 0; i < NUMPLAYERS; i++) {
        pthread_mutex_init(&signal_lock[i], NULL);
        pthread_cond_init(&npc_signal[i], NULL);
        npc_signaled[i] = false;
    }
    signal(SIGINT, handler);
    signal(SIGTERM, handler);
}

void destroier() {
    int i;
    for(i = 0; i < NUMPLAYERS; i++) {
        free(player_id[i]);
        pthread_cond_destroy(&npc_signal[i]);
        pthread_mutex_destroy(&signal_lock[i]);
    }
    pthread_mutex_destroy(&print_lock);
    pthread_mutex_destroy(&common_lock);
    unlink(SERVER_FIFO);
    printf("Done unlink server FIFO\n");
}

void* npc(void* args) {
    int counter = 0;
    char attempt[BUFFSIZE];
    char msg[BUFFSIZE];
    bool count_down = false;

    // identify each player
    int identifier = *((int*) args);
    free(args);

    // connect to the player pipe
	int player_fifo = open(PLAYER_FIFO[identifier], O_RDWR);
	if (player_fifo < 0) {
		perror("Unable to open named pipe");
		exit(-1);
	}

    while(!game_over) {
        pthread_mutex_lock(&signal_lock[identifier]);
        while(!npc_signaled[identifier]){
            pthread_cond_wait(&npc_signal[identifier], &signal_lock[identifier]);
            if(game_over) {
                // Sending message to indicate game over and kill client
                if(winner > -1)
                    write(player_fifo, "L", 2);
                else if(player_left) 
                    write(player_fifo, "e", 2);
                else if(server_down)
                    write(player_fifo, "D", 2);
                pthread_mutex_unlock(&signal_lock[identifier]);
                pthread_exit(NULL);
            }
        }

        if(!count_down) {
            // Do count down in the first round
            npc_signaled[identifier] = false;
            count_down = true;
            pthread_mutex_unlock(&signal_lock[identifier]);
            // Send count dowm msg to client
            write(player_fifo, "C", 2);

            // Send word to player
            strcpy(msg, "W");
            strcat(msg, random_words[counter]);
            strcat(msg, "\0");
            write(player_fifo, &msg, strlen(msg)+1);
        } else {
            npc_signaled[identifier] = false;
            strcpy(attempt, player_answer[identifier]);
            pthread_mutex_unlock(&signal_lock[identifier]);

            // Check if the word typed is correct
            if(strcmp(random_words[counter], attempt) == 0) {
                // Good attempt
                counter++;
                if(counter == NUMQUIZ) {
                    // WIN
                    pthread_mutex_lock(&common_lock);
                    game_over = true;
                    winner = identifier;
                    pthread_mutex_unlock(&common_lock);

                    write(player_fifo, "V", 2);
                    pthread_mutex_lock(&print_lock);
                    printf("Player %s WIIIIIIIN!!!!\n", player_id[winner]);
                    pthread_mutex_unlock(&print_lock);
                    pthread_exit(NULL);
                } else {
                    // Send next word
                    strcpy(msg, "W");
                    strcat(msg, random_words[counter]);
                    strcat(msg, "\0");
                    write(player_fifo, &msg, strlen(msg)+1);
                }
            } else {
                // Bad attempt, sending error protocol
                write(player_fifo, "E", strlen(msg)+1);
            }
        }        
    }
    pthread_exit(NULL);
}

void generate_random_words() {
    int i;
    int words_available = sizeof(words)/sizeof(char*);
    for(i = 0; i < NUMQUIZ; i++) {
        random_words[i] = words[rand() % words_available];
    }
}

int main(){
    srand(time(NULL));
	char buf[BUFFSIZE], id[BUFFSIZE], word_typed[BUFFSIZE];;
    int counter, i;
    int* identifier;

    initializer();
    generate_random_words();

	// create the server pipe
 	int result = mkfifo(SERVER_FIFO, 0600);
	if (result) {
		perror("Unable to create named pipe");
		exit(EXIT_FAILURE);
	}

	// connect to the server pipe
	printf("starting game, waiting for player...\n");
	server_fifo = open(SERVER_FIFO, O_RDONLY);
    if (server_fifo < 0) {
		perror("Unable to open named pipe");
		exit(-1);
	}
	
    // waiting for palyer registration
	while(player_registerd < NUMPLAYERS && !game_over) {
		result = read(server_fifo, &buf, 1);
		if (result < 0) {
			perror("ERROR: Error reading from pipe");
			exit(EXIT_FAILURE);
		} else if ('R' == buf[0]){
            counter = 0;
            read(server_fifo, &buf, 1);
            while('\0' != buf[0]) {
                id[counter++] = buf[0];
                read(server_fifo, &buf, 1);
            }
            id[counter] = '\0';

            strcpy(PLAYER_FIFO[player_registerd], "player_pipe_");
            strcat(PLAYER_FIFO[player_registerd], id);
            player_id[player_registerd] = strdup(id);

            pthread_mutex_lock(&print_lock);
            printf("Player %d id %s registerd\n", player_registerd,player_id[player_registerd]);
            pthread_mutex_unlock(&print_lock);

            // Create thread for player
            identifier = malloc(sizeof(int*));
            *identifier = player_registerd;
            pthread_create( &threads[player_registerd], NULL, &npc, identifier);

            player_registerd++;
        } else if ('e' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("One player left the game\n");
            pthread_mutex_unlock(&print_lock);

			game_over = true;
            player_left = true;
		}
	}

    // Game start and signal all threads
    if(!game_over) {
        pthread_mutex_lock(&print_lock);
        printf("Game start\n\n");
        pthread_mutex_unlock(&print_lock);
    }
    
    for(i = 0; i < NUMPLAYERS; i++) {
        pthread_mutex_lock(&signal_lock[i]);
        pthread_cond_signal(&npc_signal[i]);
        npc_signaled[i] = true;                    
        pthread_mutex_unlock(&signal_lock[i]);
    }
    
    while(!game_over) {
        result = read(server_fifo, &buf, 1);
		if (result < 0) {
			perror("ERROR: Error reading from pipe");
			exit(EXIT_FAILURE);
		} else if (result == 0){
            pthread_mutex_lock(&print_lock);
            printf("All players left the game\n");
            pthread_mutex_unlock(&print_lock);
			game_over = true;
		} else if ('e' == buf[0]) {
            pthread_mutex_lock(&print_lock);
            printf("One player left the game\n");
            pthread_mutex_unlock(&print_lock);
			game_over = true;
            player_left = true;
		} else if('T' == buf[0]) {
            read(server_fifo, &buf, 1);
            counter = 0;
            // process word typed
            while('/' != buf[0]) {
                word_typed[counter++] = buf[0];
                read(server_fifo, &buf, 1);
            }
            word_typed[counter] = '\0';
            
            read(server_fifo, &buf, 1);
            counter = 0;
            while('\0' != buf[0]) {
                id[counter++] = buf[0];
                read(server_fifo, &buf, 1);
            }
            id[counter] = '\0';           

            for(i = 0; i < NUMPLAYERS; i++) {
                if(strcmp(id, player_id[i]) == 0) {
                    pthread_mutex_lock(&print_lock);
                    printf("Player %d id %s typed: %s\n", i, id, word_typed);
                    pthread_mutex_unlock(&print_lock);
                    pthread_mutex_lock(&signal_lock[i]);
                    strcpy(player_answer[i], word_typed);
                    npc_signaled[i] = true;
                    pthread_cond_signal(&npc_signal[i]);
                    pthread_mutex_unlock(&signal_lock[i]);
                }
            }
        }
    }

    // clean threads
    for(i = 0; i < NUMPLAYERS; i++) {
        pthread_cond_signal(&npc_signal[i]);
    }
    
    for(i = 0; i < NUMPLAYERS; i++) {
        pthread_join(threads[i], NULL);
    }

    destroier();

    return 0;
}
