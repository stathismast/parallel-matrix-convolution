#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NROWS 2520
#define NCOLS 1920

#define TOPLEFT     my_rank-sqrt_comm_sz-1
#define TOP         my_rank-sqrt_comm_sz
#define TOPRIGHT    my_rank-sqrt_comm_sz+1
#define LEFT        my_rank-1
#define RIGHT       my_rank+1
#define BOTTOMLEFT  my_rank+sqrt_comm_sz-1
#define BOTTOM      my_rank+sqrt_comm_sz
#define BOTTOMRIGHT my_rank+sqrt_comm_sz+1

unsigned char ** get2DArray(int rows, int cols){
    unsigned char * space = malloc(rows * cols * sizeof(unsigned char));
    unsigned char ** array = malloc(rows * sizeof(unsigned char*));

    for(int i=0; i<rows; i++){
        array[i] = space+i*cols;
    }

    return array;
}

//Applies a filter to every pixel except
//those that are om the edges
void * applyFilter(void * v, int rows, int cols){

    unsigned char ** c = v;

    //Allocate space for a copy of the given 2d array
    unsigned char ** filtered = get2DArray(rows, cols);

    //Apply filter
    for(int i=1; i<rows-1; i++){
        for(int j=1; j<cols-1; j++){
            filtered[i][j] = c[i-1][j-1]*((double)1/16)
                           + c[i][j-1]*((double)2/16)
                           + c[i+1][j-1]*((double)1/16)
                           + c[i-1][j]*((double)2/16)
                           + c[i][j]*((double)4/16)
                           + c[i+1][j]*((double)2/16)
                           + c[i-1][j+1]*((double)1/16)
                           + c[i][j+1]*((double)2/16)
                           + c[i+1][j+1]*((double)1/16);
        }
    }

    //I should be freeing malloc'd memory here
    return filtered;
}

