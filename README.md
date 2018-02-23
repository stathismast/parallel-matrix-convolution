.# Parallel

-----------------------------------------------------------------------------------------------------------
P: 10/02/2018
# printRows.c
>Φτιάχνει έναν 2D array στην process 0 και μετα κάνει send κάθε γραμμή στις υπόλοιπες processes
>Στο τέλος όλες οι processes εκτυπώνουν τις γραμμές τους
!!!! ΠΡΟΣΟΧΗ: πρέπει να το τρέξεις με παράμετρο 5 processes,ώστε το comm_size να είναι 5, γιατί είναι "καρφωτό" αλλιώς θα έχει πρόβλημα

------------------------------------------------------------------------------------------------------------
S: 11/02/2018
# filterGray.c
>Φιλτράρει το gray raw αρχείο 10 φορές.
>Πρέπει να βρίσκεται στον ίδιο φάκελο το waterfall_grey_1920_2520.raw

------------------------------------------------------------------------------------------------------------
P: 11/02/2018
# printColumns.c
>Φτιάχνει έναν ΣΤΑΤΙΚΟ 2D array στην process 0 και μετα κάνει send κάθε στήλη στις υπόλοιπες processes
>Στο τέλος όλες οι processes εκτυπώνουν τις στηλες τους
!!!! ΠΡΟΣΟΧΗ: 1)πρέπει να το τρέξεις με παράμετρο 5 processes,ώστε το comm_size να είναι 5, γιατί είναι "καρφωτό" αλλιώς θα έχει πρόβλημα
	      2)Λειτουργει μονο για στατικο 2D array ωστε να ειναι συνεχομενες οι θεσεις μνημης

#split2Darray.c
>Γεμίζει έναν ΣΤΑΤΙΚΟ 2D array στην process 0 και μετά μέσω της Scatterv στέλνει έναν subarray σε κάθε process
>ΟΛΕΣ οι τιμές των πινάκων έχουν μπει "καρφωτά" , για τον global πίνακα 6χ6
!!!! ΠΡΟΣΟΧΗ: Πρέπει το πρόγραμμα να τρέξει με παράμετρο 4 processes.

------------------------------------------------------------------------------------------------------------
P: 13/02/2018
# 16x16_Split_Send.c
>Φτιάχνει έναν ΣΤΑΤΙΚΟ 2D 16x16 array στην process 0.
>Με Scatterv μοιράζει subarrays στα processes.
>Τα processes ανταλλάσουν rows,corners,cols μεταξύ τους και στο τέλος τις εκτυπώνουν.
!!!! ΠΡΟΣΟΧΗ: Πρέπει το πρόγραμμα να τρέξει με παράμετρο 4 processes.

------------------------------------------------------------------------------------------------------------

P: 14/02/2018
# general_Split_Send.c
>Γενικευμένη μορφή του 16x16_Split_Send.c

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o general_Split_Send general_Split_Send.c -lm ; mpiexec -n 4 general_Split_Send

------------------------------------------------------------------------------------------------------------

P: 15/02/2018
# finalOnTheWay.c
>Οδεύουμε προς το τελικό.

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalOnTheWay finalOnTheWay.c -lm ; mpiexec -n 4 finalOnTheWay

------------------------------------------------------------------------------------------------------------

S: 15/02/2018
# finalOnTheWay.c
>Το finalOnTheWay.c πρέπει να βρίκεται στον ίδιο φάκελο με το waterfall_grey_1920_2520.raw για να λειτουργήσει σωστά. Έχει προστεθεί η συνάρτηση applyFilter. Πλέον το συγκεκριμένο πρόγραμμα διαβάζει την εικόνα την μοιρ΄άζει στα processes τα οποία εφαρμόζουν το φίλτρο δέκα φορές ΜΟΝΟ στα pixel που είναι εσωτερικά.

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalOnTheWay finalOnTheWay.c -lm ; mpiexec -n 4 finalOnTheWay
                    mpicc -o finalOnTheWay finalOnTheWay.c -lm ; mpiexec -n 16 finalOnTheWay
------------------------------------------------------------------------------------------------------------

