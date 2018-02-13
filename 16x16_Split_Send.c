#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int comm_sz;               /* Number of processes    */
	int my_rank;               /* My process rank        */

	/* Subarray type */
	MPI_Datatype type,resizedtype;

	MPI_Request request;
	MPI_Status status;

	MPI_Datatype column;
	int rightCol[1][8];
	int leftCol[1][8];

	MPI_Datatype row;
	int topRow[8];
	int bottomRow[8];

	int leftUpCorn,rightUpCorn,rightDownCorn,leftDownCorn;

	int i;

	int array2D[16][16]; /* original array */
	int myArray[8][8]; /* local array of each process */

	/* Arguments of create_subarray */
	int sizes[2] = {16,16}; /* size of original array */
	int subsizes[2] = {8,8}; /* size of subArrays */
	int starts[2] = {0,0}; /* first subArray starts from index [0][0] */

	/* Arguments of Scatterv */
	int counts[4] = {1,1,1,1}; /* How many pieces of data everyone has in units of block */
	int displs[4] = {0,1,16,17}; /* The starting point of everyone's data in the global array, in block extents ( 3-ints )  */

	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* Creating derived Datatype */
	MPI_Type_create_subarray(2,sizes,subsizes,starts,MPI_ORDER_C,MPI_INT,&type);
	MPI_Type_create_resized(type,0,8*sizeof(int),&resizedtype);
	MPI_Type_commit(&resizedtype);

	MPI_Type_vector(8,1,8,MPI_INT,&column);
	MPI_Type_commit(&column);

	MPI_Type_contiguous(8,MPI_INT,&row);
	MPI_Type_commit(&row);


	if (my_rank == 0) {
		/* Fill in array */

		for( i=0; i<16; i++ ){ /* comm_sz must be 4 */

			array2D[i][0] = i*10 + 1;
			array2D[i][1] = i*10 + 2;
			array2D[i][2] = i*10 + 3;
			array2D[i][3] = i*10 + 4;
			array2D[i][4] = i*10 + 5;
			array2D[i][5] = i*10 + 6;
			array2D[i][6] = i*10 + 7;
			array2D[i][7] = i*10 + 8;
			array2D[i][8] = i*10 + 9;
			array2D[i][9] = i*10 + 10;
			array2D[i][10] = i*10 + 11;
			array2D[i][11] = i*10 + 12;
			array2D[i][12] = i*10 + 13;
			array2D[i][13] = i*10 + 14;
			array2D[i][14] = i*10 + 15;
			array2D[i][15] = i*10 + 16;

		}

	}

	MPI_Scatterv(array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 myArray,8*8,MPI_INT, /* I'm recieving 3*3 MPI_INTs into myArray */
				 0,MPI_COMM_WORLD );



	/* Print subArrays */
	printf("SubArray of process %d is :\n",my_rank );
	for( i=0; i<8; i++ ){
		printf("%d %d %d %d %d %d %d %d\n",myArray[i][0],myArray[i][1],myArray[i][2],myArray[i][3],myArray[i][4],myArray[i][5],myArray[i][6],myArray[i][7] );
	}
	printf("\n\n");

	/* Send and recieve rows,columns and corners */
	if( my_rank == 0 ){

		/* Send my bottomRow to process 2 */
		MPI_Send(myArray[7],1,row,2,0,MPI_COMM_WORLD);

		/* Send my rightCol to process 1 */
		//printf("tha steilei to myArray[0][7] pou exei %d %d %d %d %d \n",myArray[0][7],myArray[1][7],myArray[2][7],myArray[3][7],myArray[4][7] );
		MPI_Send(&myArray[0][7],1,column,1,0,MPI_COMM_WORLD);

		/* Send my rightDownCorn to process 3 */
		MPI_Send(&myArray[7][7],1,MPI_INT,3,0,MPI_COMM_WORLD);

		/* Recieve rightCol from process 1 */
		MPI_Recv(&rightCol,1,column,1,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from process 2 */
		MPI_Recv(bottomRow,1,row,2,0,MPI_COMM_WORLD, &status);

		/* Recieve rightDownCorn from process 3 */
		MPI_Recv(&rightDownCorn,1,MPI_INT,3,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank == 1 ){

		/* Recieve leftCol from process 0 */
		MPI_Recv(&leftCol,1,column,0,0,MPI_COMM_WORLD, &status);
		printf("pire thn column kai einai %d %d %d %d\n",leftCol[0][0],leftCol[1][0],leftCol[2][0],leftCol[3][0] );
		/* Send my leftCol to process 0 */
		MPI_Send(&myArray[0][0],1,column,0,0,MPI_COMM_WORLD);

		/* Send my leftDownCorn to process 2 */
		MPI_Send(&myArray[7][0],1,MPI_INT,2,0,MPI_COMM_WORLD);

		/* Send my bottomRow to process 3 */
		MPI_Send(myArray[7],1,row,3,0,MPI_COMM_WORLD);

		/* Recieve leftDownCorn from process 2 */
		MPI_Recv(&leftDownCorn,1,MPI_INT,2,0,MPI_COMM_WORLD, &status);

		/* Recieve bottomRow from process 3 */
		MPI_Recv(bottomRow,1,row,3,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank == 2 ){

		/* Recieve topRow from process 0 */
		MPI_Recv(topRow,1,row,0,0,MPI_COMM_WORLD, &status);

		/* Send my topRow to process 0 */
		MPI_Send(myArray[0],1,row,0,0,MPI_COMM_WORLD);

		/* Recieve rightUpCorn from process 1 */
		MPI_Recv(&rightUpCorn,1,MPI_INT,1,0,MPI_COMM_WORLD, &status);

		/* Send my rightUpCorn to process 1 */
		MPI_Send(&myArray[0][7],1,MPI_INT,1,0,MPI_COMM_WORLD);

		/* Send my rightCol to process 3 */
		MPI_Send(&myArray[0][7],1,column,3,0,MPI_COMM_WORLD);

		/* Recieve rightCol from process 3 */
		MPI_Recv(&rightCol,1,column,3,0,MPI_COMM_WORLD, &status);

	}
	else if( my_rank == 3 ){

		/* Recieve leftUpCorn from process 0 */
		MPI_Recv(&leftUpCorn,1,MPI_INT,0,0,MPI_COMM_WORLD, &status);

		/* Send my leftUpCorn to process 0 */
		MPI_Send(&myArray[0][0],1,MPI_INT,0,0,MPI_COMM_WORLD);

		/* Recieve topRow from process 1 */
		MPI_Recv(topRow,1,row,1,0,MPI_COMM_WORLD, &status);

		/* Send my topRow to process 1 */
		MPI_Send(myArray[0],1,row,1,0,MPI_COMM_WORLD);

		/* Recieve leftCol from process 2 */
		MPI_Recv(&leftCol,1,column,2,0,MPI_COMM_WORLD, &status);

		/* Send my leftCol to process 2 */
		MPI_Send(&myArray[0][7],1,column,2,0,MPI_COMM_WORLD);

	}


	/* Print recieved rows,columns and corners */
	if( my_rank == 0 ){

		printf("Im process %d and I recieved:\n",my_rank );

		printf("\tbottomRow from process 2:\n\t");
		for( i=0; i<8; i++ ){
		printf("%d ",bottomRow[i] );
		}
		printf("\n");

		printf("\trightCol from process 1:\n");
		printf("pou einai %d %d %d %d %d %d %d %d\n",rightCol[0][0],rightCol[1][0],rightCol[2][0],rightCol[3][0],rightCol[4][0],rightCol[5][0],rightCol[6][0],rightCol[7][0] );
		for( i=0; i<8; i++ ){
			printf("\t%d\n",rightCol[i][0] );
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
		for( i=0; i<8; i++ ){
			printf("%d ",bottomRow[i] );
		}
		printf("\n");

		printf("\tleftCol from process 0:\n");
		printf("pou einai %d %d %d %d %d %d %d %d\n",leftCol[0][0],leftCol[1][0],leftCol[2][0],leftCol[3][0],leftCol[4][0],leftCol[5][0],leftCol[6][0],leftCol[7][0] );
		for( i=0; i<8; i++ ){
			printf("\t%d\n",leftCol[i][0] );
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
		for( i=0; i<8; i++ ){
			printf("%d ",topRow[i] );
		}
		printf("\n");

		printf("\trightCol from process 3:\n");
		for( i=0; i<8; i++ ){
			printf("\t%d\n",rightCol[i][0] );
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
		for( i=0; i<8; i++ ){
			printf("%d ",topRow[i] );
		}
		printf("\n");

		printf("\tleftCol from process 2:\n");
		for( i=0; i<8; i++ ){
			printf("\t%d\n",leftCol[i][0] );
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
