#define _POSIX_C_SOURCE 200809L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct thread_args {
    char **file_names;
    int file_amount;
    int files_handled;
    int mmaps;
    struct file_data **file_head;
    struct zip_data **zip_head;
    pthread_mutex_t lock;
    pthread_cond_t consumer_cond;
} THREAD_ARGS;

typedef struct file_data {
    char *file_in_memory;
    int index;
    struct file_data *next;
} FILE_DATA;

typedef struct char_and_amount {
    int amount;
    char *character;
    struct char_and_amount *next;
} CHAR_AND_AMOUNT;

typedef struct zip_data {
    struct char_and_amount **head, **tail;
    int index;
    struct zip_data *next;
} ZIP_DATA;

// Initialize thread_args
THREAD_ARGS *thread_args_init(THREAD_ARGS *thread_args, char **argv, int argc, int threads) {

    if ((thread_args = (THREAD_ARGS*)malloc(sizeof(THREAD_ARGS))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    if ((thread_args->file_head = (FILE_DATA**)malloc(sizeof(FILE_DATA))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    if ((thread_args->zip_head = (ZIP_DATA**)malloc(sizeof(ZIP_DATA))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    if ((thread_args->file_names = (char**)malloc((argc-1)*sizeof*thread_args->file_names)) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    // Copy filenames to thread_args
    for(int i = 1; i < argc; i++)
    {
        int length = strlen(argv[i]);
        if ((thread_args->file_names[i-1] = (char*)malloc(length*sizeof(char))) == NULL)
        {
            fprintf(stderr ,"malloc failed\n");
            exit(1);
        }
        memcpy(thread_args->file_names[i-1], argv[i], length);
    }

    thread_args->file_amount = argc - 1;
    thread_args->files_handled = 0;
    thread_args->mmaps = 0;

    if (pthread_mutex_init(&thread_args->lock, NULL) != 0) 
    {
        fprintf(stderr ,"pthread lock init failed\n");
        exit(1);
    }
    if (pthread_cond_init(&thread_args->consumer_cond, NULL) != 0) 
    {
        fprintf(stderr ,"pthread cond init failed\n");
        exit(1);
    }

    return thread_args;
}

// Free thread_args
void free_thread_args(THREAD_ARGS *thread_args) {
    free(thread_args->file_head);
    free(thread_args->zip_head);
    for (int i = 0; i < thread_args->file_amount; i++)
    {
        free(thread_args->file_names[i]);
    }
    free(thread_args->file_names);
    pthread_cond_destroy(&thread_args->consumer_cond);
    pthread_mutex_destroy(&thread_args->lock);
    free(thread_args);
}

// Inserts file_data as the first element of linked list
void insertNode(FILE_DATA **file_head, FILE_DATA *new_node) {
    if (*file_head == NULL)
    {
        (*file_head) = new_node;
    }
    else
    {
        new_node->next = (*file_head);
        (*file_head) = new_node;
    }
}

// Removes first elements of file_data linked list
void removeNode(FILE_DATA **file_head) {
    (*file_head) = (*file_head)->next;
}

// Inserts char_and_amount as the last element of linked list
CHAR_AND_AMOUNT *insertCharAndAmount(CHAR_AND_AMOUNT **head, CHAR_AND_AMOUNT **tail, CHAR_AND_AMOUNT *new_node, CHAR_AND_AMOUNT *pre_node) {
    if (pre_node == NULL)
    {
        (*head) = new_node;
        (*tail) = new_node;
    }
    else
    {
        pre_node->next = new_node;
        (*tail) = new_node;
    }
    return new_node;
}

// Inserts zip_data in the order as args were given to linked list
void insertNodeZip(ZIP_DATA **zip_head, ZIP_DATA *new_node) {   
    if (*zip_head == NULL)
    {
        (*zip_head) = new_node;
    }
    else if (new_node->index < (*zip_head)->index)
    {
        new_node->next = (*zip_head);
        (*zip_head) = new_node;
    }
    else if (new_node->index > (*zip_head)->index)
    {
        ZIP_DATA *tmp = (*zip_head);
        while (new_node->index > tmp->index && tmp->next != NULL)
        {
            tmp = tmp->next;
        }
        if (tmp->next == NULL)
        {
            tmp->next = new_node;
        }
        else 
        {
            new_node->next = tmp->next;
            tmp->next = new_node;
        }
    } 
}

/*
    Reads file with mmap, saves mmap result and size to struct
*/
FILE_DATA *readFileToMMAP(FILE_DATA **file_head, char *filename, int index) {

    int file;
    FILE_DATA *new_node = NULL;

    if ((new_node = (FILE_DATA*)malloc(sizeof(FILE_DATA))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    /*
    FROM: How to Map Files into Memory in c (mmap, memory mapped file io)
    youtube.com/watch?v=m7E9piHcfr4
    */
    if ((file = open(filename, O_RDONLY, S_IRUSR | S_IWUSR)) < 0)
    {
        fprintf(stderr ,"pzip: cannot open file '%s'\n",filename);
        exit(1);
    }

    struct stat file_stat;
    if (fstat(file,&file_stat) == -1)
    {
        fprintf(stderr ,"pzip: cannot get file '%s' size\n", filename);
    }

    new_node->file_in_memory = mmap(NULL, file_stat.st_size, PROT_READ, MAP_PRIVATE, file, 0);
    new_node->index = index;

    return new_node;  
}

// run length encodes given node->file_in_memory 
ZIP_DATA *pzip(FILE_DATA *node) {

    int length = strlen(node->file_in_memory); 
    int amount = 0;

    ZIP_DATA *new_node = NULL;
    if ((new_node = (ZIP_DATA*)malloc(sizeof(ZIP_DATA))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    if ((new_node->head = (CHAR_AND_AMOUNT**)malloc(sizeof(CHAR_AND_AMOUNT))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    if ((new_node->tail = (CHAR_AND_AMOUNT**)malloc(sizeof(CHAR_AND_AMOUNT))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }

    CHAR_AND_AMOUNT *pre_node = NULL;
    for (int i = 0; i < length; i++)
    {

        CHAR_AND_AMOUNT *char_and_amount = NULL;
        if ((char_and_amount= (CHAR_AND_AMOUNT*)malloc(sizeof(CHAR_AND_AMOUNT))) == NULL)
        {   
        fprintf(stderr ,"malloc failed\n");
        exit(1);
        }

        if ((char_and_amount->character= (char*)malloc(sizeof(char))) == NULL)
        {   
        fprintf(stderr ,"malloc failed\n");
        exit(1);
        }

        // New character
        strncpy(char_and_amount->character, &node->file_in_memory[i], 1);
     
        // Number of characters
        amount = 1;
        while (node->file_in_memory[i] == node->file_in_memory[i + 1] && i + 1 < length)
        {
            amount++;
            i++;
        }

        char_and_amount->amount = amount;

        pre_node = insertCharAndAmount(new_node->head, new_node->tail, char_and_amount, pre_node);
    }

    new_node->index = node->index;
    
    free(node);
    return new_node;
}

/* If multiple files are given checks if file ends with
   same character as next one begins. */
void cmpHeadTail(ZIP_DATA **zip_head) {

    ZIP_DATA *current_zip_node = NULL;
    ZIP_DATA *next_zip_node = NULL;
    ZIP_DATA *tmp_zip = NULL;
    CHAR_AND_AMOUNT **current_tail = NULL;
    CHAR_AND_AMOUNT **next_head = NULL;
    CHAR_AND_AMOUNT **next_tail = NULL;
    CHAR_AND_AMOUNT *tmp_char = NULL;
    
    current_zip_node = (*zip_head);
    next_zip_node = (*zip_head)->next;

    while (next_zip_node != NULL)
    {
        current_tail = current_zip_node->tail;
        next_head = next_zip_node->head;
        next_tail = next_zip_node->tail;

        // file ends with same character as next one begins
        if (strcmp((*current_tail)->character, (*next_head)->character) == 0)
        {
            (*current_tail)->amount += (*next_head)->amount;
            // Char and amount list only has one element
            if ((*next_head) == (*next_tail)) 
            {
                tmp_zip = next_zip_node;
                next_zip_node = next_zip_node->next;
                current_zip_node->next = next_zip_node;

                free((*next_head)->character);
                free(next_head);
                free(next_tail);
                free(tmp_zip);
            }
            else
            {
                // Char and amount list has more than one element
                tmp_char = (*next_head);
                (*next_head) = (*next_head)->next;

                free(tmp_char->character);
                free(tmp_char);

                current_zip_node = current_zip_node->next;
                next_zip_node = next_zip_node->next;
            }  
        }
        else
        {
            // File ends with differet character than the next one begins
            current_zip_node = current_zip_node->next;
            next_zip_node = next_zip_node->next;
        } 
    }
}

// Writes amount as 4-byte integer and character in ASCII
void writeFile(ZIP_DATA **zip_head) {

    ZIP_DATA *node = NULL;
    CHAR_AND_AMOUNT **head = NULL;
    CHAR_AND_AMOUNT *char_and_amount = NULL;

    while ((*zip_head) != NULL)
    {
        node = (*zip_head);
        head = node->head;
    
        while ((*head) != NULL)
        {
            char_and_amount = (*head); 
            fwrite((int*) &char_and_amount->amount, sizeof(int), 1, stdout);
            fwrite((char*) char_and_amount->character, sizeof(char), 1, stdout);
            free(char_and_amount->character);
            free(char_and_amount);
            (*head) = (*head)->next;
        }
        (*zip_head) = (*zip_head)->next;
        free(node->head);
        free(node->tail);
        free(node);
    } 
}

/* Reads files to mmap after which signals sleeping consumer threads */
void *producer(void *arg) {
    THREAD_ARGS *thread_args = (THREAD_ARGS*) arg;
    for (int i = 0; i < thread_args->file_amount; i++)
    {
        FILE_DATA *new_node = readFileToMMAP(thread_args->file_head, thread_args->file_names[i], i);
        pthread_mutex_lock(&thread_args->lock);
        insertNode(thread_args->file_head, new_node);
        thread_args->mmaps += 1;
        pthread_cond_broadcast(&thread_args->consumer_cond);
        pthread_mutex_unlock(&thread_args->lock);
    }
    return NULL;
}

/* Sleeps if current amount of mmaps is 0 and all files are not compressed.
   Otherwise takes out mmaps from thread args and compresses it. */
void *consumer(void *arg) {
    THREAD_ARGS *thread_args = (THREAD_ARGS*) arg;
    while (thread_args->files_handled < thread_args->file_amount)
    {
        pthread_mutex_lock(&thread_args->lock);
        while (thread_args->mmaps == 0 && thread_args->files_handled < thread_args->file_amount)
        {
            pthread_cond_wait(&thread_args->consumer_cond, &thread_args->lock);
        }
        
        if (thread_args->mmaps > 0 && thread_args->files_handled < thread_args->file_amount)
        {
            FILE_DATA **file_head = thread_args->file_head;
            FILE_DATA *node = (*file_head);
            removeNode(thread_args->file_head);
            thread_args->mmaps -= 1;
            thread_args->files_handled += 1;

            pthread_mutex_unlock(&thread_args->lock);

            ZIP_DATA *new_node = pzip(node);

            pthread_mutex_lock(&thread_args->lock);

            insertNodeZip(thread_args->zip_head, new_node);

            pthread_mutex_unlock(&thread_args->lock);

        }

        pthread_mutex_unlock(&thread_args->lock);
            
    }
    return NULL;
}

int main (int argc, char *argv[]) {

    // No files given
    if (argc < 2)
    {
        fprintf(stdout ,"wzip: file1 [file2 ...]\n");
        exit(1);
    }

    // One producer thread and consumer thread == number of avaible processors 
    int threads = get_nprocs();
    pthread_t pthread, cthread[threads];

    THREAD_ARGS *thread_args = NULL;
    thread_args = thread_args_init(thread_args, argv, argc, threads);

    pthread_create(&pthread, NULL, producer, (void*) thread_args);
    
    for (int i = 0; i < threads; i++)
    {
        pthread_create(&cthread[i], NULL, consumer, (void*) thread_args);
    }
    
    for (int i = 0; i < threads; i++)
    {
        pthread_join(cthread[i], NULL);
    }

    pthread_join(pthread, NULL);

    cmpHeadTail(thread_args->zip_head);

    writeFile(thread_args->zip_head);
    
    free_thread_args(thread_args);

    return 0;
}