P: 16/02/2018
# finalOnTheWay.c
>Άλλαξα λίγο την συνάρτηση και κάνω το swap στο τέλος διότι:
1) Έτσι όπως ήταν η συνάρτηση χανόντουσαν οι ακριανές τιμές ( δηλαδή topRow,bottomRow κλπ ) οπότε θα έπρεπε να τις κάνω copy στο filtered array όπως το είχες
2) Στα if για το filter στα οριακά πάλι θα χρειαζόμουν κάτι σαν temp για να κρατάω την αρχική κατάσταση των top/bottom row και λοιπά

Οπότε έτσι λύθηκαν τα παραπάνω προβλήματα.Δηλαδή ο original ( κάθε διεργασίας ) θα παραμένει original μέχρι το τέλος, και στο τέλος του for θα γίνεται swap με τον τελικό filtered. ( Αυτό μάλλον εννοούσε και ο Κοτρώνης στην διάλεξη στο δήλο που είπε να κάνουμε malloc , ώστε στο τέλος να αλλάζουμε απλά δείκτες ).

>Εφαρμόζουμε φίλτρο ξεχωριστά στο εσωτερικό των γραμμών/στηλών και ξεχωριστά στις γωνίες, γιατί μπορεί να έχουμε πάρει κάποια γραμμή/στήλη αλλά όχι γωνία ( όταν κάνουμε Irecieve )

>Έκανα για το εσωτερικό της topRow ( και για την περιπτωση που εχει topRow,αρα recieved == 1 ( δηλαδη δεν ειναι topRow του πινακα ) και για την περιπτωση που δεν εχει topRow αρα recieved == 2 )και δουλευει μια χαρά.

>Μένουν να συμπληρωθούν τα υπόλοιπα if για τις άλλες περιπτώσεις με παρόμοια λογική.

ΠΡΟΣΟΧΗ ΜΗΝ ΧΡΗΣΙΜΟΠΟΙΗΣΕΙΣ ΤΗΝ ΜΕΤΑΒΛΗΤΗ i ΤΟΥ ΕΞΩΤΕΡΙΚΟΥ for!!!

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalOnTheWay finalOnTheWay.c -lm ; mpiexec -n 4 finalOnTheWay
                    mpicc -o finalOnTheWay finalOnTheWay.c -lm ; mpiexec -n 16 finalOnTheWay
------------------------------------------------------------------------------------------------------------

P: 18/02/2018
# finalGrey.c
>Σχεδόν το τελικό πρόγραμμα για εφαρμογή φίλτρου στην grey εικόνα με χρήση MPI.
>Μένουν:1)Περίπτωση για 1 process
	2)Reduce και έλεγχος αν είναι το ίδιο το τελευταίο με το προτελευταίο

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalGrey finalGrey.c -lm ; mpiexec -n 4 finalGrey
                    mpicc -o finalGrey finalGrey.c -lm ; mpiexec -n 16 finalGrey
------------------------------------------------------------------------------------------------------------

S: 18/02/2018
# finalColor1.c
>Uses 3 Scatter calls for each color channel individually. Doesn't use irecv yet. Will be updated in the near future.

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalColor1 finalColor1.c -lm ; mpiexec -n 4 finalColor1
                    mpicc -o finalColor1 finalColor1.c -lm ; mpiexec -n 16 finalColor1
------------------------------------------------------------------------------------------------------------

P: 19/02/2018
# finalColor_1_Scatter.c
>Uses 1 Scatter call.Each "cell" has 3 chars ( one for each color ) so each time we apply the filter on something, we apply it 3 times
(one for each color, that means one for j( first color ) , one for j+1 ( second color ) and one for j+2 ( third color ).

TO DO GENERAL:1)Free arrays we malloced and free_types
	      2)What we should do when we have only 1 process.
	      3)Reduce and check if last picture is same with pre-last picture.
	      4)Measure times etc
	      5)OpenMP + MPI
	      6)Readme

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalColor_1_Scatter finalColor_1_Scatter.c -lm ; mpiexec -n 4 finalColor_1_Scatter
                    mpicc -o finalColor_1_Scatter finalColor_1_Scatter.c -lm ; mpiexec -n 16 finalColor_1_Scatter
------------------------------------------------------------------------------------------------------------
S: 19/02/2018
# finalColor1.c
>It now utilizes irecv.

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalColor1 finalColor1.c -lm ; mpiexec -n 4 finalColor1
                    mpicc -o finalColor1 finalColor1.c -lm ; mpiexec -n 16 finalColor1
------------------------------------------------------------------------------------------------------------

