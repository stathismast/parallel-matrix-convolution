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
	unsigned char * space = malloc(rows * cols * ( 3 *  sizeof(unsigned char) ) );
	unsigned char ** array = malloc(rows * sizeof(unsigned char*));

	for(int i=0; i<rows; i++){
		array[i] = space + i * ( 3 * cols);
	}

	return array;
}

//Applies a filter to every pixel except
//those that are on the edges
void applyFilter(unsigned char **original,unsigned char **final, int rows, int cols){
	int c;
	//Apply filter
	for(int i=1; i<rows-1; i++){
		for(int j=3; j<3*(cols-1); j = j+3 ){
			for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
				final[i][j+c] = original[i-1][j-3+c]*((double)1/16)		/* leftUpCorn */
						   + original[i][j-3+c]*((double)2/16)			/* left */
						   + original[i+1][j-3+c]*((double)1/16)		/* leftDownCorn */
						   + original[i-1][j+c]*((double)2/16)			/* up */
						   + original[i][j+c]*((double)4/16)			/* itself */
						   + original[i+1][j+c]*((double)2/16)			/* down */
						   + original[i-1][j+3+c]*((double)1/16)		/* rightUpCorn */
						   + original[i][j+3+c]*((double)2/16)			/* right */
						   + original[i+1][j+3+c]*((double)1/16);		/* rightDownCorn*/
			}
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

	unsigned char **myArray; /* local array of each process */
	unsigned char **myFinalArray;
	unsigned char **temp;

	unsigned char *topRow;
	unsigned char *bottomRow;

	unsigned char *leftCol;
	unsigned char *rightCol;

	unsigned char *leftUpCorn;
	unsigned char *rightUpCorn;
	unsigned char *rightDownCorn;
	unsigned char *leftDownCorn;

	typedef struct Item{
		char recieved;	/* 0 -> not been recieved yet, 1 -> recieved , 2 -> mustn't be recieved  */
		MPI_Request request;
	} Item;

	typedef struct allItems{
		Item topRow;
		Item bottomRow;
		Item leftCol;
		Item rightCol;
		Item leftUpCorn;
		Item rightUpCorn;
		Item rightDownCorn;
		Item leftDownCorn;
	} allItems;

	allItems myItems;

	int counterItems = 0; /* How many items we have recieved */
	int flag = 0;
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

	topRow = malloc( (NCOLS/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );
	bottomRow = malloc( (NCOLS/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );

	leftCol = malloc( (NROWS/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );
	rightCol = malloc( (NROWS/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );

	leftUpCorn = malloc( 3 * sizeof(unsigned char) );
	rightUpCorn = malloc( 3 * sizeof(unsigned char) );
	rightDownCorn = malloc( 3 * sizeof(unsigned char) );
	leftDownCorn = malloc( 3 * sizeof(unsigned char) );

	array2D = get2DArray(NROWS,NCOLS);
	final2D = get2DArray(NROWS,NCOLS);

	myArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);
	myFinalArray = get2DArray(NROWS/sqrt_comm_sz,NCOLS/sqrt_comm_sz);

	/* Arguments of create_subarray */
	sizes[0] = NROWS;
	sizes[1] = 3 * NCOLS;
	subsizes[0] = NROWS/sqrt_comm_sz;
	subsizes[1] = 3 * (NCOLS/sqrt_comm_sz);

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
	MPI_Type_create_resized(type,0,(NCOLS/sqrt_comm_sz)*( 3 * sizeof(unsigned char) ),&resizedtype);
	MPI_Type_commit(&resizedtype);

	MPI_Type_contiguous(3*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,&row);
	MPI_Type_commit(&row);

	MPI_Type_vector(NROWS/sqrt_comm_sz,3,3*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,&column);
	MPI_Type_commit(&column);


	if (my_rank == 0) {

		/* Fill in array from file*/
		FILE *inputFile = fopen("waterfall_1920_2520.raw", "r");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<3*NCOLS; j = j+3 ){
				array2D[i][j] = fgetc(inputFile);
				array2D[i][j+1] = fgetc(inputFile);
				array2D[i][j+2] = fgetc(inputFile);
			}
		}

	}

	MPI_Scatterv(*array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 *myArray,(NROWS/sqrt_comm_sz)*(3*(NCOLS/sqrt_comm_sz)),MPI_UNSIGNED_CHAR, /* I'm recieving (NROWS/sqrt_comm_sz)*3*(NCOLS/sqrt_comm_sz) MPI_UNSIGNED_CHARs into myArray */
				 0,MPI_COMM_WORLD );

	for(i=0; i<50; i++){

		myItems.topRow.recieved = 0;
		myItems.bottomRow.recieved = 0;
		myItems.leftCol.recieved = 0;
		myItems.rightCol.recieved = 0;
		myItems.leftUpCorn.recieved = 0;
		myItems.rightUpCorn.recieved = 0;
		myItems.rightDownCorn.recieved = 0;
		myItems.leftDownCorn.recieved = 0;

		/* Send and recieve rows,columns and corners */
		if( (my_rank < sqrt_comm_sz) && ( (my_rank%sqrt_comm_sz) == 0 ) ){ //top left corner

			/* Send my rightCol to my right process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

			/* Send my bottomRow to my bottom process */
			MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my rightDownCorn to my rightdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Recieve rightCol from my right process */
			MPI_Irecv(rightCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &myItems.rightCol.request);

			/* Recieve bottomRow from my bottom process */
			MPI_Irecv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.bottomRow.request);

			/* Recieve rightDownCorn from my rightdown process */
			MPI_Irecv(rightDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightDownCorn.request);

			/* What i won't recieve */
			myItems.topRow.recieved = 2;
			myItems.leftCol.recieved = 2;
			myItems.leftUpCorn.recieved = 2;
			myItems.rightUpCorn.recieved = 2;
			myItems.leftDownCorn.recieved = 2;

		}
		else if( (my_rank < sqrt_comm_sz) && ( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ) ){ //top right corner

			/* Send my leftCol to my left process */
			MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

			/* Send my leftDownCorn to my leftdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Send my bottomRow to my bottom process */
			MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Recieve leftCol from my left process */
			MPI_Irecv(leftCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &myItems.leftCol.request);

			/* Recieve leftDownCorn from my leftdown process */
			MPI_Irecv(leftDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftDownCorn.request);

			/* Recieve bottomRow from my bottom process */
			MPI_Irecv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.bottomRow.request);

			/* What i won't recieve */
			myItems.topRow.recieved = 2;
			myItems.rightCol.recieved = 2;
			myItems.leftUpCorn.recieved = 2;
			myItems.rightUpCorn.recieved = 2;
			myItems.rightDownCorn.recieved = 2;

		}
		else if( ( my_rank >= (comm_sz-sqrt_comm_sz) ) && ( (my_rank%sqrt_comm_sz) == 0 ) ){ //bottom left corner

			/* Send my topRow to my top process */
			MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my rightUpCorn to my rightup process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Send my rightCol to my right process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

			/* Recieve topRow from my top process */
			MPI_Irecv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.topRow.request);

			/* Recieve rightUpCorn from my rightup rocess */
			MPI_Irecv(rightUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightUpCorn.request);

			/* Recieve rightCol from my right process */
			MPI_Irecv(rightCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &myItems.rightCol.request);

			/* What i won't recieve */
			myItems.bottomRow.recieved = 2;
			myItems.leftCol.recieved = 2;
			myItems.leftUpCorn.recieved = 2;
			myItems.rightDownCorn.recieved = 2;
			myItems.leftDownCorn.recieved = 2;

		}
		else if( ( my_rank >= (comm_sz-sqrt_comm_sz) ) && ( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ) ){ //bottom right corner

			/* Send my leftUpCorn to my leftup process */
			MPI_Isend(&myArray[0][0],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Send my topRow to my top process */
			MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my leftCol to my left process */
			MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

			/* Recieve leftUpCorn from my left process */
			MPI_Irecv(leftUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftUpCorn.request);

			/* Recieve topRow from my top process */
			MPI_Irecv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.topRow.request);

			/* Recieve leftCol from my left process */
			MPI_Irecv(leftCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &myItems.leftCol.request);

			/* What i won't recieve */
			myItems.bottomRow.recieved = 2;
			myItems.rightCol.recieved = 2;
			myItems.rightUpCorn.recieved = 2;
			myItems.rightDownCorn.recieved = 2;
			myItems.leftDownCorn.recieved = 2;

		}
		else if( my_rank < sqrt_comm_sz ){ //top row

			/* Send my rightCol to my right process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

			/* Send my bottomRow to my bottom process */
			MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my rightDownCorn to my rightdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Send my leftCol to my left process */
			MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

			/* Send my leftDownCorn to my leftdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Recieve rightCol from my right process */
			MPI_Irecv(rightCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &myItems.rightCol.request);

			/* Recieve bottomRow from my bottom process */
			MPI_Irecv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.bottomRow.request);

			/* Recieve rightDownCorn from my rightdown process */
			MPI_Irecv(rightDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightDownCorn.request);

			/* Recieve leftCol from my left process */
			MPI_Irecv(leftCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &myItems.leftCol.request);

			/* Recieve leftDownCorn from my leftdown process */
			MPI_Irecv(leftDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftDownCorn.request);

			/* What i won't recieve */
			myItems.topRow.recieved = 2;
			myItems.leftUpCorn.recieved = 2;
			myItems.rightUpCorn.recieved = 2;

		}
		else if( my_rank >= (comm_sz - sqrt_comm_sz) ){ //bottom row

			/* Send my topRow to my top process */
			MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my rightUpCorn to my rightup process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Send my rightCol to my right process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

			/* Send my leftUpCorn to my leftup process */
			MPI_Isend(&myArray[0][0],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Send my leftCol to my left process */
			MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

			/* Recieve topRow from my top process */
			MPI_Irecv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.topRow.request);

			/* Recieve rightUpCorn from my rightup rocess */
			MPI_Irecv(rightUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightUpCorn.request);

			/* Recieve rightCol from my right process */
			MPI_Irecv(rightCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &myItems.rightCol.request);

			/* Recieve leftUpCorn from my left process */
			MPI_Irecv(leftUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftUpCorn.request);

			/* Recieve leftCol from my left process */
			MPI_Irecv(leftCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &myItems.leftCol.request);

			/* What i won't recieve */
			myItems.bottomRow.recieved = 2;
			myItems.rightDownCorn.recieved = 2;
			myItems.leftDownCorn.recieved = 2;

		}
		else if( (my_rank%sqrt_comm_sz) == 0 ){ //leftmost column

			/* Send my topRow to my top process */
			MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my rightUpCorn to my rightup process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Send my rightCol to my right process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

			/* Send my bottomRow to my bottom process */
			MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my rightDownCorn to my rightdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Recieve topRow from my top process */
			MPI_Irecv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.topRow.request);

			/* Recieve rightUpCorn from my rightup rocess */
			MPI_Irecv(rightUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightUpCorn.request);

			/* Recieve rightCol from my right process */
			MPI_Irecv(rightCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &myItems.rightCol.request);

			/* Recieve bottomRow from my bottom process */
			MPI_Irecv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.bottomRow.request);

			/* Recieve rightDownCorn from my rightdown process */
			MPI_Irecv(rightDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightDownCorn.request);

			/* What i won't recieve */
			myItems.leftCol.recieved = 2;
			myItems.leftUpCorn.recieved = 2;
			myItems.leftDownCorn.recieved = 2;

		}
		else if( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ){ //rightmost column

			/* Send my topRow to my top process */
			MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my leftUpCorn to my leftup process */
			MPI_Isend(&myArray[0][0],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Send my leftCol to my left process */
			MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

			/* Send my bottomRow to my bottom process */
			MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my leftDownCorn to my leftdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Recieve topRow from my top process */
			MPI_Irecv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.topRow.request);

			/* Recieve leftUpCorn from my left process */
			MPI_Irecv(leftUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftUpCorn.request);

			/* Recieve leftCol from my left process */
			MPI_Irecv(leftCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &myItems.leftCol.request);

			/* Recieve bottomRow from my bottom process */
			MPI_Irecv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.bottomRow.request);

			/* Recieve leftDownCorn from my leftdown process */
			MPI_Irecv(leftDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftDownCorn.request);

			/* What i won't recieve */
			myItems.rightCol.recieved = 2;
			myItems.rightUpCorn.recieved = 2;
			myItems.rightDownCorn.recieved = 2;

		}
		else{ //everything in between

			/* Send my topRow to my top process */
			MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my leftUpCorn to my leftup process */
			MPI_Isend(&myArray[0][0],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Send my leftCol to my left process */
			MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

			/* Send my bottomRow to my bottom process */
			MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

			/* Send my leftDownCorn to my leftdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

			/* Send my rightUpCorn to my rightup process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Send my rightCol to my right process */
			MPI_Isend(&myArray[0][3*(NCOLS/sqrt_comm_sz)-3],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

			/* Send my rightDownCorn to my rightdown process */
			MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][3*(NCOLS/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

			/* Recieve topRow from my top process */
			MPI_Irecv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.topRow.request);

			/* Recieve leftUpCorn from my left process */
			MPI_Irecv(leftUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftUpCorn.request);

			/* Recieve leftCol from my left process */
			MPI_Irecv(leftCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank-1,0,MPI_COMM_WORLD, &myItems.leftCol.request);

			/* Recieve bottomRow from my bottom process */
			MPI_Irecv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &myItems.bottomRow.request);

			/* Recieve leftDownCorn from my leftdown process */
			MPI_Irecv(leftDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &myItems.leftDownCorn.request);

			/* Recieve rightUpCorn from my rightup rocess */
			MPI_Irecv(rightUpCorn,3,MPI_UNSIGNED_CHAR,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightUpCorn.request);

			/* Recieve rightCol from my right process */
			MPI_Irecv(rightCol,3*(NROWS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,my_rank+1,0,MPI_COMM_WORLD, &myItems.rightCol.request);

			/* Recieve rightDownCorn from my rightdown process */
			MPI_Irecv(rightDownCorn,3,MPI_UNSIGNED_CHAR,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &myItems.rightDownCorn.request);

		}

		/* Apply filter on the inner pixels */
		applyFilter(myArray,myFinalArray,(NROWS/sqrt_comm_sz), (NCOLS/sqrt_comm_sz));

		counterItems = 0;

		/* Check what we have recieved and apply filter to these items */
		while( counterItems < 8 ){

			/* Apply filter on the inner topRow */
			if( myItems.topRow.recieved == 0 ){

				MPI_Test(&myItems.topRow.request,&flag,&status);
				if( flag ){
					// i=0
					int c;
					for( j=3; j<3*( (NCOLS/sqrt_comm_sz)-1 ); j = j + 3 ){
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[0][j+c] = topRow[j-3+c]*((double)1/16) 		/* leftUpCorn */
										   + myArray[0][j-3+c]*((double)2/16)		/* left */
										   + myArray[0+1][j-3+c]*((double)1/16)	/* leftDownCorn */
										   + topRow[j+c]*((double)2/16)			/* up */
										   + myArray[0][j+c]*((double)4/16)		/* itself */
										   + myArray[0+1][j+c]*((double)2/16)		/* down */
										   + topRow[j+3+c]*((double)1/16)			/* rightUpCorn */
										   + myArray[0][j+3+c]*((double)2/16)		/* right */
										   + myArray[0+1][j+3+c]*((double)1/16);	/* rightDownCorn*/
						}
					}
					myItems.topRow.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}
			else if( myItems.topRow.recieved == 2 ){
				// i=0
				for( j=3; j<3*( (NCOLS/sqrt_comm_sz)-1 ); j = j + 3 ){
					for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
						myFinalArray[0][j+c] = myArray[0][j+c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][j-3+c]*((double)2/16)		/* left */
									   + myArray[0+1][j-3+c]*((double)1/16)	/* leftDownCorn */
									   + myArray[0][j+c]*((double)2/16)		/* up */		/* <<< WE USE ITSELF */
									   + myArray[0][j+c]*((double)4/16)		/* itself */
									   + myArray[0+1][j+c]*((double)2/16)		/* down */
									   + myArray[0][j+c]*((double)1/16)		/* rightUpCorn */ /* <<< WE USE ITSELF */
									   + myArray[0][j+3+c]*((double)2/16)		/* right */
									   + myArray[0+1][j+3+c]*((double)1/16);	/* rightDownCorn*/
					}
				}
				myItems.topRow.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}


			/* Apply filter on the inner bottomRow */
			if( myItems.bottomRow.recieved == 0 ){

				MPI_Test(&myItems.bottomRow.request,&flag,&status);
				if( flag ){
					int s = (NROWS/sqrt_comm_sz)-1;
					for( j=3; j<3*( (NCOLS/sqrt_comm_sz)-1 ); j = j + 3 ){
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[s][j+c] = myArray[s-1][j-3+c]*((double)1/16) 	/* leftUpCorn */
										   + myArray[s][j-3+c]*((double)2/16)		/* left */
										   + bottomRow[j-3+c]*((double)1/16)		/* leftDownCorn */
										   + myArray[s-1][j+c]*((double)2/16)		/* up */
										   + myArray[s][j+c]*((double)4/16)		/* itself */
										   + bottomRow[j+c]*((double)2/16)		/* down */
										   + myArray[s-1][j+3+c]*((double)1/16)	/* rightUpCorn */
										   + myArray[s][j+3+c]*((double)2/16)		/* right */
										   + bottomRow[j+3+c]*((double)1/16);		/* rightDownCorn*/
						}
					}
					myItems.bottomRow.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}
			else if( myItems.bottomRow.recieved == 2 ){
				int s = (NROWS/sqrt_comm_sz)-1;
				for( j=3; j<3*( (NCOLS/sqrt_comm_sz)-1 ); j = j + 3 ){
					for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
						myFinalArray[s][j+c] = myArray[s-1][j-3+c]*((double)1/16) 	/* leftUpCorn */
									   + myArray[s][j-3+c]*((double)2/16)		/* left */
									   + myArray[s][j+c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[s-1][j+c]*((double)2/16)		/* up */
									   + myArray[s][j+c]*((double)4/16)		/* itself */
									   + myArray[s][j+c]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
									   + myArray[s-1][j+3+c]*((double)1/16)	/* rightUpCorn */
									   + myArray[s][j+3+c]*((double)2/16)		/* right */
									   + myArray[s][j+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
					}
				}
				myItems.bottomRow.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}

			/* Apply filter on the inner leftCol */
			if( myItems.leftCol.recieved == 0 ){

				MPI_Test(&myItems.leftCol.request,&flag,&status);
				if( flag ){
					//j=0
					for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[j][c] = leftCol[3*j-3+c]*((double)1/16) 		/* leftUpCorn */
											   + leftCol[3*j+c]*((double)2/16)			/* left */
											   + leftCol[3*j+3+c]*((double)1/16)		/* leftDownCorn */
											   + myArray[j-1][c]*((double)2/16)		/* up */
											   + myArray[j][c]*((double)4/16)		/* itself */
											   + myArray[j+1][c]*((double)2/16)		/* down */
											   + myArray[j-1][c+3]*((double)1/16)	/* rightUpCorn */
											   + myArray[j][c+3]*((double)2/16)		/* right */
											   + myArray[j+1][c+3]*((double)1/16);	/* rightDownCorn*/
						}
					}
					myItems.leftCol.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}
			else if( myItems.leftCol.recieved == 2 ){
				//j=0
				for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
					for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
						myFinalArray[j][c] = myArray[j][c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
										   + myArray[j][c]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
										   + myArray[j][c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
										   + myArray[j-1][c]*((double)2/16)		/* up */
										   + myArray[j][c]*((double)4/16)		/* itself */
										   + myArray[j+1][c]*((double)2/16)		/* down */
										   + myArray[j-1][c+3]*((double)1/16)	/* rightUpCorn */
										   + myArray[j][c+3]*((double)2/16)		/* right */
										   + myArray[j+1][c+3]*((double)1/16);	/* rightDownCorn*/
					}

				}
				myItems.leftCol.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}

			/* Apply filter on the inner rightCol */
			if( myItems.rightCol.recieved == 0 ){

				MPI_Test(&myItems.rightCol.request,&flag,&status);
				if( flag ){
					int s = 3 * (NCOLS/sqrt_comm_sz) - 3;
					for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[j][s+c] = myArray[j-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
										   + myArray[j][s-3+c]*((double)2/16)		/* left */
										   + myArray[j+1][s-3+c]*((double)1/16)	/* leftDownCorn */
										   + myArray[j-1][s+c]*((double)2/16)		/* up */
										   + myArray[j][s+c]*((double)4/16)		/* itself */
										   + myArray[j+1][s+c]*((double)2/16)		/* down */
										   + rightCol[(3*j)-3+c]*((double)1/16)		/* rightUpCorn */
										   + rightCol[(3*j)+c]*((double)2/16)			/* right */
										   + rightCol[(3*j)+3+c]*((double)1/16);		/* rightDownCorn*/
						}
					}
					myItems.rightCol.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}
			else if( myItems.rightCol.recieved == 2 ){
				int s = 3 * (NCOLS/sqrt_comm_sz)-3;
				for( j=1; j<(NROWS/sqrt_comm_sz)-1; j++ ){
					for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
						myFinalArray[j][s+c] = myArray[j-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
									   + myArray[j][s-3+c]*((double)2/16)		/* left */
									   + myArray[j+1][s-3+c]*((double)1/16)	/* leftDownCorn */
									   + myArray[j-1][s+c]*((double)2/16)		/* up */
									   + myArray[j][s+c]*((double)4/16)		/* itself */
									   + myArray[j+1][s+c]*((double)2/16)		/* down */
									   + myArray[j][s+c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[j][s+c]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[j][s+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
					}

				}
				myItems.rightCol.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}

			/* Apply filter on the leftUpCorn */
			if( (myItems.topRow.recieved == 3) && (myItems.leftCol.recieved == 3) && (myItems.leftUpCorn.recieved == 2) ){
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					//i=0 and j=0
					myFinalArray[0][c] = myArray[0][c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)4/16)		/* itself */
									   + myArray[0+1][c]*((double)2/16)		/* down */
									   + myArray[0][c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][c+3]*((double)2/16)		/* right */
									   + myArray[0+1][c+3]*((double)1/16);	/* rightDownCorn*/
				}
				myItems.leftUpCorn.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.topRow.recieved == 3) && (myItems.leftCol.recieved == 1) && (myItems.leftUpCorn.recieved == 2) ){
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					//i=0 and j=0
					myFinalArray[0][c] = myArray[0][c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + leftCol[0+c]*((double)2/16)			/* left */
									   + leftCol[3+c]*((double)1/16)			/* leftDownCorn */
									   + myArray[0][c]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)4/16)		/* itself */
									   + myArray[0+1][c]*((double)2/16)		/* down */
									   + myArray[0][c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][c+3]*((double)2/16)		/* right */
									   + myArray[0+1][c+3]*((double)1/16);	/* rightDownCorn*/
				}
				myItems.leftUpCorn.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.topRow.recieved == 1) && (myItems.leftCol.recieved == 3) && (myItems.leftUpCorn.recieved == 2) ){
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					//i=0 and j=0
					myFinalArray[0][c] = myArray[0][c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
									   + myArray[0][c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + topRow[0+c]*((double)2/16)			/* up */
									   + myArray[0][c]*((double)4/16)		/* itself */
									   + myArray[0+1][c]*((double)2/16)		/* down */
									   + topRow[3+c]*((double)1/16)			/* rightUpCorn */
									   + myArray[0][c+3]*((double)2/16)		/* right */
									   + myArray[0+1][c+3]*((double)1/16);	/* rightDownCorn*/
				}
				myItems.leftUpCorn.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.topRow.recieved == 1) && (myItems.leftCol.recieved == 1) && (myItems.leftUpCorn.recieved == 0) ){

				MPI_Test(&myItems.leftUpCorn.request,&flag,&status);
				if( flag ){
					for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
						//i=0 and j=0
						myFinalArray[0][c] = leftUpCorn[c]*((double)1/16) 			/* leftUpCorn */
										   + leftCol[0+c]*((double)2/16)			/* left */
										   + leftCol[3+c]*((double)1/16)			/* leftDownCorn */
										   + topRow[0+c]*((double)2/16)			/* up */
										   + myArray[0][c]*((double)4/16)		/* itself */
										   + myArray[0+1][c]*((double)2/16)		/* down */
										   + topRow[3+c]*((double)1/16)			/* rightUpCorn */
										   + myArray[0][c+3]*((double)2/16)		/* right */
										   + myArray[0+1][c+3]*((double)1/16);	/* rightDownCorn*/
					}
					myItems.leftUpCorn.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the rightUpCorn */
			if( (myItems.topRow.recieved == 3) && (myItems.rightCol.recieved == 3) && (myItems.rightUpCorn.recieved == 2) ){
				//i=0
				int r = 3*(NCOLS/sqrt_comm_sz) - 3 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[0][r+c] = myArray[0][r+c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][r-3+c]*((double)2/16)		/* left */
									   + myArray[0+1][r-3+c]*((double)1/16)	/* leftDownCorn */
									   + myArray[0][r+c]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
									   + myArray[0][r+c]*((double)4/16)		/* itself */
									   + myArray[0+1][r+c]*((double)2/16)		/* down */
									   + myArray[0][r+c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][r+c]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[0][r+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}


				myItems.rightUpCorn.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.topRow.recieved == 3) && (myItems.rightCol.recieved == 1) && (myItems.rightUpCorn.recieved == 2) ){
				//i=0
				int r = 3*(NCOLS/sqrt_comm_sz) - 3 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[0][r+c] = myArray[0][r+c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][r-3+c]*((double)2/16)		/* left */
									   + myArray[0+1][r-3+c]*((double)1/16)	/* leftDownCorn */
									   + myArray[0][r+c]*((double)2/16)		/* up */			/* <<< WE USE ITSELF */
									   + myArray[0][r+c]*((double)4/16)		/* itself */
									   + myArray[0+1][r+c]*((double)2/16)		/* down */
									   + myArray[0][r+c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + rightCol[0+c]*((double)2/16)			/* right */
									   + rightCol[3+c]*((double)1/16);		/* rightDownCorn*/
				}
				myItems.rightUpCorn.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.topRow.recieved == 1) && (myItems.rightCol.recieved == 3) && (myItems.rightUpCorn.recieved == 2) ){
				//i=0
				int r = 3*(NCOLS/sqrt_comm_sz) - 3 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[0][r+c] = topRow[r-3+c]*((double)1/16) 		/* leftUpCorn */
									   + myArray[0][r-3+c]*((double)2/16)		/* left */
									   + myArray[0+1][r-3+c]*((double)1/16)	/* leftDownCorn */
									   + topRow[r+c]*((double)2/16)			/* up */
									   + myArray[0][r+c]*((double)4/16)		/* itself */
									   + myArray[0+1][r+c]*((double)2/16)		/* down */
									   + myArray[0][r+c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[0][r+c]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[0][r+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}
				myItems.rightUpCorn.recieved = 3; /* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.topRow.recieved == 1) && (myItems.rightCol.recieved == 1) && (myItems.rightUpCorn.recieved == 0) ){

					MPI_Test(&myItems.rightUpCorn.request,&flag,&status);
					if( flag ){
						//i=0
						int r = 3*(NCOLS/sqrt_comm_sz) - 3 ;
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[0][r+c] = topRow[r-3+c]*((double)1/16) 		/* leftUpCorn */
											   + myArray[0][r-3+c]*((double)2/16)		/* left */
											   + myArray[0+1][r-3+c]*((double)1/16)	/* leftDownCorn */
											   + topRow[r+c]*((double)2/16)			/* up */
											   + myArray[0][r+c]*((double)4/16)		/* itself */
											   + myArray[0+1][r+c]*((double)2/16)		/* down */
											   + rightUpCorn[c]*((double)1/16)			/* rightUpCorn */
											   + rightCol[0+c]*((double)2/16)			/* right */
											   + rightCol[3+c]*((double)1/16);		/* rightDownCorn*/
						}
						myItems.rightUpCorn.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Apply filter on the leftDownCorn */
			if( (myItems.bottomRow.recieved == 3) && (myItems.leftCol.recieved == 3) && (myItems.leftDownCorn.recieved == 2) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[r][c] = myArray[r][c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[r][c]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
									   + myArray[r][c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[r-1][c]*((double)2/16)		/* up */
									   + myArray[r][c]*((double)4/16)		/* itself */
									   + myArray[r][c]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
									   + myArray[r-1][c+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[r][c+3]*((double)2/16)		/* right */
									   + myArray[r][c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}


				myItems.leftDownCorn.recieved = 3;	/* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.bottomRow.recieved == 3) && (myItems.leftCol.recieved == 1) && (myItems.leftDownCorn.recieved == 2) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[r][c] = leftCol[3*r-3+c]*((double)1/16) 		/* leftUpCorn */
									   + leftCol[3*r+c]*((double)2/16)			/* left */
									   + myArray[r][c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[r-1][c]*((double)2/16)		/* up */
									   + myArray[r][c]*((double)4/16)		/* itself */
									   + myArray[r][c]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
									   + myArray[r-1][c+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[r][c+3]*((double)2/16)		/* right */
									   + myArray[r][c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}
				myItems.leftDownCorn.recieved = 3;	/* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.bottomRow.recieved == 1) && (myItems.leftCol.recieved == 3) && (myItems.leftDownCorn.recieved == 2) ){
				//j=0
				int r = NROWS/sqrt_comm_sz - 1 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[r][c] = myArray[r][c]*((double)1/16) 		/* leftUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[r][c]*((double)2/16)		/* left */			/* <<< WE USE ITSELF */
									   + myArray[r][c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[r-1][c]*((double)2/16)		/* up */
									   + myArray[r][c]*((double)4/16)		/* itself */
									   + bottomRow[0+c]*((double)2/16)		/* down */
									   + myArray[r-1][c+3]*((double)1/16)	/* rightUpCorn */
									   + myArray[r][c+3]*((double)2/16)		/* right */
									   + bottomRow[3+c]*((double)1/16);		/* rightDownCorn*/
				}
				myItems.leftDownCorn.recieved = 3;	/* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.bottomRow.recieved == 1) && (myItems.leftCol.recieved == 1) && (myItems.leftDownCorn.recieved == 0) ){

					MPI_Test(&myItems.leftDownCorn.request,&flag,&status);
					if( flag ){
						//j=0
						int r = NROWS/sqrt_comm_sz - 1 ;
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[r][c] = leftCol[3*r-3+c]*((double)1/16) 		/* leftUpCorn */
											   + leftCol[3*r+c]*((double)2/16)			/* left */
											   + leftDownCorn[c]*((double)1/16)		/* leftDownCorn */
											   + myArray[r-1][c]*((double)2/16)		/* up */
											   + myArray[r][c]*((double)4/16)		/* itself */
											   + bottomRow[0+c]*((double)2/16)		/* down */
											   + myArray[r-1][c+3]*((double)1/16)	/* rightUpCorn */
											   + myArray[r][c+3]*((double)2/16)		/* right */
											   + bottomRow[3+c]*((double)1/16);		/* rightDownCorn*/
						}
						myItems.leftDownCorn.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}
			}

			/* Apply filter on the rightDownCorn */
			if( (myItems.bottomRow.recieved == 3) && (myItems.rightCol.recieved == 3) && (myItems.rightDownCorn.recieved == 2) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = 3*(NCOLS/sqrt_comm_sz) - 3 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[r][s+c] = myArray[r-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
									   + myArray[r][s-3+c]*((double)2/16)		/* left */
									   + myArray[r][s+c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[r-1][s+c]*((double)2/16)		/* up */
									   + myArray[r][s+c]*((double)4/16)		/* itself */
									   + myArray[r][s+c]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
									   + myArray[r][s+c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[r][s+c]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[r][s+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}


				myItems.rightDownCorn.recieved = 3;	/* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.bottomRow.recieved == 3) && (myItems.rightCol.recieved == 1) && (myItems.rightDownCorn.recieved == 2) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = 3*(NCOLS/sqrt_comm_sz) - 3 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[r][s+c] = myArray[r-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
									   + myArray[r][s-3+c]*((double)2/16)		/* left */
									   + myArray[r][s+c]*((double)1/16)		/* leftDownCorn */	/* <<< WE USE ITSELF */
									   + myArray[r-1][s+c]*((double)2/16)		/* up */
									   + myArray[r][s+c]*((double)4/16)		/* itself */
									   + myArray[r][s+c]*((double)2/16)		/* down */			/* <<< WE USE ITSELF */
									   + rightCol[3*r-3+c]*((double)1/16)		/* rightUpCorn */
									   + rightCol[3*r+c]*((double)2/16)			/* right */
									   + myArray[r][s+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}


				myItems.rightDownCorn.recieved = 3;	/* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.bottomRow.recieved == 1) && (myItems.rightCol.recieved == 3) && (myItems.rightDownCorn.recieved == 2) ){

				int r = NROWS/sqrt_comm_sz - 1 ;
				int s = 3*(NCOLS/sqrt_comm_sz) - 3 ;
				for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
					myFinalArray[r][s+c] = myArray[r-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
									   + myArray[r][s-3+c]*((double)2/16)		/* left */
									   + bottomRow[s-3+c]*((double)1/16)		/* leftDownCorn */
									   + myArray[r-1][s+c]*((double)2/16)		/* up */
									   + myArray[r][s+c]*((double)4/16)		/* itself */
									   + bottomRow[s+c]*((double)2/16)		/* down */
									   + myArray[r][s+c]*((double)1/16)		/* rightUpCorn */	/* <<< WE USE ITSELF */
									   + myArray[r][s+c]*((double)2/16)		/* right */			/* <<< WE USE ITSELF */
									   + myArray[r][s+c]*((double)1/16);		/* rightDownCorn*/	/* <<< WE USE ITSELF */
				}


				myItems.rightDownCorn.recieved = 3;	/* It's ok we applied filter to it. */
				counterItems ++;
			}
			else if( (myItems.bottomRow.recieved == 1) && (myItems.rightCol.recieved == 1) && (myItems.rightDownCorn.recieved == 0) ){

					MPI_Test(&myItems.rightDownCorn.request,&flag,&status);
					if( flag ){
						int r = NROWS/sqrt_comm_sz - 1 ;
						int s = 3*(NCOLS/sqrt_comm_sz) - 3 ;
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[r][s+c] = myArray[r-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
											   + myArray[r][s-3+c]*((double)2/16)		/* left */
											   + bottomRow[s-3+c]*((double)1/16)		/* leftDownCorn */
											   + myArray[r-1][s+c]*((double)2/16)		/* up */
											   + myArray[r][s+c]*((double)4/16)		/* itself */
											   + bottomRow[s+c]*((double)2/16)		/* down */
											   + rightCol[3*r-3+c]*((double)1/16)		/* rightUpCorn */
											   + rightCol[3*r+c]*((double)2/16)			/* right */
											   + rightDownCorn[c]*((double)1/16);		/* rightDownCorn*/
						}

						myItems.rightDownCorn.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

		}

		/* Swap arrays */
		temp = myArray;
		myArray = myFinalArray;
		myFinalArray = temp;
	}

	MPI_Gatherv(*myArray,(NROWS/sqrt_comm_sz)*3*(NCOLS/sqrt_comm_sz),MPI_UNSIGNED_CHAR,*final2D,counts,displs,resizedtype,0,MPI_COMM_WORLD );

	if( my_rank == 0 ){

		FILE *outputFile = fopen("mpiOutputColor.raw", "w");

		for( i=0; i<NROWS; i++ ){
			for( j=0; j<3*NCOLS; j++ ){
				//printf("%c", final2D[i][j]);
				fputc(final2D[i][j], outputFile);
			}
		}
	}


	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
