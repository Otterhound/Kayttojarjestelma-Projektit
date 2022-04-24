#define _POSIX_C_SOURCE 200809L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct file_and_size {
    char *name;
    char *file_in_memory;
    off_t size;
    int *newline_index;
    int newline_index_size;
} FILE_AND_SIZE; 


/* 
    Reads file, saves locations of new lines to int array,
    saves array and its size in struct. 
*/
void *markNewLines(void *arg) {
    FILE *file;
    char *line = NULL;
    size_t len = 0;
    ssize_t length;
    int size = 0;
    int position = 0;

    FILE_AND_SIZE *faz;
        if ((faz = (FILE_AND_SIZE*)malloc(sizeof(FILE_AND_SIZE))) == NULL)
        {
            fprintf(stderr ,"malloc failed\n");
            exit(1);
        }

    faz = (FILE_AND_SIZE*) arg;


    if ((file = fopen(faz->name, "r")) == NULL)
    {
        fprintf(stderr ,"reverse: cannot open file '%s'\n", faz->name);
        exit(1);
    }

    if ((faz->newline_index = (int*)malloc(size*sizeof(int))) == NULL)
        {
            fprintf(stderr ,"malloc failed\n");
            exit(1);
        }
    while ((length = getline(&line, &len, file)) != -1) {
        if (strcmp(&line[length]-1,"\n") == 0)
        {
            position = position + length -1;
            if (size > 0)
            {
                if ((faz->newline_index = (int*)realloc(faz->newline_index, size * sizeof(int))) == NULL)
                {
                    fprintf(stderr ,"realloc failed\n");
                    exit(1);
                }
            }
            faz->newline_index[size] = position;
            size++;
        }
    }
    faz->newline_index_size = size;
    fclose(file);
    free(line);
    return (void*) faz;
}

