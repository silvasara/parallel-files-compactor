#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ftw.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define MAX_FILES 100000
#define MAX_SIZE 1000
#define NUM_PRODUCERS 1
#define NUM_CONSUMERS 8

typedef struct {
    char *name;
} File;

char *input_dir = NULL;
char *output_dir = NULL;

// Critical Region, buffer
File buffer[MAX_FILES];
int used = -1;

pthread_mutex_t mux_buffer;

// Identify when the producer has finished
int producer_finished = 0;

pthread_t producers[NUM_PRODUCERS];
pthread_t consumers[NUM_CONSUMERS];

void inc_consumer();
void dec_consumer();
void compress_file(char *filename);
void *to_out_path(char *source, char *out_path);

void *consume_from_stack(){
    fprintf(stderr, "Start  %d\n", used);
    
    while(!producer_finished || used >= 0){
        int has_file = 0;
        char name[MAX_SIZE];

        pthread_mutex_lock(&mux_buffer); // Begin
        if(used >= 0){
            strcpy(name, buffer[used].name);
            free(buffer[used].name);

            used--;
            has_file = 1;
        }
        pthread_mutex_unlock(&mux_buffer); // End

        if (has_file) {
            compress_file(name);
        }
    }

    fprintf(stderr, "End %d\n", used);
}

void add_to_stack(char *filename){
    pthread_mutex_lock(&mux_buffer); // Begin

    used++;
    buffer[used].name = malloc(strlen(filename) + 1);
    strcpy(buffer[used].name, filename);

    pthread_mutex_unlock(&mux_buffer); // End
}

void *to_out_path(char *source, char *out_path){
    strcpy(out_path, output_dir);
    strcat(out_path, source + strlen(input_dir));
}

void compress_file(char *filename){
    char command[MAX_SIZE] = "";

    char out_path[MAX_SIZE];
    to_out_path(filename, out_path);

    sprintf(command, "bzip2 -kc %s > %s.bz2", filename, out_path);

    FILE *file = popen(command, "r");
    pclose(file);
}

void tar_folder(){
    char command[MAX_SIZE] = "";

    sprintf(command, "tar cf %s.tar %s", output_dir, output_dir);

    FILE *file = popen(command, "r");
    pclose(file);
}

void produce_aux(char *prefix, char *lookup_dir);

void* produce(void *dirname_arg){
    char *dirname = (char *) dirname_arg;
    produce_aux("", dirname);
}

void produce_aux(char *prefix, char *lookup_dir){
    struct dirent *dp = NULL;
    DIR *dir_ptr = NULL;

    char input_dir[MAX_SIZE];

    if(strcmp(prefix, "") != 0){
        sprintf(input_dir, "%s/%s", prefix, lookup_dir);
    }
    else{
        strcpy(input_dir, lookup_dir);
    }

    if((dir_ptr = opendir(input_dir)) == NULL){
        fprintf(stderr, "Can't open %s\n", input_dir);
        exit(1);
    }
    
    char filename[MAX_SIZE];

    while((dp = readdir(dir_ptr)) != NULL){
        struct stat stbuf;

        sprintf(filename , "%s/%s", input_dir, dp->d_name);

        if(stat(filename, &stbuf) == -1){ // bad
            printf("Unable to stat file: %s\n", filename) ;
            continue;
        }

        char filename[MAX_SIZE];
        sprintf(filename, "%s/%s", input_dir, dp->d_name);

        if((stbuf.st_mode & S_IFMT) == S_IFDIR){ // It's a directory 
            if(strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){
                // Create subdirs
                char out_path[MAX_SIZE];
                to_out_path(filename, out_path);
                mkdir(out_path, 0700);

                produce_aux(input_dir, dp->d_name);
            }
        }
        else{ // It's a file
            add_to_stack(filename); // Thread-safe
        }
    }

    producer_finished = 1;
    closedir(dir_ptr);
}

int rm_r_aux(const char *path, const struct stat *stat, int tflags, struct FTW *ftwbuf){
    remove(path);
}

int main(int argc, char** argv){
    input_dir = argv[1];
    output_dir = argv[2];

    if(pthread_mutex_init(&mux_buffer, NULL) != 0){
        fprintf(stderr, "Fail initing mutex!\n");
    }

    output_dir[strlen(output_dir)-4] = '\0'; // remove .tar

    if(argc <= 2){
        printf("Usage: %s [dirname] [output_dir]\n", argv[0]);
        return 1;
    }

    mkdir(output_dir, 0700);

    for(int i = 0; i < NUM_PRODUCERS; ++i){
        pthread_create(&producers[i], NULL, &produce, input_dir);
    }
    
    for(int i = 0; i < NUM_CONSUMERS; ++i){
        pthread_create(&consumers[i], NULL, &consume_from_stack, input_dir);
    }

    for(int i = 0; i < NUM_CONSUMERS; ++i){
        pthread_join(consumers[i], NULL);
    }

    for(int i = 0; i < NUM_PRODUCERS; ++i){
        pthread_join(producers[i], NULL);
    }

    tar_folder();
    nftw(output_dir, rm_r_aux, 64, FTW_DEPTH); // delete the temporary directory

    return 0;
}

