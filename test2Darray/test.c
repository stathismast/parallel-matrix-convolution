#include <stdio.h>
#include <stdlib.h>

int ** get2DArray(int rows, int cols){
    int * space = malloc(rows * cols * sizeof(int));
    int ** array = malloc(rows * sizeof(int*));

    for(int i=0; i<rows; i++){
        array[i] = space+i*cols;
    }

    return array;
}

void free2DArray(int ** array){
    free(*array);
    free(array);
}

int main(void){

    int rows = 3;
    int cols = 6;
    int ** array = get2DArray(rows,cols);

    int (*array2D) [cols] = malloc( sizeof(int[rows][cols]) );

    for(int i=0; i<rows; i++){
        for(int j=0; j<cols; j++){
            array[i][j] = i+j;
        }
    }

    for(int i=0; i<rows; i++){
        for(int j=0; j<cols; j++){
            array2D[i][j] = i+j;
        }
    }

    for(int i=0; i<rows; i++){
       for(int j=0; j<cols; j++){
           printf("%d", array[i][j]);
       }
       printf("\n");
    }

    for(int i=0; i<rows; i++){
       for(int j=0; j<cols; j++){
           printf("%d", array2D[i][j]);
       }
       printf("\n");
    }

    printf("\nMy first pointer  : %d\nMy second pointer : %d\n\n", (unsigned int)array[0], (unsigned int)array[1]);
    printf("\nTheir first pointer  : %d\nTheir second pointer : %d\n\n", (unsigned int)array2D[0], (unsigned int)array2D[1]);

    free2DArray(array);
}
