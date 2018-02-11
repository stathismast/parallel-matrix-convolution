#include <stdio.h>
#include <stdlib.h>

#define X 1920
#define Y 2520

//Applies a filter to every pixel except
//those that are om the edges
void applyFilter(char ** c){

    //Allocate space for a copy of the given 2d array
    char** copy;
    copy = malloc(X*sizeof(char *));
    for(int i=0; i<X; i++){
        copy[i] = malloc(Y*sizeof(char));
    }

    //Copy every pixel into the new 2D array
    for(int i=0; i<Y; i++){
        for(int j=0; j<X; j++){
            copy[j][i] = c[j][i];
        }
    }

    //Apply filter
    for(int i=1; i<Y-1; i++){
        for(int j=1; j<X-1; j++){
            c[j][i] = (unsigned char)copy[j-1][i-1]*((double)1/16)
                    + (unsigned char)copy[j][i-1]*((double)2/16)
                    + (unsigned char)copy[j+1][i-1]*((double)1/16)
                    + (unsigned char)copy[j-1][i]*((double)2/16)
                    + (unsigned char)copy[j][i]*((double)4/16)
                    + (unsigned char)copy[j+1][i]*((double)2/16)
                    + (unsigned char)copy[j-1][i+1]*((double)1/16)
                    + (unsigned char)copy[j][i+1]*((double)2/16)
                    + (unsigned char)copy[j+1][i+1]*((double)1/16);
        }
    }

    //I should be freeing malloc'd memory here
}


int main(void){
    char fileName[28] = "waterfall_grey_1920_2520.raw";

    FILE *file = fopen(fileName, "r");

    //Allocate enough space for every pixel
    char ** c;
    c = malloc(X*sizeof(char *));
    for(int i=0; i<X; i++){
        c[i] = malloc(Y*sizeof(char));
    }

    //load every pixel into a 2D array
    for(int i=0; i<Y; i++){
        for(int j=0; j<X; j++){
            c[j][i] = fgetc(file);
        }
    }

    //Apply filter 10 times
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);
    applyFilter(c);

    //Print every pixel in order
    for(int i=0; i<Y; i++){
        for(int j=0; j<X; j++){
            printf("%c", c[j][i]);
        }
    }

    return 0;
}