/*
    Reads file with mmap, saves mmap result and size to struct
*/
void *readFileToMMAP(void *arg) {
    int file;

    FILE_AND_SIZE *faz;
    if ((faz = (FILE_AND_SIZE*)malloc(sizeof(FILE_AND_SIZE))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    faz = (FILE_AND_SIZE*) arg;


    if ((file = open(faz->name, O_RDONLY, S_IRUSR | S_IWUSR)) < 0)
    {
        fprintf(stderr ,"pzip: cannot open file '%s'\n",faz->name);
        exit(1);
    }
    /*
    From: How to Map Files into Memory in C (mmap, memory mapped file io)
    youtube.com/watch?v=m7E9piHcfr4
    */
    struct stat file_stat;
    if (fstat(file,&file_stat) == -1)
    {
        fprintf(stderr ,"pzip: cannot get file '%s' size\n", faz->name);
    }
    char *file_in_memory = mmap(NULL, file_stat.st_size, PROT_READ,
    MAP_PRIVATE, file, 0);
    /*
    From: How to Map Files into Memory in C (mmap, memory mapped file io)
    youtube.com/watch?v=m7E9piHcfr4
    */
    if ((faz->file_in_memory = (char*)malloc(sizeof(file_stat.st_size))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }
    faz->file_in_memory = file_in_memory;
    faz->size = file_stat.st_size;
    return (void*) faz;   
}

void *pzip(void *arg) {
    /*
    run length encoding
    https://www.geeksforgeeks.org/run-length-encoding/
    */
    char *line = (char*) arg;
    char *comp_line;
    int length = strlen(line); 
    int worst_case_length = length*2+1;
    int index = 0, amount;
    char count[worst_case_length];
    // If all characters are different worst case length = line_length*2+1
    if ((comp_line = (char*)malloc(worst_case_length*sizeof(char))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    for (int i = 0; i < length; i++)
    {
        // New character
        comp_line[index++] = line[i];
        // Number of characters
        amount = 1;
        while (line[i] == line[i + 1] && i + 1 < length)
        {
            amount++;
            i++;
        }

        sprintf(count, "%d", amount);
        
        // Add amount to comp_string
        for (int j = 0; *(count + j) ; j++,index++)
        {
            comp_line[index] = count[j];
        }
    }
    comp_line[index] = *(char*) "\0";
    // run lenght encoding
    return (void*) comp_line;
}

/* 
void *writeFile(void *arg) {

    FILE_AND_SIZE *faz;
    if ((faz = (FILE_AND_SIZE*)malloc(sizeof(FILE_AND_SIZE))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    faz = (FILE_AND_SIZE*) arg;

    FILE *file;
    if ((file = open(faz->name, O_RDONLY, S_IRUSR | S_IWUSR)) < 0)
    {
        fprintf(stderr ,"pzip: cannot open file '%s'\n",faz->name);
        exit(1);
    }
    fprintf(file, "%c", faz->file_in_memory);
    return NULL;
}
*/

int main (int argc, char *argv[]) {
    // No files given
    if (argc == 1)
    {
        fprintf(stderr ,"pzip: file1 [file2...]\n");
        exit(1);
    }

    int threads_needed = argc - 1;
    FILE_AND_SIZE files[threads_needed];
    pthread_t threads[threads_needed];
    void *retvals[threads_needed];

    /*
        Creates threads equal to amount of given files
        Per thread: Runs markNewLines
    */
    for (int i = 0; i < threads_needed; i++)
        {
            FILE_AND_SIZE *faz;
            if ((faz = (FILE_AND_SIZE*)malloc(sizeof(FILE_AND_SIZE))) == NULL)
            {
                fprintf(stderr ,"malloc failed\n");
                exit(1);
            }
            if ((faz->name = (char*)malloc(sizeof(strlen(argv[i + 1])))) == NULL)
            {
                fprintf(stderr ,"malloc failed\n");
                exit(1);
            }

            faz->name = argv[i + 1];

            if (pthread_create(&threads[i], NULL, &markNewLines, faz) != 0) 
            {
                fprintf(stderr ,"failed to create thread\n");
                exit(1);
            }
        }
    for (int i = 0; i < threads_needed; i++)
    {
        if (pthread_join(threads[i], &retvals[i]) != 0) 
        {
            fprintf(stderr ,"failed to create thread\n");
            exit(1);
        }
        files[i] = *(FILE_AND_SIZE*) retvals[i];
    }

    // files to mmap
    /*
        Creates threads equal to amount of given files
        Per thread: Runs readFileToMMAP
    */
    FILE_AND_SIZE *tmp;
    for (int i = 0; i < threads_needed; i++)
    {
        tmp = &files[i];
        if (pthread_create(&threads[i], NULL, &readFileToMMAP, tmp) != 0) 
        {
            fprintf(stderr ,"failed to create thread\n");
            exit(1);
        }
    }
    for (int i = 0; i < threads_needed; i++)
    {
        if (pthread_join(threads[i], &retvals[i]) != 0) 
        {
            fprintf(stderr ,"failed to create thread\n");
            exit(1);
        }
        files[i] = *(FILE_AND_SIZE*) retvals[i];
    }

    // Zip
    /*
        Per file: Creates threads equal to size of new line array
        Per thread: Runs pzip
        Joins threads: Adds compressed lines together, which then replaces
        previous saved file content
    */
    for (int i = 0; i < threads_needed; i++)
    {

        tmp = &files[i];
        pthread_t threadsZip[tmp->newline_index_size];
        int copy_begin = 0;

        char *comp_file_content; 
        int current_size = 0;
        if ((comp_file_content = (char*)malloc(1*sizeof(char))) == NULL)
            {
                fprintf(stderr ,"malloc failed\n");
                exit(1);
            }


        for (int j = 0; j < tmp->newline_index_size; j++)
        {
            int new_line_location = tmp->newline_index[j];
            char line[new_line_location - copy_begin];

            memcpy(line, &tmp->file_in_memory[copy_begin], new_line_location);

            char *pline; 
            if ((pline = (char*)malloc(sizeof(strlen(line)))) == NULL)
                {
                    fprintf(stderr ,"malloc failed\n");
                    exit(1);
                }
            pline = line;

            

            if (pthread_create(&threadsZip[j], NULL, &pzip, pline) != 0) 
            {
                fprintf(stderr ,"failed to create thread\n");
                exit(1);
            }
            copy_begin = new_line_location + 1;
        }

        for (int j = 0; j < threads_needed; j++)
        {
            if (pthread_join(threadsZip[j], &retvals[j]) != 0) 
            {
                fprintf(stderr ,"failed to create thread\n");
                exit(1);
            }

            char *comp_line = (char*)retvals[i];

            current_size = current_size + strlen(comp_line);


            if ((comp_file_content = (char*)realloc(comp_file_content, current_size * sizeof(char))) == NULL)
                {
                    fprintf(stderr ,"realloc failed\n");
                    exit(1);
                }
            strcat(comp_file_content, comp_line);
        }


        if ((tmp->file_in_memory = (char*)realloc(tmp->file_in_memory, current_size * sizeof(char))) == NULL)
        {
            fprintf(stderr ,"realloc failed\n");
            exit(1);
        }
        tmp->file_in_memory = comp_file_content;
        files[i] = *tmp;
    }


    
/*  Test function
    Prints positions of new lines 
    for (int j = 0; j < argc-1; j++)
    {
        printf("File number %d\n",j);
        for (int i = 0; i < files[j].newline_index_size; i++)
        {
            printf("Newline %d\n", files[j].newline_index[i]);
        }   
    }
*/

/*  Test function
    Prints given files file content 
    for (int i = 0; i < argc - 1; i++)
    {
        for (int j = 0; j < files[i].size; j++)
        {
            printf("%c", files[i].file_in_memory[j]);
        }
    }
*/  
    return 0;
}