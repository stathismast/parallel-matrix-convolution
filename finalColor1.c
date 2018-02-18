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
//those that are on the edges
void applyFilter(unsigned char **original,unsigned char **final, int rows, int cols){

	//Apply filter
	for(int i=1; i<rows-1; i++){
		for(int j=1; j<cols-1; j++){
			final[i][j] = original[i-1][j-1]*((double)1/16)
						   + original[i][j-1]*((double)2/16)
						   + original[i+1][j-1]*((double)1/16)
						   + original[i-1][j]*((double)2/16)
						   + original[i][j]*((double)4/16)
						   + original[i+1][j]*((double)2/16)
						   + original[i-1][j+1]*((double)1/16)
						   + original[i][j+1]*((double)2/16)
						   + original[i+1][j+1]*((double)1/16);
		}
	}

	//I should be freeing malloc'd memory here

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

	unsigned char **arrayR;
	unsigned char **arrayG;
	unsigned char **arrayB;

	unsigned char **finalR;
	unsigned char **finalG;
	unsigned char **finalB;

	unsigned char **myArray; /* local array of each process */
	unsigned char **myFinalArray;
	unsigned char **temp;

	unsigned char *topRow;
	unsigned char *bottomRow;

	unsigned char *leftCol;
	unsigned char *rightCol;

	unsigned char leftUpCorn,rightUpCorn,rightDownCorn,leftDownCorn;

	typedef struct recievedItems{ /* 0 -> not been recieved yet, 1 -> recieved , 2 -> mustn't be recieved  */
		char topRow;
		char bottomRow;
		char leftCol;
		char rightCol;
		char leftUpCorn;
		char rightUpCorn;
		char rightDownCorn;
		char leftDownCorn;
	} recievedItems;

	recievedItems myRecievedItems;

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

	arrayR = get2DArray(NROWS,NCOLS);
	arrayG = get2DArray(NROWS,NCOLS);
	arrayB = get2DArray(NROWS,NCOLS);
	finalR = get2DArray(NROWS,NCOLS);
	finalG = get2DArray(NROWS,NCOLS);
	finalB = get2DArray(NROWS,NCOLS);

	myArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);
	myFinalArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);

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
		FILE *inputFile = fopen("waterfall_1920_2520.raw", "r");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
				arrayR[i][j] = fgetc(inputFile);
				arrayG[i][j] = fgetc(inputFile);
				arrayB[i][j] = fgetc(inputFile);
			}
		}

	}


	for(int k=0; k<3; k++){

		if(k==0) {array2D = arrayR; final2D = finalR;}
		if(k==1) {array2D = arrayG; final2D = finalG;}
		if(k==2) {array2D = arrayB; final2D = finalB;}

		MPI_Scatterv(*array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
					 resizedtype,
					 *myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR, /* I'm recieving (NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz) MPI_UNSIGNED_CHARs into myArray */
					 0,MPI_COMM_WORLD );

		for(i=0; i<20; i++){

			myRecievedItems.topRow = 0;
			myRecievedItems.bottomRow = 0;
			myRecievedItems.leftCol = 0;
			myRecievedItems.rightCol = 0;
			myRecievedItems.leftUpCorn = 0;
			myRecievedItems.rightUpCorn = 0;
			myRecievedItems.rightDownCorn = 0;
			myRecievedItems.leftDownCorn = 0;

			/* Send and recieve rows,columns and corners */
			if( (my_rank < sqrt_comm_sz) && ( (my_rank%sqrt_comm_sz) == 0 ) ){ //top left corner

				/* Send my rightCol to my right process */
				MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

				/* Send my bottomRow to my bottom process */
				MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

				/* Send my rightDownCorn to my rightdown process */
				MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

				/* Recieve rightCol from my right process */
				MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightCol = 1;

				/* Recieve bottomRow from my bottom process */
				MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.bottomRow = 1;

				/* Recieve rightDownCorn from my rightdown process */
				MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightDownCorn = 1;

				/* What i didn't recieve */
				myRecievedItems.topRow = 2;
				myRecievedItems.leftCol = 2;
				myRecievedItems.leftUpCorn = 2;
				myRecievedItems.rightUpCorn = 2;
				myRecievedItems.leftDownCorn = 2;

			}
			else if( (my_rank < sqrt_comm_sz) && ( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ) ){ //top right corner

				/* Send my leftCol to my left process */
				MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

				/* Send my leftDownCorn to my leftdown process */
				MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

				/* Send my bottomRow to my bottom process */
				MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

				/* Recieve leftCol from my left process */
				MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftCol = 1;

				/* Recieve leftDownCorn from my leftdown process */
				MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftDownCorn = 1;

				/* Recieve bottomRow from my bottom process */
				MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.bottomRow = 1;

				/* What i didn't recieve */
				myRecievedItems.topRow = 2;
				myRecievedItems.rightCol = 2;
				myRecievedItems.leftUpCorn = 2;
				myRecievedItems.rightUpCorn = 2;
				myRecievedItems.rightDownCorn = 2;

			}
			else if( ( my_rank >= (comm_sz-sqrt_comm_sz) ) && ( (my_rank%sqrt_comm_sz) == 0 ) ){ //bottom left corner

				/* Send my topRow to my top process */
				MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

				/* Send my rightUpCorn to my rightup process */
				MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

				/* Send my rightCol to my right process */
				MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

				/* Recieve topRow from my top process */
				MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.topRow = 1;

				/* Recieve rightUpCorn from my rightup rocess */
				MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightUpCorn = 1;

				/* Recieve rightCol from my right process */
				MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightCol = 1;

				/* What i didn't recieve */
				myRecievedItems.bottomRow = 2;
				myRecievedItems.leftCol = 2;
				myRecievedItems.leftUpCorn = 2;
				myRecievedItems.rightDownCorn = 2;
				myRecievedItems.leftDownCorn = 2;

			}
			else if( ( my_rank >= (comm_sz-sqrt_comm_sz) ) && ( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ) ){ //bottom right corner

				/* Send my leftUpCorn to my leftup process */
				MPI_Isend(&myArray[0][0],1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

				/* Send my topRow to my top process */
				MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

				/* Send my leftCol to my left process */
				MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

				/* Recieve leftUpCorn from my left process */
				MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftUpCorn = 1;

				/* Recieve topRow from my top process */
				MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.topRow = 1;

				/* Recieve leftCol from my left process */
				MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftCol = 1;

				/* What i didn't recieve */
				myRecievedItems.bottomRow = 2;
				myRecievedItems.rightCol = 2;
				myRecievedItems.rightUpCorn = 2;
				myRecievedItems.rightDownCorn = 2;
				myRecievedItems.leftDownCorn = 2;

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
				myRecievedItems.rightCol = 1;

				/* Recieve bottomRow from my bottom process */
				MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.bottomRow = 1;

				/* Recieve rightDownCorn from my rightdown process */
				MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightDownCorn = 1;

				/* Recieve leftCol from my left process */
				MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftCol = 1;

				/* Recieve leftDownCorn from my leftdown process */
				MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftDownCorn = 1;

				/* What i didn't recieve */
				myRecievedItems.topRow = 2;
				myRecievedItems.leftUpCorn = 2;
				myRecievedItems.rightUpCorn = 2;

			}
			else if( my_rank >= (comm_sz - sqrt_comm_sz) ){ //bottom row

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
				myRecievedItems.topRow = 1;

				/* Recieve rightUpCorn from my rightup rocess */
				MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightUpCorn = 1;

				/* Recieve rightCol from my right process */
				MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightCol = 1;

				/* Recieve leftUpCorn from my left process */
				MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftUpCorn = 1;

				/* Recieve leftCol from my left process */
				MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftCol = 1;

				/* What i didn't recieve */
				myRecievedItems.bottomRow = 2;
				myRecievedItems.rightDownCorn = 2;
				myRecievedItems.leftDownCorn = 2;

			}
			else if( (my_rank%sqrt_comm_sz) == 0 ){ //leftmost column

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
				myRecievedItems.topRow = 1;

				/* Recieve rightUpCorn from my rightup rocess */
				MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightUpCorn = 1;

				/* Recieve rightCol from my right process */
				MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightCol = 1;

				/* Recieve bottomRow from my bottom process */
				MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.bottomRow = 1;

				/* Recieve rightDownCorn from my rightdown process */
				MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightDownCorn = 1;

				/* What i didn't recieve */
				myRecievedItems.leftCol = 2;
				myRecievedItems.leftUpCorn = 2;
				myRecievedItems.leftDownCorn = 2;

			}
			else if( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ){ //rightmost column

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
				myRecievedItems.topRow = 1;

				/* Recieve leftUpCorn from my left process */
				MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftUpCorn = 1;

				/* Recieve leftCol from my left process */
				MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftCol = 1;

				/* Recieve bottomRow from my bottom process */
				MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.bottomRow = 1;

				/* Recieve leftDownCorn from my leftdown process */
				MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftDownCorn = 1;

				/* What i didn't recieve */
				myRecievedItems.rightCol = 2;
				myRecievedItems.rightUpCorn = 2;
				myRecievedItems.rightDownCorn = 2;

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
				myRecievedItems.topRow = 1;

				/* Recieve leftUpCorn from my left process */
				MPI_Recv(&leftUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftUpCorn = 1;

				/* Recieve leftCol from my left process */
				MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftCol = 1;

				/* Recieve bottomRow from my bottom process */
				MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);
				myRecievedItems.bottomRow = 1;

				/* Recieve leftDownCorn from my leftdown process */
				MPI_Recv(&leftDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightDownCorn = 1;

				/* Recieve rightUpCorn from my rightup rocess */
				MPI_Recv(&rightUpCorn,1,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightUpCorn = 1;

				/* Recieve rightCol from my right process */
				MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.rightCol = 1;

				/* Recieve rightDownCorn from my rightdown process */
				MPI_Recv(&rightDownCorn,1,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);
				myRecievedItems.leftDownCorn = 1;

			}

			/* Apply filter on the inner pixels */
			applyFilter(myArray,myFinalArray,(NROWS/sqrt_comm_sz), (NCOLS/sqrt_comm_sz));

			/* Apply filter on the inner topRow */
			if( myRecievedItems.topRow == 2 ){
				// i=0
				for( j=3; j<(NCOLS/sqrt_comm_sz)-3; j++ ){
					myFinalArray[0][j] = myArray[0][j]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][j-3]*((double)2/16)		/* left */
									   + myArray[0+1][j-3]*((double)1/16)	/* leftDownCorn */
									   + myArray[0][j]*((double)2/16)		/* up */		/* <<< WE USE ITSELF */
									   + myArray[0][j]*((double)4/16)		/* itself */
									   + myArray[0+1][j]*((double)2/16)		/* down */
									   + myArray[0][j]*((double)1/16)		/* rightUpCorn */ /* <<< WE USE ITSELF */
									   + myArray[0][j+3]*((double)2/16)		/* right */
									   + myArray[0+1][j+3]*((double)1/16);	/* rightDownCorn*/
				}
			}
			else if( myRecievedItems.topRow == 1 ){
				// i=0
				for( j=3; j<(NCOLS/sqrt_comm_sz)-3; j++ ){
					myFinalArray[0][j] = topRow[j-3]*((double)1/16) 		/* leftUpCorn */
									   + myArray[0][j-3]*((double)2/16)		/* left */
									   + myArray[0+1][j-3]*((double)1/16)	/* leftDownCorn */
									   + topRow[j]*((double)2/16)			/* up */
									   + myArray[0][j]*((double)4/16)		/* itself */
									   + myArray[0+1][j]*((double)2/16)		/* down */
									   + topRow[j+3]*((double)1/16)			/* rightUpCorn */
									   + myArray[0][j+3]*((double)2/16)		/* right */
									   + myArray[0+1][j+3]*((double)1/16);	/* rightDownCorn*/
				}
			}

			/* Apply filter on the inner bottomRow */
			if( myRecievedItems.bottomRow == 2 ){
				int s = (NROWS/sqrt_comm_sz)-1;
				for( j=3; j<(NCOLS/sqrt_comm_sz)-3; j++ ){
					myFinalArray[s][j] = myArray[s-1][j-3]*((double)1/16) 	/* leftUpCorn */
									   + myArray[s][j-3]*((double)2/16)		/* left */
									   + myArray[s][j]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[s-1][j]*((double)2/16)		/* up */
									   + myArray[s][j]*((double)4/16)		/* itself */
									   + myArray[s][j]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
									   + myArray[s-1][j+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[s][j+3]*((double)2/16)		/* right */
									   + myArray[s][j]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}
			}
			else if( myRecievedItems.bottomRow == 1 ){
				int s = (NROWS/sqrt_comm_sz)-1;
				for( j=3; j<(NCOLS/sqrt_comm_sz)-3; j++ ){
					myFinalArray[s][j] = myArray[s-1][j-3]*((double)1/16) 	/* leftUpCorn */
									   + myArray[s][j-3]*((double)2/16)		/* left */
									   + bottomRow[j-3]*((double)1/16)		/* leftDownCorn */
									   + myArray[s-1][j]*((double)2/16)		/* up */
									   + myArray[s][j]*((double)4/16)		/* itself */
									   + bottomRow[j]*((double)2/16)		/* down */
									   + myArray[s-1][j+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[s][j+3]*((double)2/16)		/* right */
									   + bottomRow[j+3]*((double)1/16);		/* rightDownCorn*/
				}
			}

			/* Apply filter on the inner leftCol */
			if( myRecievedItems.leftCol == 2 ){
				//j=0
				for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
					myFinalArray[j][0] = myArray[j][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[j][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
									   + myArray[j][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[j-1][0]*((double)2/16)		/* up */
									   + myArray[j][0]*((double)4/16)		/* itself */
									   + myArray[j+1][0]*((double)2/16)		/* down */
									   + myArray[j-1][0+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[j][0+3]*((double)2/16)		/* right */
									   + myArray[j+1][0+3]*((double)1/16);	/* rightDownCorn*/

				   myFinalArray[j][0] = myArray[j][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
		   							   + myArray[j][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
		   							   + myArray[j][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
		   							   + myArray[j-1][0]*((double)2/16)		/* up */
		   							   + myArray[j][0]*((double)4/16)		/* itself */
		   							   + myArray[j+1][0]*((double)2/16)		/* down */
		   							   + myArray[j-1][0+3]*((double)1/16)	/* rightUpCorn */
		   							   + myArray[j][0+3]*((double)2/16)		/* right */
		   							   + myArray[j+1][0+3]*((double)1/16);	/* rightDownCorn*/

				   myFinalArray[j][0] = myArray[j][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[j][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
									   + myArray[j][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[j-1][0]*((double)2/16)		/* up */
									   + myArray[j][0]*((double)4/16)		/* itself */
									   + myArray[j+1][0]*((double)2/16)		/* down */
									   + myArray[j-1][0+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[j][0+3]*((double)2/16)		/* right */
									   + myArray[j+1][0+3]*((double)1/16);	/* rightDownCorn*/
				}
			}
			else if( myRecievedItems.leftCol == 1 ){
				//j=0
				for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
					myFinalArray[j][0] = leftCol[j-1]*((double)1/16) 		/* leftUpCorn */
									   + leftCol[j]*((double)2/16)			/* left */
									   + leftCol[j+1]*((double)1/16)		/* leftDownCorn */
									   + myArray[j-1][0]*((double)2/16)		/* up */
									   + myArray[j][0]*((double)4/16)		/* itself */
									   + myArray[j+1][0]*((double)2/16)		/* down */
									   + myArray[j-1][0+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[j][0+3]*((double)2/16)		/* right */
									   + myArray[j+1][0+3]*((double)1/16);	/* rightDownCorn*/

					myFinalArray[j][0] = leftCol[j-1]*((double)1/16) 		/* leftUpCorn */
									   + leftCol[j]*((double)2/16)			/* left */
									   + leftCol[j+1]*((double)1/16)		/* leftDownCorn */
									   + myArray[j-1][0]*((double)2/16)		/* up */
									   + myArray[j][0]*((double)4/16)		/* itself */
									   + myArray[j+1][0]*((double)2/16)		/* down */
									   + myArray[j-1][0+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[j][0+3]*((double)2/16)		/* right */
									   + myArray[j+1][0+3]*((double)1/16);	/* rightDownCorn*/

	   				myFinalArray[j][0] = leftCol[j-1]*((double)1/16) 		/* leftUpCorn */
	   								   + leftCol[j]*((double)2/16)			/* left */
	   								   + leftCol[j+1]*((double)1/16)		/* leftDownCorn */
	   								   + myArray[j-1][0]*((double)2/16)		/* up */
	   								   + myArray[j][0]*((double)4/16)		/* itself */
	   								   + myArray[j+1][0]*((double)2/16)		/* down */
	   								   + myArray[j-1][0+3]*((double)1/16)	/* rightUpCorn */
	   								   + myArray[j][0+3]*((double)2/16)		/* right */
	   								   + myArray[j+1][0+3]*((double)1/16);	/* rightDownCorn*/
				}
			}

			/* Apply filter on the inner rightCol */
			if( myRecievedItems.rightCol == 2 ){
				int s = (NCOLS/sqrt_comm_sz)-1;
				for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
					myFinalArray[j][s] = myArray[j-1][s-3]*((double)1/16) 	/* leftUpCorn */
									   + myArray[j][s-3]*((double)2/16)		/* left */
									   + myArray[j+1][s-3]*((double)1/16)	/* leftDownCorn */
									   + myArray[j-1][s]*((double)2/16)		/* up */
									   + myArray[j][s]*((double)4/16)		/* itself */
									   + myArray[j+1][s]*((double)2/16)		/* down */
									   + myArray[j][s]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[j][s]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[j][s]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

					myFinalArray[j][s] = myArray[j-1][s-3]*((double)1/16) 	/* leftUpCorn */
									   + myArray[j][s-3]*((double)2/16)		/* left */
									   + myArray[j+1][s-3]*((double)1/16)	/* leftDownCorn */
									   + myArray[j-1][s]*((double)2/16)		/* up */
									   + myArray[j][s]*((double)4/16)		/* itself */
									   + myArray[j+1][s]*((double)2/16)		/* down */
									   + myArray[j][s]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[j][s]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[j][s]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

					myFinalArray[j][s] = myArray[j-1][s-3]*((double)1/16) 	/* leftUpCorn */
									   + myArray[j][s-3]*((double)2/16)		/* left */
									   + myArray[j+1][s-3]*((double)1/16)	/* leftDownCorn */
									   + myArray[j-1][s]*((double)2/16)		/* up */
									   + myArray[j][s]*((double)4/16)		/* itself */
									   + myArray[j+1][s]*((double)2/16)		/* down */
									   + myArray[j][s]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[j][s]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[j][s]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}
			}
			else if( myRecievedItems.rightCol == 1 ){
				int s = (NCOLS/sqrt_comm_sz)-1;
				for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
					myFinalArray[j][s] = myArray[j-1][s-3]*((double)1/16) 	/* leftUpCorn */
									   + myArray[j][s-3]*((double)2/16)		/* left */
									   + myArray[j+1][s-3]*((double)1/16)	/* leftDownCorn */
									   + myArray[j-1][s]*((double)2/16)		/* up */
									   + myArray[j][s]*((double)4/16)		/* itself */
									   + myArray[j+1][s]*((double)2/16)		/* down */
									   + rightCol[j-1]*((double)1/16)		/* rightUpCorn */
									   + rightCol[j]*((double)2/16)			/* right */
									   + rightCol[j+1]*((double)1/16);		/* rightDownCorn*/

				   	myFinalArray[j][s] = myArray[j-1][s-3]*((double)1/16) 	/* leftUpCorn */
				   					   + myArray[j][s-3]*((double)2/16)		/* left */
				   					   + myArray[j+1][s-3]*((double)1/16)	/* leftDownCorn */
				   					   + myArray[j-1][s]*((double)2/16)		/* up */
				   					   + myArray[j][s]*((double)4/16)		/* itself */
				   					   + myArray[j+1][s]*((double)2/16)		/* down */
				   					   + rightCol[j-1]*((double)1/16)		/* rightUpCorn */
				   					   + rightCol[j]*((double)2/16)			/* right */
				   					   + rightCol[j+1]*((double)1/16);		/* rightDownCorn*/

				   	myFinalArray[j][s] = myArray[j-1][s-3]*((double)1/16) 	/* leftUpCorn */
				   					   + myArray[j][s-3]*((double)2/16)		/* left */
				   					   + myArray[j+1][s-3]*((double)1/16)	/* leftDownCorn */
				   					   + myArray[j-1][s]*((double)2/16)		/* up */
				   					   + myArray[j][s]*((double)4/16)		/* itself */
				   					   + myArray[j+1][s]*((double)2/16)		/* down */
				   					   + rightCol[j-1]*((double)1/16)		/* rightUpCorn */
				   					   + rightCol[j]*((double)2/16)			/* right */
				   					   + rightCol[j+1]*((double)1/16);		/* rightDownCorn*/
				}
			}

			/* Apply filter on the leftUpCorn */
			if( (myRecievedItems.topRow == 2) && (myRecievedItems.leftCol == 2) && (myRecievedItems.leftUpCorn == 2) ){
				//i=0 and j=0
				myFinalArray[0][0] = myArray[0][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)4/16)		/* itself */
								   + myArray[0+1][0]*((double)2/16)		/* down */
								   + myArray[0][0]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][0+1]*((double)2/16)		/* right */
								   + myArray[0+1][0+1]*((double)1/16);	/* rightDownCorn*/
			}
			else if( (myRecievedItems.topRow == 2) && (myRecievedItems.leftCol == 1) && (myRecievedItems.leftUpCorn == 2) ){
				//i=0 and j=0
				myFinalArray[0][0] = myArray[0][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + leftCol[0]*((double)2/16)			/* left */
								   + leftCol[1]*((double)1/16)			/* leftDownCorn */
								   + myArray[0][0]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)4/16)		/* itself */
								   + myArray[0+1][0]*((double)2/16)		/* down */
								   + myArray[0][0]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][0+1]*((double)2/16)		/* right */
								   + myArray[0+1][0+1]*((double)1/16);	/* rightDownCorn*/
			}
			else if( (myRecievedItems.topRow == 1) && (myRecievedItems.leftCol == 2) && (myRecievedItems.leftUpCorn == 2) ){
				//i=0 and j=0
				myFinalArray[0][0] = myArray[0][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
								   + myArray[0][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + topRow[0]*((double)2/16)			/* up */
								   + myArray[0][0]*((double)4/16)		/* itself */
								   + myArray[0+1][0]*((double)2/16)		/* down */
								   + topRow[1]*((double)1/16)			/* rightUpCorn */
								   + myArray[0][0+1]*((double)2/16)		/* right */
								   + myArray[0+1][0+1]*((double)1/16);	/* rightDownCorn*/
			}
			else if( (myRecievedItems.topRow == 1) && (myRecievedItems.leftCol == 1) && (myRecievedItems.leftUpCorn == 1) ){
				//i=0 and j=0
				myFinalArray[0][0] = leftUpCorn*((double)1/16) 			/* leftUpCorn */
								   + leftCol[0]*((double)2/16)			/* left */
								   + leftCol[1]*((double)1/16)			/* leftDownCorn */
								   + topRow[0]*((double)2/16)			/* up */
								   + myArray[0][0]*((double)4/16)		/* itself */
								   + myArray[0+1][0]*((double)2/16)		/* down */
								   + topRow[1]*((double)1/16)			/* rightUpCorn */
								   + myArray[0][0+1]*((double)2/16)		/* right */
								   + myArray[0+1][0+1]*((double)1/16);	/* rightDownCorn*/
			}

			/* Apply filter on the rightUpCorn */
			if( (myRecievedItems.topRow == 2) && (myRecievedItems.rightCol == 2) && (myRecievedItems.rightUpCorn == 2) ){
				//i=0
				int r = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[0][r] = myArray[0][r]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][r-1]*((double)2/16)		/* left */
								   + myArray[0+1][r-1]*((double)1/16)	/* leftDownCorn */
								   + myArray[0][r]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
								   + myArray[0][r]*((double)4/16)		/* itself */
								   + myArray[0+1][r]*((double)2/16)		/* down */
								   + myArray[0][r]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][r]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
								   + myArray[0][r]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
			}
			else if( (myRecievedItems.topRow == 2) && (myRecievedItems.rightCol == 1) && (myRecievedItems.rightUpCorn == 2) ){
				//i=0
				int r = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[0][r] = myArray[0][r]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][r-1]*((double)2/16)		/* left */
								   + myArray[0+1][r-1]*((double)1/16)	/* leftDownCorn */
								   + myArray[0][r]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
								   + myArray[0][r]*((double)4/16)		/* itself */
								   + myArray[0+1][r]*((double)2/16)		/* down */
								   + myArray[0][r]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + rightCol[0]*((double)2/16)			/* right */
								   + rightCol[1]*((double)1/16);		/* rightDownCorn*/

			}
			else if( (myRecievedItems.topRow == 1) && (myRecievedItems.rightCol == 2) && (myRecievedItems.rightUpCorn == 2) ){
				//i=0
				int r = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[0][r] = topRow[r-1]*((double)1/16) 		/* leftUpCorn */
								   + myArray[0][r-1]*((double)2/16)		/* left */
								   + myArray[0+1][r-1]*((double)1/16)	/* leftDownCorn */
								   + topRow[r]*((double)2/16)			/* up */
								   + myArray[0][r]*((double)4/16)		/* itself */
								   + myArray[0+1][r]*((double)2/16)		/* down */
								   + myArray[0][r]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[0][r]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
								   + myArray[0][r]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

			}
			else if( (myRecievedItems.topRow == 1) && (myRecievedItems.rightCol == 1) && (myRecievedItems.rightUpCorn == 1) ){
				//i=0
				int r = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[0][r] = topRow[r-1]*((double)1/16) 		/* leftUpCorn */
								   + myArray[0][r-1]*((double)2/16)		/* left */
								   + myArray[0+1][r-1]*((double)1/16)	/* leftDownCorn */
								   + topRow[r]*((double)2/16)			/* up */
								   + myArray[0][r]*((double)4/16)		/* itself */
								   + myArray[0+1][r]*((double)2/16)		/* down */
								   + rightUpCorn*((double)1/16)			/* rightUpCorn */
								   + rightCol[0]*((double)2/16)			/* right */
								   + rightCol[1]*((double)1/16);		/* rightDownCorn*/

			}

			/* Apply filter on the leftDownCorn */
			if( (myRecievedItems.bottomRow == 2) && (myRecievedItems.leftCol == 2) && (myRecievedItems.leftDownCorn == 2) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;

				myFinalArray[r][0] = myArray[r][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[r][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
								   + myArray[r][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + myArray[r-1][0]*((double)2/16)		/* up */
								   + myArray[r][0]*((double)4/16)		/* itself */
								   + myArray[r][0]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
								   + myArray[r-1][0+1]*((double)1/16)	/* rightUpCorn */
								   + myArray[r][0+1]*((double)2/16)		/* right */
								   + myArray[r][0]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

			}
			else if( (myRecievedItems.bottomRow == 2) && (myRecievedItems.leftCol == 1) && (myRecievedItems.leftDownCorn == 2) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;

				myFinalArray[r][0] = leftCol[r-1]*((double)1/16) 		/* leftUpCorn */
								   + leftCol[r]*((double)2/16)			/* left */
								   + myArray[r][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + myArray[r-1][0]*((double)2/16)		/* up */
								   + myArray[r][0]*((double)4/16)		/* itself */
								   + myArray[r][0]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
								   + myArray[r-1][0+1]*((double)1/16)	/* rightUpCorn */
								   + myArray[r][0+1]*((double)2/16)		/* right */
								   + myArray[r][0]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

			}
			else if( (myRecievedItems.bottomRow == 1) && (myRecievedItems.leftCol == 2) && (myRecievedItems.leftDownCorn == 2) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;

				myFinalArray[r][0] = myArray[r][0]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[r][0]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
								   + myArray[r][0]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + myArray[r-1][0]*((double)2/16)		/* up */
								   + myArray[r][0]*((double)4/16)		/* itself */
								   + bottomRow[0]*((double)2/16)		/* down */
								   + myArray[r-1][0+1]*((double)1/16)	/* rightUpCorn */
								   + myArray[r][0+1]*((double)2/16)		/* right */
								   + bottomRow[1]*((double)1/16);		/* rightDownCorn*/

			}
			else if( (myRecievedItems.bottomRow == 1) && (myRecievedItems.leftCol == 1) && (myRecievedItems.leftDownCorn == 1) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;

				myFinalArray[r][0] = leftCol[r-1]*((double)1/16) 		/* leftUpCorn */
								   + leftCol[r]*((double)2/16)			/* left */
								   + leftDownCorn*((double)1/16)		/* leftDownCorn */
								   + myArray[r-1][0]*((double)2/16)		/* up */
								   + myArray[r][0]*((double)4/16)		/* itself */
								   + bottomRow[0]*((double)2/16)		/* down */
								   + myArray[r-1][0+1]*((double)1/16)	/* rightUpCorn */
								   + myArray[r][0+1]*((double)2/16)		/* right */
								   + bottomRow[1]*((double)1/16);		/* rightDownCorn*/

			}

			/* Apply filter on the rightDownCorn */
			if( (myRecievedItems.bottomRow == 2) && (myRecievedItems.rightCol == 2) && (myRecievedItems.rightDownCorn == 2) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[r][s] = myArray[r-1][s-1]*((double)1/16) 	/* leftUpCorn */
								   + myArray[r][s-1]*((double)2/16)		/* left */
								   + myArray[r][s]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + myArray[r-1][s]*((double)2/16)		/* up */
								   + myArray[r][s]*((double)4/16)		/* itself */
								   + myArray[r][s]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
								   + myArray[r][s]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[r][s]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
								   + myArray[r][s]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

			}
			else if( (myRecievedItems.bottomRow == 2) && (myRecievedItems.rightCol == 1) && (myRecievedItems.rightDownCorn == 2) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[r][s] = myArray[r-1][s-1]*((double)1/16) 	/* leftUpCorn */
								   + myArray[r][s-1]*((double)2/16)		/* left */
								   + myArray[r][s]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
								   + myArray[r-1][s]*((double)2/16)		/* up */
								   + myArray[r][s]*((double)4/16)		/* itself */
								   + myArray[r][s]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
								   + rightCol[r-1]*((double)1/16)		/* rightUpCorn */
								   + rightCol[r]*((double)2/16)			/* right */
								   + myArray[r][s]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

			}
			else if( (myRecievedItems.bottomRow == 1) && (myRecievedItems.rightCol == 2) && (myRecievedItems.rightDownCorn == 2) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[r][s] = myArray[r-1][s-1]*((double)1/16) 	/* leftUpCorn */
								   + myArray[r][s-1]*((double)2/16)		/* left */
								   + bottomRow[s-1]*((double)1/16)		/* leftDownCorn */
								   + myArray[r-1][s]*((double)2/16)		/* up */
								   + myArray[r][s]*((double)4/16)		/* itself */
								   + bottomRow[s]*((double)2/16)		/* down */
								   + myArray[r][s]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
								   + myArray[r][s]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
								   + myArray[r][s]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */

			}
			else if( (myRecievedItems.bottomRow == 1) && (myRecievedItems.rightCol == 1) && (myRecievedItems.rightDownCorn == 1) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = NCOLS/sqrt_comm_sz - 1 ;

				myFinalArray[r][s] = myArray[r-1][s-1]*((double)1/16) 	/* leftUpCorn */
								   + myArray[r][s-1]*((double)2/16)		/* left */
								   + bottomRow[s-1]*((double)1/16)		/* leftDownCorn */
								   + myArray[r-1][s]*((double)2/16)		/* up */
								   + myArray[r][s]*((double)4/16)		/* itself */
								   + bottomRow[s]*((double)2/16)		/* down */
								   + rightCol[r-1]*((double)1/16)		/* rightUpCorn */
								   + rightCol[r]*((double)2/16)			/* right */
								   + rightDownCorn*((double)1/16);		/* rightDownCorn*/

			}

			/* Swap arrays */
			temp = myArray;
			myArray = myFinalArray;
			myFinalArray = temp;
		}

		MPI_Gatherv(*myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,*array2D,counts,displs,resizedtype,0,MPI_COMM_WORLD );
	}

	if( my_rank == 0 ){

		FILE *outputFile = fopen("mpiOutput.raw", "w");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
				//printf("%c", final2D[i][j]);
				fputc(arrayR[i][j], outputFile);
				fputc(arrayG[i][j], outputFile);
				fputc(arrayB[i][j], outputFile);
			}
		}
	}


	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