P: 20/02/2018
# finalGrey_Null.c AND finalColor_Null.c
>Uses MPI_PROC_NULL.

TO DO GENERAL:1)Free arrays we malloced and free_types
	      2)What we should do when we have only 1 process.
	      3)Reduce and check if last picture is same with pre-last picture.
	      4)Measure times etc
	      5)OpenMP + MPI
	      6)Readme
	      7)Barrier
	      8)Parallel I/O
	      9)Cartesian topology

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalGrey_Null finalGrey_Null.c -lm ; mpiexec -n 4 finalGrey_Null
                    mpicc -o finalGrey_Null finalGrey_Null.c -lm ; mpiexec -n 16 finalGrey_Null

		    mpicc -o finalColor_Null finalColor_Null.c -lm ; mpiexec -n 4 finalColor_Null
                    mpicc -o finalColor_Null finalColor_Null.c -lm ; mpiexec -n 16 finalColor_Null

---------------------------------------------------------------------------------------------------------
P: 21/02/2018
# finalGrey_Null_Reduce.c AND finalColor_Null_Reduce.c
>Uses MPI_Reduce to check for change.

>It does 2000 loops as it is, and print message that there are no changes from loop 1932 till loop 1999.

TO DO GENERAL:1)Free arrays we malloced and free_types
	      2)What we should do when we have only 1 process.
	OK->> 3)Reduce and check if last picture is same with pre-last picture.
	      4)Measure times etc
	      5)OpenMP + MPI
	      6)Readme
	      7)Barrier
	      8)Parallel I/O
	      9)Cartesian topology

ΟΔΗΓΙΕΣ ΤΡΕΞΙΜΑΤΟΣ: mpicc -o finalGrey_Null_Reduce finalGrey_Null_Reduce.c -lm ; mpiexec -n 4 finalGrey_Null_Reduce
                    mpicc -o finalGrey_Null_Reduce finalGrey_Null_Reduce.c -lm ; mpiexec -n 16 finalGrey_Null_Reduce

		    mpicc -o finalColor_Null_Reduce finalColor_Null_Reduce.c -lm ; mpiexec -n 4 finalColor_Null_Reduce
                    mpicc -o finalColor_Null_Reduce finalColor_Null_Reduce.c -lm ; mpiexec -n 16 finalColor_Null_Reduce
-----------------------------------------------------------------------------------------------------------
4 Final Versions

finalColor_Null.c
finalColor_Null_Reduce.c
finalGrey_Null.c
finalGrey_Null_Reduce.c

> 2 for grey ( 1 with reduce and 1 without )
> 2 for color ( 1 with reduce and 1 without )

TO DO:

		1)Parallel I/O
		2)Cartesian Topology
		3)Start - Init
		4)ApplyFilter not to be function and inline get2DArray/getRGBArray
>>>>> DONE 	5)Maybe we take as arguments fileName,pictureSize ( rows and cols ) so as to be easier when we measure times
		6)OpenMP
		7)Readme


How to run:
mpicc -o finalColor_Null finalColor_Null.c -lm ; mpiexec -n <N> finalColor_Null	[-i inputFileName] [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications]

mpicc -o finalColor_Null_Reduce finalColor_Null_Reduce.c -lm ; mpiexec -n <N> finalColor_Null_Reduce [-i inputFileName] [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications]

mpicc -o finalGrey_Null finalGrey_Null.c -lm ; mpiexec -n <N> finalGrey_Null [-i inputFileName] [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications]

mpicc -o finalGrey_Null_Reduce finalGrey_Null_Reduce.c -lm ; mpiexec -n <N> finalGrey_Null_Reduce [-i inputFileName] [-o outputFileName] [-s rowsNumber colsNumber] [-f filterApplications]

-------------------------------------------------------------------------------------------------------------------

S: 23/02/2018
# testing folder
>Folder containing 4 subfolders for testing purposes. One folder for the original version of color.c and gray.c (no cartesian, no sendInit, no reduce), one for cartesian, one for reduce and one for sendInit. Folder 'timings' is an archive of previous comparative timings.
------------------------------------------------------------------------------------------------------------

P: 23/02/2018
# PARADOSI folder
>TO DO:
1)Correcting makefiles
2)OpenMP
3)Timings
4)Cartesian README + memory leak
5)Inline/Define get2Darray/getRGBarray
------------------------------------------------------------------------------------------------------------
