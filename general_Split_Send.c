#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define NROWS 16
#define NCOLS 24

#define TOPLEFT     my_rank-sqrt_comm_sz-1
#define TOP         my_rank-sqrt_comm_sz
#define TOPRIGHT    my_rank-sqrt_comm_sz+1
#define LEFT        my_rank-1
#define RIGHT       my_rank+1
#define BOTTOMLEFT  my_rank+sqrt_comm_sz-1
#define BOTTOM      my_rank+sqrt_comm_sz
#define BOTTOMRIGHT my_rank+sqrt_comm_sz+1

int ** get2DArray(int rows, int cols){
    int * space = malloc(rows * cols * sizeof(int));
    int ** array = malloc(rows * sizeof(int*));

    for(int i=0; i<rows; i++){
        array[i] = space+i*cols;
    }

    return array;
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

	int **array2D; /* original array */
	int **myArray; /* local array of each process */

	int *topRow;
	int *bottomRow;

	int *leftCol;
	int *rightCol;

	int leftUpCorn,rightUpCorn,rightDownCorn,leftDownCorn;

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

	topRow = malloc( (NCOLS/sqrt_comm_sz) * sizeof(int) );
	bottomRow = malloc( (NCOLS/sqrt_comm_sz) * sizeof(int) );

	leftCol = malloc( (NROWS/sqrt_comm_sz) * sizeof(int) );
	rightCol = malloc( (NROWS/sqrt_comm_sz) * sizeof(int) );

	array2D = get2DArray(NROWS,NCOLS);

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
	MPI_Type_create_subarray(2,sizes,subsizes,starts,MPI_ORDER_C,MPI_INT,&type);
	MPI_Type_create_resized(type,0,(NCOLS/sqrt_comm_sz)*sizeof(int),&resizedtype);
	MPI_Type_commit(&resizedtype);

	MPI_Type_contiguous(NCOLS/sqrt_comm_sz,MPI_INT,&row);
	MPI_Type_commit(&row);

	MPI_Type_vector(NROWS/sqrt_comm_sz,1,NCOLS/sqrt_comm_sz,MPI_INT,&column);
	MPI_Type_commit(&column);

	if (my_rank == 0) {
		/* Fill in array */
		for( i=0; i<NROWS; i++ ){
			for( j=0; j<NCOLS; j++ ){
				array2D[i][j] = j + i*10;
			}
		}
	}

	MPI_Scatterv(*array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 *myArray,(NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz),MPI_INT, /* I'm recieving (NROWS/sqrt_comm_sz)*(NCOLS/sqrt_comm_sz) MPI_INTs into myArray */
				 0,MPI_COMM_WORLD );

	/* Print subArrays */
	printf("SubArray of process %d is :\n",my_rank );
	for( i=0; i<NROWS/sqrt_comm_sz; i++ ){
		for( j=0; j<NCOLS/sqrt_comm_sz; j++ ){
			printf("%d ",myArray[i][j] );
		}
		printf("\n" );
	}
	printf("\n\n");

	/* Send and recieve rows,columns and corners */
	if( my_rank < sqrt_comm_sz && my_rank%sqrt_comm_sz == 0 ){ //top left corner

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][(NCOLS/sqrt_comm_sz)-1],1,MPI_INT,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_INT,my_rank+1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Recv(&rightDownCorn,1,MPI_INT,my_rank+sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank < sqrt_comm_sz && my_rank%sqrt_comm_sz == sqrt_comm_sz-1 ){ //top right corner

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(NROWS/sqrt_comm_sz)-1][0],1,MPI_INT,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(NROWS/sqrt_comm_sz)-1],1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_INT,my_rank-1,0,MPI_COMM_WORLD, &status);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Recv(&leftDownCorn,1,MPI_INT,my_rank+sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from my bottom process */
		MPI_Recv(bottomRow,1,row,my_rank+sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank >= comm_sz-sqrt_comm_sz && my_rank%sqrt_comm_sz == 0 ){ //bottom left corner

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,MPI_INT,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD,&request);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][(NCOLS/sqrt_comm_sz)-1],1,column,my_rank+1,0,MPI_COMM_WORLD,&request);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Recv(&rightUpCorn,1,MPI_INT,my_rank-sqrt_comm_sz+1,0,MPI_COMM_WORLD, &status);

		/* Recieve rightCol from my right process */
		MPI_Recv(rightCol,(NROWS/sqrt_comm_sz),MPI_INT,my_rank+1,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank >= comm_sz-sqrt_comm_sz && my_rank%sqrt_comm_sz == sqrt_comm_sz-1 ){ //bottom right corner

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],1,MPI_INT,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD,&request);

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD,&request);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,my_rank-1,0,MPI_COMM_WORLD,&request);

		/* Recieve leftUpCorn from my left process */
		MPI_Recv(&leftUpCorn,1,MPI_INT,my_rank-sqrt_comm_sz-1,0,MPI_COMM_WORLD, &status);

		/* Recieve topRow from my top process */
		MPI_Recv(topRow,1,row,my_rank-sqrt_comm_sz,0,MPI_COMM_WORLD, &status);

		/* Recieve leftCol from my left process */
		MPI_Recv(leftCol,(NROWS/sqrt_comm_sz),MPI_INT,my_rank-1,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank < sqrt_comm_sz ){ //top row

	}
	else if( my_rank >= comm_sz - sqrt_comm_sz ){ //bottom row

	}
	else if( my_rank%sqrt_comm_sz == 0 ){ //leftmost column

	}
	else if( my_rank%sqrt_comm_sz == sqrt_comm_sz-1 ){ //rightmost column

	}
	else{ //everything in between

	}



	/* Print recieved rows,columns and corners */
	if( my_rank == 0 ){

		printf("Im process %d and I recieved:\n",my_rank );

		printf("\tbottomRow from process 2:\n\t");
		for( i=0; i<NCOLS/sqrt_comm_sz; i++ ){
			printf("%d ",bottomRow[i] );
		}
		printf("\n");

		printf("\trightCol from process 1:\n");
		for( i=0; i<(NROWS/sqrt_comm_sz); i++ ){
			printf("\t%d\n",rightCol[i] );
		}
		printf("\n");

		printf("\trightDownCorn from process 3:\n");
		printf("\t%d\n",rightDownCorn );
		printf("\n" );

		printf("\n");

	}
	else if( my_rank == 1 ){

		printf("Im process %d and I recieved:\n",my_rank );

		printf("\tbottomRow from process 3:\n\t");
		for( i=0; i<NCOLS/sqrt_comm_sz; i++ ){
			printf("%d ",bottomRow[i] );
		}
		printf("\n");

		printf("\tleftCol from process 0:\n");
		for( i=0; i<NROWS/sqrt_comm_sz; i++ ){
			printf("\t%d\n",leftCol[i] );
		}
		printf("\n");

		printf("\tleftDownCorn from process 2:\n");
		printf("\t%d\n",leftDownCorn );
		printf("\n" );

		printf("\n");

	}
	else if( my_rank == 2 ){

		printf("Im process %d and I recieved:\n",my_rank );

		printf("\ttopRow from process 0:\n\t");
		for( i=0; i<NCOLS/sqrt_comm_sz; i++ ){
			printf("%d ",topRow[i] );
		}
		printf("\n");

		printf("\trightCol from process 3:\n");
		for( i=0; i<NROWS/sqrt_comm_sz; i++ ){
			printf("\t%d\n",rightCol[i] );
		}
		printf("\n");

		printf("\trightUpCorn from process 1:\n");
		printf("\t%d\n",rightUpCorn );
		printf("\n" );

		printf("\n");

	}
	else if( my_rank == 3 ){

		printf("Im process %d and I recieved:\n",my_rank );

		printf("\ttopRow from process 1:\n\t");
		for( i=0; i<NCOLS/sqrt_comm_sz; i++ ){
			printf("%d ",topRow[i] );
		}
		printf("\n");

		printf("\tleftCol from process 2:\n");
		for( i=0; i<NROWS/sqrt_comm_sz; i++ ){
			printf("\t%d\n",leftCol[i] );
		}
		printf("\n");

		printf("\tleftUpCorn from process 0:\n");
		printf("\t%d\n",leftUpCorn );
		printf("\n" );

		printf("\n");

	}

	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