int main(int argc, char *argv[]) {
	int comm_sz;               /* Number of processes    */
	int my_rank;               /* My process rank        */

	/* Subarray type */
	MPI_Datatype type,resizedtype;

	MPI_Request request;
	MPI_Status status;

	MPI_Datatype column;
	MPI_Datatype row;

	int sqrt_comm_sz;

	unsigned char **array2D; /* original array */
	unsigned char **final2D;

	unsigned char **myArray; /* local array of each process */

	unsigned char *topRow;
	unsigned char *bottomRow;

	unsigned char *leftCol;
	unsigned char *rightCol;

	unsigned char leftUpCorn,rightUpCorn,rightDownCorn,leftDownCorn;

	int i,j,k;

	/* Arguments of create_subarray */
	int sizes[2]; /* size of original array */
	int subsizes[2]; /* size of subArrays */
	int starts[2] = {0,0}; /* first subArray starts from index [0][0] */

	/* Arguments of Scatterv */
	int *counts; /* How many pieces of data everyone has in units of block */

	int *displs; /* The starting point of everyone's data in the global array, in block extents ( NCOLS/sqrt_comm_sz ints )  */

	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	sqrt_comm_sz = sqrt(comm_sz);

	topRow = malloc( (NCOLS/sqrt_comm_sz) * sizeof(unsigned char) );
	bottomRow = malloc( (NCOLS/sqrt_comm_sz) * sizeof(unsigned char) );

	leftCol = malloc( (NROWS/sqrt_comm_sz) * sizeof(unsigned char) );
	rightCol = malloc( (NROWS/sqrt_comm_sz) * sizeof(unsigned char) );

	array2D = get2DArray(NROWS,NCOLS);
	final2D = get2DArray(NROWS,NCOLS);

	myArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);

	/* Arguments of create_subarray */
	sizes[0] = NROWS;
	sizes[1] = NCOLS;
	subsizes[0] = NROWS/sqrt_comm_sz;
	subsizes[1] = NCOLS/sqrt_comm_sz;

	counts = malloc( comm_sz * sizeof(int) ); /* How many pieces of data everyone has in units of block */
	for ( i=0; i<comm_sz; i++ ){
		counts[i] = 1;
	}

	displs = malloc( comm_sz * sizeof(int) );
	k=0;
	for( i=0; i<sqrt_comm_sz; i++ ){
		for( j=0; j<sqrt_comm_sz; j++ ){
			displs[k] = i*NROWS+j;
			k++;
		}
	}

	/* Creating derived Datatype */
	MPI_Type_create_subarray(2,sizes,subsizes,starts,MPI_ORDER_C,MPI_UNSIGNED_CHAR,&type);
	MPI_Type_create_resized(type,0,(NCOLS/sqrt_comm_sz)*sizeof(unsigned char),&resizedtype);
	MPI_Type_commit(&resizedtype);

	MPI_Type_contiguous(NCOLS/sqrt_comm_sz,MPI_UNSIGNED_CHAR,&row);
	MPI_Type_commit(&row);

	MPI_Type_vector(NROWS/sqrt_comm_sz,1,NCOLS/sqrt_comm_sz,MPI_UNSIGNED_CHAR,&column);
	MPI_Type_commit(&column);


	if (my_rank == 0) {
		/* Fill in array from file*/
        FILE *inputFile = fopen("waterfall_grey_1920_2520.raw", "r");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
				array2D[i][j] = fgetc(inputFile);
			}
		}
	}

	MPI_Scatterv(*array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 *myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR, /* I'm recieving (NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz) MPI_UNSIGNED_CHARs into myArray */
				 0,MPI_COMM_WORLD );

	/* Send and recieve rows,columns and corners */
	if( my_rank < sqrt_comm_sz && my_rank%sqrt_comm_sz == 0 ){ //top left corner

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		topRow = NULL ;
		leftCol = NULL ;
		leftUpCorn = -1;
		rightUpCorn = -1;
		leftDownCorn = -1;

	}
	else if( my_rank < sqrt_comm_sz && my_rank%sqrt_comm_sz == sqrt_comm_sz-1 ){ //top right corner

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		topRow = NULL ;
		rightCol = NULL ;
		leftUpCorn = -1;
		rightUpCorn = -1;
		rightDownCorn = -1;

	}
	else if( my_rank >= comm_sz-sqrt_comm_sz && my_rank%sqrt_comm_sz == 0 ){ //bottom left corner

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		bottomRow = NULL ;
		leftCol = NULL ;
		leftUpCorn = -1;
		rightDownCorn = -1;
		leftDownCorn = -1;

	}
	else if( my_rank >= comm_sz-sqrt_comm_sz && my_rank%sqrt_comm_sz == sqrt_comm_sz-1 ){ //bottom right corner

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Recieve leftUpCorn from my left process */
		MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		bottomRow = NULL ;
		rightCol = NULL ;
		rightUpCorn = -1;
		rightDownCorn = -1;
		leftDownCorn = -1;

	}
	else if( my_rank < sqrt_comm_sz ){ //top row

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		topRow = NULL ;
		leftUpCorn = -1;
		rightUpCorn = -1;

	}
	else if( my_rank >= comm_sz - sqrt_comm_sz ){ //bottom row

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftUpCorn from my left process */
		MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		bottomRow = NULL ;
		rightDownCorn = -1;
		leftDownCorn = -1;

	}
	else if( my_rank%sqrt_comm_sz == 0 ){ //leftmost column

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		leftCol = NULL ;
		leftUpCorn = -1;
		leftDownCorn = -1;

	}
	else if( my_rank%sqrt_comm_sz == sqrt_comm_sz-1 ){ //rightmost column

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve leftUpCorn from my left process */
		MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* What i didn't recieve */
		rightCol = NULL ;
		rightUpCorn = -1;
		rightDownCorn = -1;

	}
	else{ //everything in between

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve leftUpCorn from my left process */
		MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

	}

    for(int i=0; i<10; i++)
        myArray = applyFilter(myArray, (NROWS/sqrt_comm_sz), (NCOLS/sqrt_comm_sz));

	MPI_Gatherv(*myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,*final2D,counts,displs,resizedtype,0,MPI_COMM_WORLD );

	if( my_rank == 0 ){

        FILE *outputFile = fopen("mpiOutput.raw", "w");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
                //printf("%c", final2D[i][j]);
                fputc(final2D[i][j], outputFile);
			}
		}
	}


	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
