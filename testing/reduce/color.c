#include <stdio.h>
#include <mpi.h>     /* For MPI functions, etc */
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TOPLEFT     my_rank-sqrt_comm_sz-1
#define TOP         my_rank-sqrt_comm_sz
#define TOPRIGHT    my_rank-sqrt_comm_sz+1
#define LEFT        my_rank-1
#define RIGHT       my_rank+1
#define BOTTOMLEFT  my_rank+sqrt_comm_sz-1
#define BOTTOM      my_rank+sqrt_comm_sz
#define BOTTOMRIGHT my_rank+sqrt_comm_sz+1


unsigned char ** getRGBArray(int rows, int cols){
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

	double startTime, endTime;

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

	typedef struct neighbor{
		int rank;
		char recieved;	/* 0 -> not been recieved yet, 1 -> recieved  */
		MPI_Request recieveRequest;
		MPI_Request sendRequest;
	} neighbor;

	typedef struct allNeighbors{
		neighbor top;
		neighbor bottom;
		neighbor left;
		neighbor right;
		neighbor leftUp;
		neighbor rightUp;
		neighbor rightDown;
		neighbor leftDown;
	} allNeighbors;

	allNeighbors myNeighbors;

	int counterItems = 0; /* How many items we have recieved */
	int flag = 0;
	int i,j,k;
	char localChanges,globalChanges; /* 0 -> there are no changes, 1 -> there are changes */

	/* Arguments of create_subarray */
	int sizes[2]; /* size of original array */
	int subsizes[2]; /* size of subArrays */
	int starts[2] = {0,0}; /* first subArray starts from index [0][0] */

	/* Arguments of Scatterv */
	int *counts; /* How many pieces of data everyone has in units of block */

	int *displs; /* The starting point of everyone's data in the global array, in block extents ( colsNumber/sqrt_comm_sz ints )  */

	char inputFileName[40] = "waterfall_1920_2520.raw";
	char outputFileName[40] = "outputColor.raw";
	int rowsNumber = 2520;
	int colsNumber = 1920;
	int filterApplications = 100;
	int error = 0;

	for( int i=1; i<argc; i++ ){

		if( strcmp(argv[i],"-i") == 0 ){
			if( i+1 < argc ){
				i++;
				strcpy(inputFileName,argv[i]);
			}
			else{
				error = 1;
				break;
			}
		}
		else if(strcmp(argv[i],"-o") == 0){
			if( i+1 < argc ){
				i++;
				strcpy(outputFileName,argv[i]);
			}
			else{
				error = 1;
				break;
			}
		}
		else if(strcmp(argv[i],"-s") == 0){
			if( i+2 < argc ){
				i++;
				rowsNumber = atoi(argv[i]);
				i++;
				colsNumber = atoi(argv[i]);
			}
			else{
				error = 1;
				break;
			}
		}
		else if(strcmp(argv[i],"-f") == 0){
			if( i+1 < argc ){
				i++;
				filterApplications = atoi(argv[i]);
			}
			else{
				error = 1;
				break;
			}
		}

	}

	/* Start up MPI */
	MPI_Init(&argc, &argv);

	/* Get the number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

	/* Get my rank among all the processes */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	if(error){
		if( my_rank == 0 ){
			printf("Wrong argument. Please execute again with this form: %s [-i inputFileName] [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications] \n",argv[0] );
			return -1;
		}
		else{
			return -1;
		}
	}

	sqrt_comm_sz = sqrt(comm_sz);

	topRow = malloc( (colsNumber/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );
	bottomRow = malloc( (colsNumber/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );

	leftCol = malloc( (rowsNumber/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );
	rightCol = malloc( (rowsNumber/sqrt_comm_sz) * ( 3 * sizeof(unsigned char) ) );

	leftUpCorn = malloc( 3 * sizeof(unsigned char) );
	rightUpCorn = malloc( 3 * sizeof(unsigned char) );
	rightDownCorn = malloc( 3 * sizeof(unsigned char) );
	leftDownCorn = malloc( 3 * sizeof(unsigned char) );

	array2D = getRGBArray(rowsNumber,colsNumber);
	final2D = getRGBArray(rowsNumber,colsNumber);

	myArray = getRGBArray(rowsNumber/sqrt_comm_sz,colsNumber/sqrt_comm_sz);
	myFinalArray = getRGBArray(rowsNumber/sqrt_comm_sz,colsNumber/sqrt_comm_sz);

	/* Arguments of create_subarray */
	sizes[0] = rowsNumber;
	sizes[1] = 3 * colsNumber;
	subsizes[0] = rowsNumber/sqrt_comm_sz;
	subsizes[1] = 3 * (colsNumber/sqrt_comm_sz);

	counts = malloc( comm_sz * sizeof(int) ); /* How many pieces of data everyone has in units of block */
	for ( i=0; i<comm_sz; i++ ){
		counts[i] = 1;
	}

	displs = malloc( comm_sz * sizeof(int) );
	k=0;
	for( i=0; i<sqrt_comm_sz; i++ ){
		for( j=0; j<sqrt_comm_sz; j++ ){
			displs[k] = i*rowsNumber+j;
			k++;
		}
	}

	/* Creating derived Datatype */
	MPI_Type_create_subarray(2,sizes,subsizes,starts,MPI_ORDER_C,MPI_UNSIGNED_CHAR,&type);
	MPI_Type_create_resized(type,0,(colsNumber/sqrt_comm_sz)*( 3 * sizeof(unsigned char) ),&resizedtype);
	MPI_Type_commit(&resizedtype);

	MPI_Type_contiguous(3*(colsNumber/sqrt_comm_sz),MPI_UNSIGNED_CHAR,&row);
	MPI_Type_commit(&row);

	MPI_Type_vector(rowsNumber/sqrt_comm_sz,3,3*(colsNumber/sqrt_comm_sz),MPI_UNSIGNED_CHAR,&column);
	MPI_Type_commit(&column);


	if (my_rank == 0) {

		/* Fill in array from file*/
		FILE *inputFile = fopen(inputFileName, "r");

		for( i=0; i<rowsNumber; i++ ){
			for( j=0; j<3*colsNumber; j = j+3 ){
				array2D[i][j] = fgetc(inputFile);
				array2D[i][j+1] = fgetc(inputFile);
				array2D[i][j+2] = fgetc(inputFile);
			}
		}

		fclose(inputFile);
	}

	MPI_Scatterv(*array2D,counts,displs, /*process i gets counts[i] types(subArrays) from displs[i] */
				 resizedtype,
				 *myArray,(rowsNumber/sqrt_comm_sz)*(3*(colsNumber/sqrt_comm_sz)),MPI_UNSIGNED_CHAR, /* I'm recieving (rowsNumber/sqrt_comm_sz)*3*(colsNumber/sqrt_comm_sz) MPI_UNSIGNED_CHARs into myArray */
				 0,MPI_COMM_WORLD );

	/* Determine my neighbors */
	myNeighbors.top.rank = my_rank-sqrt_comm_sz;
	myNeighbors.bottom.rank = my_rank+sqrt_comm_sz;
	myNeighbors.left.rank = my_rank-1;
	myNeighbors.right.rank = my_rank+1;
	myNeighbors.leftUp.rank = my_rank-sqrt_comm_sz-1;
	myNeighbors.rightUp.rank = my_rank-sqrt_comm_sz+1;
	myNeighbors.rightDown.rank = my_rank+sqrt_comm_sz+1;
	myNeighbors.leftDown.rank = my_rank+sqrt_comm_sz-1;

	/* Which of my neighbors are MPI_PROC_NULL */

	if( my_rank < sqrt_comm_sz ){ //top row

		myNeighbors.top.rank = MPI_PROC_NULL;
		myNeighbors.leftUp.rank = MPI_PROC_NULL;
		myNeighbors.rightUp.rank = MPI_PROC_NULL;

	}

	if( my_rank >= (comm_sz - sqrt_comm_sz) ){ //bottom row

		myNeighbors.bottom.rank = MPI_PROC_NULL;
		myNeighbors.rightDown.rank = MPI_PROC_NULL;
		myNeighbors.leftDown.rank = MPI_PROC_NULL;

	}

	if( (my_rank%sqrt_comm_sz) == 0 ){ //leftmost column

		myNeighbors.left.rank = MPI_PROC_NULL;
		myNeighbors.leftUp.rank = MPI_PROC_NULL;
		myNeighbors.leftDown.rank = MPI_PROC_NULL;

	}

	if( (my_rank%sqrt_comm_sz) == (sqrt_comm_sz-1) ){ //rightmost column

		myNeighbors.right.rank = MPI_PROC_NULL;
		myNeighbors.rightUp.rank = MPI_PROC_NULL;
		myNeighbors.rightDown.rank = MPI_PROC_NULL;

	}

	/* Prepare buffers with default values */
	for( i=0; i<3*(colsNumber/sqrt_comm_sz); i=i+3 ){
		for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
			topRow[i+c] = myArray[0][i+c];
		}
	}

	for( i=0; i<3*(colsNumber/sqrt_comm_sz); i=i+3 ){
		for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
			bottomRow[i+c] = myArray[(rowsNumber/sqrt_comm_sz)-1][i+c];
		}
	}

	for( i=0; i<rowsNumber/sqrt_comm_sz; i++ ){
		for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
			leftCol[3*i+c] = myArray[i][0+c];
		}

	}

	for( i=0; i<rowsNumber/sqrt_comm_sz; i++ ){
		for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
			rightCol[3*i+c] = myArray[i][3*(colsNumber/sqrt_comm_sz)-3+c];
		}
	}

	for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
		leftUpCorn[c] = myArray[0][0+c];
		rightUpCorn[c] = myArray[0][3*(colsNumber/sqrt_comm_sz)-3+c];
		rightDownCorn[c] = myArray[(rowsNumber/sqrt_comm_sz)-1][3*(colsNumber/sqrt_comm_sz)-3+c];
		leftDownCorn[c] = myArray[(rowsNumber/sqrt_comm_sz)-1][0+c];
	}

	MPI_Barrier(MPI_COMM_WORLD);
	startTime = MPI_Wtime();

	for(i=0; i<filterApplications; i++){

		/* Initialize recieved flags for MPI_Test */
		myNeighbors.top.recieved = 0;
		myNeighbors.bottom.recieved = 0;
		myNeighbors.left.recieved = 0;
		myNeighbors.right.recieved = 0;
		myNeighbors.leftUp.recieved = 0;
		myNeighbors.rightUp.recieved = 0;
		myNeighbors.rightDown.recieved = 0;
		myNeighbors.leftDown.recieved = 0;

		/* Send rows,columns and corners */

		/* Send my topRow to my top process */
		MPI_Isend(myArray[0],1,row,myNeighbors.top.rank,0,MPI_COMM_WORLD,&myNeighbors.top.sendRequest);

		/* Send my leftUpCorn to my leftup process */
		MPI_Isend(&myArray[0][0],3,MPI_UNSIGNED_CHAR,myNeighbors.leftUp.rank,0,MPI_COMM_WORLD,&myNeighbors.leftUp.sendRequest);

		/* Send my leftCol to my left process */
		MPI_Isend(&myArray[0][0],1,column,myNeighbors.left.rank,0,MPI_COMM_WORLD,&myNeighbors.left.sendRequest);

		/* Send my bottomRow to my bottom process */
		MPI_Isend(myArray[(rowsNumber/sqrt_comm_sz)-1],1,row,myNeighbors.bottom.rank,0,MPI_COMM_WORLD,&myNeighbors.bottom.sendRequest);

		/* Send my leftDownCorn to my leftdown process */
		MPI_Isend(&myArray[(rowsNumber/sqrt_comm_sz)-1][0],3,MPI_UNSIGNED_CHAR,myNeighbors.leftDown.rank,0,MPI_COMM_WORLD,&myNeighbors.leftDown.sendRequest);

		/* Send my rightUpCorn to my rightup process */
		MPI_Isend(&myArray[0][3*(colsNumber/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,myNeighbors.rightUp.rank,0,MPI_COMM_WORLD,&myNeighbors.rightUp.sendRequest);

		/* Send my rightCol to my right process */
		MPI_Isend(&myArray[0][3*(colsNumber/sqrt_comm_sz)-3],1,column,myNeighbors.right.rank,0,MPI_COMM_WORLD,&myNeighbors.right.sendRequest);

		/* Send my rightDownCorn to my rightdown process */
		MPI_Isend(&myArray[(rowsNumber/sqrt_comm_sz)-1][3*(colsNumber/sqrt_comm_sz)-3],3,MPI_UNSIGNED_CHAR,myNeighbors.rightDown.rank,0,MPI_COMM_WORLD,&myNeighbors.rightDown.sendRequest);


		/* Recieve rows,columns and corners */

		/* Recieve topRow from my top process */
		MPI_Irecv(topRow,1,row,myNeighbors.top.rank,0,MPI_COMM_WORLD, &myNeighbors.top.recieveRequest);

		/* Recieve leftUpCorn from my leftup process */
		MPI_Irecv(leftUpCorn,3,MPI_UNSIGNED_CHAR,myNeighbors.leftUp.rank,0,MPI_COMM_WORLD, &myNeighbors.leftUp.recieveRequest);

		/* Recieve leftCol from my left process */
		MPI_Irecv(leftCol,3*(rowsNumber/sqrt_comm_sz),MPI_UNSIGNED_CHAR,myNeighbors.left.rank,0,MPI_COMM_WORLD, &myNeighbors.left.recieveRequest);

		/* Recieve bottomRow from my bottom process */
		MPI_Irecv(bottomRow,1,row,myNeighbors.bottom.rank,0,MPI_COMM_WORLD, &myNeighbors.bottom.recieveRequest);

		/* Recieve leftDownCorn from my leftdown process */
		MPI_Irecv(leftDownCorn,3,MPI_UNSIGNED_CHAR,myNeighbors.leftDown.rank,0,MPI_COMM_WORLD, &myNeighbors.leftDown.recieveRequest);

		/* Recieve rightUpCorn from my rightup rocess */
		MPI_Irecv(rightUpCorn,3,MPI_UNSIGNED_CHAR,myNeighbors.rightUp.rank,0,MPI_COMM_WORLD, &myNeighbors.rightUp.recieveRequest);

		/* Recieve rightCol from my right process */
		MPI_Irecv(rightCol,3*(rowsNumber/sqrt_comm_sz),MPI_UNSIGNED_CHAR,myNeighbors.right.rank,0,MPI_COMM_WORLD, &myNeighbors.right.recieveRequest);

		/* Recieve rightDownCorn from my rightdown process */
		MPI_Irecv(rightDownCorn,3,MPI_UNSIGNED_CHAR,myNeighbors.rightDown.rank,0,MPI_COMM_WORLD, &myNeighbors.rightDown.recieveRequest);


		/* Apply filter on the inner pixels */
		applyFilter(myArray,myFinalArray,(rowsNumber/sqrt_comm_sz), (colsNumber/sqrt_comm_sz));

		counterItems = 0;

		/* Check what we have recieved and apply filter to these items */
		while( counterItems < 8 ){

			/* Apply filter on the inner topRow */
			if( myNeighbors.top.recieved == 0 ){

				MPI_Test(&myNeighbors.top.recieveRequest,&flag,&status);
				if( flag ){
					// i=0
					for( j=3; j<3*( (colsNumber/sqrt_comm_sz)-1 ); j = j + 3 ){
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
					myNeighbors.top.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the inner bottomRow */
			if( myNeighbors.bottom.recieved == 0 ){

				MPI_Test(&myNeighbors.bottom.recieveRequest,&flag,&status);
				if( flag ){
					int s = (rowsNumber/sqrt_comm_sz)-1;
					for( j=3; j<3*( (colsNumber/sqrt_comm_sz)-1 ); j = j + 3 ){
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
					myNeighbors.bottom.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the inner leftCol */
			if( myNeighbors.left.recieved == 0 ){

				MPI_Test(&myNeighbors.left.recieveRequest,&flag,&status);
				if( flag ){
					//j=0
					for( j=1; j<(rowsNumber/sqrt_comm_sz)-1; j++ ){
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[j][c] = leftCol[3*j-3+c]*((double)1/16) 	/* leftUpCorn */
											   + leftCol[3*j+c]*((double)2/16)		/* left */
											   + leftCol[3*j+3+c]*((double)1/16)	/* leftDownCorn */
											   + myArray[j-1][c]*((double)2/16)		/* up */
											   + myArray[j][c]*((double)4/16)		/* itself */
											   + myArray[j+1][c]*((double)2/16)		/* down */
											   + myArray[j-1][c+3]*((double)1/16)	/* rightUpCorn */
											   + myArray[j][c+3]*((double)2/16)		/* right */
											   + myArray[j+1][c+3]*((double)1/16);	/* rightDownCorn*/
						}
					}
					myNeighbors.left.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the inner rightCol */
			if( myNeighbors.right.recieved == 0 ){

				MPI_Test(&myNeighbors.right.recieveRequest,&flag,&status);
				if( flag ){
					int s = 3 * (colsNumber/sqrt_comm_sz) - 3;
					for( j=1; j<(rowsNumber/sqrt_comm_sz)-1; j++ ){
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[j][s+c] = myArray[j-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
												 + myArray[j][s-3+c]*((double)2/16)		/* left */
												 + myArray[j+1][s-3+c]*((double)1/16)	/* leftDownCorn */
												 + myArray[j-1][s+c]*((double)2/16)		/* up */
												 + myArray[j][s+c]*((double)4/16)		/* itself */
												 + myArray[j+1][s+c]*((double)2/16)		/* down */
												 + rightCol[(3*j)-3+c]*((double)1/16)	/* rightUpCorn */
												 + rightCol[(3*j)+c]*((double)2/16)		/* right */
												 + rightCol[(3*j)+3+c]*((double)1/16);	/* rightDownCorn*/
						}
					}
					myNeighbors.right.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the leftUpCorn */
			if( (myNeighbors.top.recieved == 1) && (myNeighbors.left.recieved == 1) && (myNeighbors.leftUp.recieved == 0) ){

				MPI_Test(&myNeighbors.leftUp.recieveRequest,&flag,&status);
				if( flag ){
					for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
						//i=0 and j=0
						myFinalArray[0][c] = leftUpCorn[c]*((double)1/16) 		/* leftUpCorn */
										   + leftCol[0+c]*((double)2/16)		/* left */
										   + leftCol[3+c]*((double)1/16)		/* leftDownCorn */
										   + topRow[0+c]*((double)2/16)			/* up */
										   + myArray[0][c]*((double)4/16)		/* itself */
										   + myArray[0+1][c]*((double)2/16)		/* down */
										   + topRow[3+c]*((double)1/16)			/* rightUpCorn */
										   + myArray[0][c+3]*((double)2/16)		/* right */
										   + myArray[0+1][c+3]*((double)1/16);	/* rightDownCorn*/
					}
					myNeighbors.leftUp.recieved = 1;
					counterItems ++;
					flag = 0 ;
				}

			}

			/* Apply filter on the rightUpCorn */
			if( (myNeighbors.top.recieved == 1) && (myNeighbors.right.recieved == 1) && (myNeighbors.rightUp.recieved == 0) ){

					MPI_Test(&myNeighbors.rightUp.recieveRequest,&flag,&status);
					if( flag ){
						//i=0
						int r = 3*(colsNumber/sqrt_comm_sz) - 3 ;
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[0][r+c] = topRow[r-3+c]*((double)1/16) 		/* leftUpCorn */
												 + myArray[0][r-3+c]*((double)2/16)		/* left */
												 + myArray[0+1][r-3+c]*((double)1/16)	/* leftDownCorn */
												 + topRow[r+c]*((double)2/16)			/* up */
												 + myArray[0][r+c]*((double)4/16)		/* itself */
												 + myArray[0+1][r+c]*((double)2/16)		/* down */
												 + rightUpCorn[c]*((double)1/16)		/* rightUpCorn */
												 + rightCol[0+c]*((double)2/16)			/* right */
												 + rightCol[3+c]*((double)1/16);		/* rightDownCorn*/
						}
						myNeighbors.rightUp.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Apply filter on the leftDownCorn */
			if( (myNeighbors.bottom.recieved == 1) && (myNeighbors.left.recieved == 1) && (myNeighbors.leftDown.recieved == 0) ){

					MPI_Test(&myNeighbors.leftDown.recieveRequest,&flag,&status);
					if( flag ){
						//j=0
						int r = rowsNumber/sqrt_comm_sz - 1 ;
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[r][c] = leftCol[3*r-3+c]*((double)1/16) 	/* leftUpCorn */
											   + leftCol[3*r+c]*((double)2/16)		/* left */
											   + leftDownCorn[c]*((double)1/16)		/* leftDownCorn */
											   + myArray[r-1][c]*((double)2/16)		/* up */
											   + myArray[r][c]*((double)4/16)		/* itself */
											   + bottomRow[0+c]*((double)2/16)		/* down */
											   + myArray[r-1][c+3]*((double)1/16)	/* rightUpCorn */
											   + myArray[r][c+3]*((double)2/16)		/* right */
											   + bottomRow[3+c]*((double)1/16);		/* rightDownCorn*/
						}
						myNeighbors.leftDown.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Apply filter on the rightDownCorn */
			if( (myNeighbors.bottom.recieved == 1) && (myNeighbors.right.recieved == 1) && (myNeighbors.rightDown.recieved == 0) ){

					MPI_Test(&myNeighbors.rightDown.recieveRequest,&flag,&status);
					if( flag ){
						int r = rowsNumber/sqrt_comm_sz - 1 ;
						int s = 3*(colsNumber/sqrt_comm_sz) - 3 ;
						for(int c=0; c<=2; c++ ){ /* For 3 colors j,j+1,j+2 */
							myFinalArray[r][s+c] = myArray[r-1][s-3+c]*((double)1/16) 	/* leftUpCorn */
												 + myArray[r][s-3+c]*((double)2/16)		/* left */
												 + bottomRow[s-3+c]*((double)1/16)		/* leftDownCorn */
												 + myArray[r-1][s+c]*((double)2/16)		/* up */
												 + myArray[r][s+c]*((double)4/16)		/* itself */
												 + bottomRow[s+c]*((double)2/16)		/* down */
												 + rightCol[3*r-3+c]*((double)1/16)		/* rightUpCorn */
												 + rightCol[3*r+c]*((double)2/16)		/* right */
												 + rightDownCorn[c]*((double)1/16);		/* rightDownCorn*/
						}
						myNeighbors.rightDown.recieved = 1;
						counterItems ++;
						flag = 0 ;
					}

			}

			/* Wait my Isend */
			MPI_Wait(&myNeighbors.top.sendRequest,&status);
			MPI_Wait(&myNeighbors.leftUp.sendRequest,&status);
			MPI_Wait(&myNeighbors.left.sendRequest,&status);
			MPI_Wait(&myNeighbors.bottom.sendRequest,&status);
			MPI_Wait(&myNeighbors.leftDown.sendRequest,&status);
			MPI_Wait(&myNeighbors.rightUp.sendRequest,&status);
			MPI_Wait(&myNeighbors.right.sendRequest,&status);
			MPI_Wait(&myNeighbors.rightDown.sendRequest,&status);

		}
		/* Swap arrays */
		temp = myArray;
		myArray = myFinalArray;
		myFinalArray = temp;

		/* Check if there are changes */
		int x = 0 ;
		int stop = 0;
		localChanges = 0;
		while( ( stop!=1 ) && ( x < rowsNumber/sqrt_comm_sz ) ){

			for( int j=0; j<3*(colsNumber/sqrt_comm_sz); j++ ){
				if( myArray[x][j] != myFinalArray[x][j] ){ /* There is change */
					stop = 1;
					localChanges = 1;
					break;
				}
			}
			x++;
		}
		MPI_Allreduce(&localChanges,&globalChanges,1,MPI_CHAR,MPI_LOR,MPI_COMM_WORLD);

		if( !globalChanges ){
			/* There are no changes */
			printf("THERE ARE NO CHANGES WOW for i %d \n",i );
		}
		else{
			/* There are changes somewhere */
		}
		
		MPI_Barrier(MPI_COMM_WORLD);

	}

	endTime = MPI_Wtime();
	printf("%d.\t%f seconds\n", my_rank, endTime-startTime);

	MPI_Gatherv(*myArray,(rowsNumber/sqrt_comm_sz)*3*(colsNumber/sqrt_comm_sz),MPI_UNSIGNED_CHAR,*final2D,counts,displs,resizedtype,0,MPI_COMM_WORLD );

	if( my_rank == 0 ){

		FILE *outputFile = fopen(outputFileName, "w");

		for( i=0; i<rowsNumber; i++ ){
			for( j=0; j<3*colsNumber; j++ ){
				//printf("%c", final2D[i][j]);
				fputc(final2D[i][j], outputFile);
			}
		}
		fclose(outputFile);
	}

	free(topRow);
	free(bottomRow);
	free(leftCol);
	free(rightCol);

	free(leftUpCorn);
	free(rightUpCorn);
	free(rightDownCorn);
	free(leftDownCorn);

	free(*array2D);
	free(array2D);
	free(*final2D);
	free(final2D);

	free(*myArray);
	free(myArray);
	free(*myFinalArray);
	free(myFinalArray);

	free(counts);
	free(displs);

	MPI_Type_free(&type);
	MPI_Type_free(&resizedtype);
	MPI_Type_free(&row);
	MPI_Type_free(&column);

	/* Shut down MPI */
	MPI_Finalize();
	return 0;
}
