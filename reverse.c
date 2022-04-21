#define _POSIX_C_SOURCE 200809L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef struct list {
    char *line;
    struct list *pNext, *pLast;
} LIST;

void addNode(LIST **pStart, LIST **pEnd,char *line, size_t len) {
    LIST *pNew, *ptr;
    if ((pNew = (LIST*)malloc(sizeof(LIST))) == NULL)
        {
            fprintf(stderr ,"malloc failed\n");
            exit(1);
        }
    if ((pNew->line = (char*)malloc(sizeof(len))) == NULL)
    {
        fprintf(stderr ,"malloc failed\n");
        exit(1);
    }
    strcpy(pNew->line, line);
    pNew->pNext = NULL;
    pNew->pLast = NULL;
    if ((*pStart) == NULL)
    { 
        (*pStart) = pNew;
        (*pEnd) = pNew;
    }
    else
    {
        ptr = (*pStart);
        while (ptr->pNext != NULL)
        {
            ptr = ptr->pNext;
        }
        ptr->pNext = pNew;
        pNew->pLast = ptr;
        (*pEnd) = pNew;
    }
}

void deleteList(LIST *pStart) {
    LIST *ptr = pStart;
    while (ptr != NULL)
    {
        pStart = ptr->pNext;
        free(ptr->line);
        free(ptr);
        ptr = pStart;
    }
}

void readFile(LIST **pStart, LIST **pEnd, char *name) {
    FILE *file;
    char *line = NULL;
    size_t len = 0;
    if ((file = fopen(name, "r")) == NULL)
    {
        fprintf(stderr ,"reverse: cannot open file '%s'\n", name);
        exit(1);
    }
    while (getline(&line, &len, file) != -1) {
        addNode(pStart, pEnd,line, len);
    }
    fclose(file);
    free(line);
}

void writeFile(LIST *pEnd, char *name) {
    FILE *file;
    if ((file = fopen(name, "w")) == NULL)
    {
        fprintf(stderr ,"reverse: cannot open file '%s'\n",name);
        exit(1);
    }
    LIST *ptr = pEnd;
    while (ptr != NULL)
    {
        fprintf(file, "%s",ptr->line);
        ptr = ptr->pLast;
    }
    fclose(file);
}

int isSameFile(char *name1, char* name2) {
    int check = -1;
    /* FROM stack overflow getting file inode number
    Source:
    stackoverflow.com/questions/9480568/find-inode-number-of-a-file-using-c-code
    */
    int file1, file2;
    if ((file1 = open(name1, O_RDWR)) < 0)
    {
        fprintf(stderr ,"reverse: cannot open file '%s'\n",name1);
        exit(1);
    }
    if ((file2 = open(name2, O_RDWR)) < 0)
    {
        fprintf(stderr ,"reverse: cannot open file '%s'\n",name2);
        exit(1);
    }
    struct stat file_stat1, file_stat2;
    int ret;
    if ((ret = fstat(file1, &file_stat1)) < 0)
    {
        fprintf(stderr ,"File stat failed\n");
        exit(1);
    }
    if ((ret = fstat(file2, &file_stat2)) < 0)
    {
        fprintf(stderr ,"File stat failed\n");
        exit(1);
    }
    if (file_stat1.st_ino == file_stat2.st_ino)
    {
        check = 1;
    }
    else
    {
        check = 0;
    }
    // Stack overflow end
    return check;
}


void readStdin(LIST **pStart, LIST **pEnd) {
    char *line = NULL;
    size_t len = 0;
    // Stop input with ctrl+d
    while (getline(&line, &len, stdin) != -1) {
        addNode(pStart, pEnd,line, len);
    }
    free(line);
}

void writeStdout(LIST *pEnd) {
    LIST *ptr = pEnd;
    while (ptr != NULL)
    {
        fprintf(stdout, "%s",ptr->line);
        ptr = ptr->pLast;
    }
}

int main(int argc, char *argv[]) {
    LIST *pStart = NULL, *pEnd = NULL;
    if (argc > 3)
    {
        fprintf(stderr ,"usage: reverse <input> <output>\n");
        exit(1);
    }
    else if (argc == 3) {
        if (isSameFile(argv[1], argv[2]) == 1) {
            fprintf(stderr ,"reverse: input and output file must differ\n");
            exit(1);
        }
        readFile(&pStart, &pEnd,argv[1]);
        writeFile(pEnd, argv[2]);
    }
    else if (argc == 2) {
        readFile(&pStart, &pEnd, argv[1]);
        writeStdout(pEnd);
    }
    else {
        readStdin(&pStart, &pEnd);
        writeStdout(pEnd);
    }
    deleteList(pStart);
    return 0;
}


