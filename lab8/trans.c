/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * Student Name: Junjie Fang
 * Student ID: 516030910006
 *
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int row, col, i, j;
    int t0,t1,t2,t3,t4,t5,t6,t7;
    if (M == 32 && N == 32) {
        // split into 8*8 blocks
        for (col = 0; col < 4; col++) {
            for (row = 0; row < 4; row++) {
                for(i = 0; i < 8; i++) { 
                    /*  at first it is: (loop j 0-7) 
                          B[8 * col + j][8 * row + i] = A[8 * row + i][8 * col + j]
                        which has 343 misses more than this(287)
                        but why?
                        has not save 8 value of A at a time?
                    */
                    t0 = A[8 * row + i][8 * col + 0];
                    t1 = A[8 * row + i][8 * col + 1];
                    t2 = A[8 * row + i][8 * col + 2];
                    t3 = A[8 * row + i][8 * col + 3];
                    t4 = A[8 * row + i][8 * col + 4];
                    t5 = A[8 * row + i][8 * col + 5];
                    t6 = A[8 * row + i][8 * col + 6];
                    t7 = A[8 * row + i][8 * col + 7];

                    B[8 * col + 0][8 * row + i] = t0;
                    B[8 * col + 1][8 * row + i] = t1;
                    B[8 * col + 2][8 * row + i] = t2;
                    B[8 * col + 3][8 * row + i] = t3;
                    B[8 * col + 4][8 * row + i] = t4;
                    B[8 * col + 5][8 * row + i] = t5;
                    B[8 * col + 6][8 * row + i] = t6;
                    B[8 * col + 7][8 * row + i] = t7;
                }
            }
        }
    }

    else if (M == 64 && N == 64) {

        // split into 64 8*8 blocks
        for (row = 0; row < 8; row++) {
            for (col = 0; col < 8; col++) {
                // deal each block
                for (i = 0; i < 4; i++){
                    // deal up 4 line 
                    t0 = A[8 * row + i][8 * col + 0];
                    t1 = A[8 * row + i][8 * col + 1];
                    t2 = A[8 * row + i][8 * col + 2];
                    t3 = A[8 * row + i][8 * col + 3];
                    t4 = A[8 * row + i][8 * col + 4];
                    t5 = A[8 * row + i][8 * col + 5];
                    t6 = A[8 * row + i][8 * col + 6];
                    t7 = A[8 * row + i][8 * col + 7];
                    // store A up left to B up left 
                    B[8 * col + 0][8 * row + i] = t0;
                    B[8 * col + 1][8 * row + i] = t1;
                    B[8 * col + 2][8 * row + i] = t2;
                    B[8 * col + 3][8 * row + i] = t3;
                    // store A up right to B up right temporarily (by col) 
                    B[8 * col + 0][8 * row + i + 4] = t4;
                    B[8 * col + 1][8 * row + i + 4] = t5;
                    B[8 * col + 2][8 * row + i + 4] = t6;
                    B[8 * col + 3][8 * row + i + 4] = t7;
                }

                for (i = 0; i < 4; i++){
                    // deal down 4 line
                    // fetch temporary B up right value of A (by row)
                    t0 = B[8 * col + i][8 * row + 4];
                    t1 = B[8 * col + i][8 * row + 5];
                    t2 = B[8 * col + i][8 * row + 6];
                    t3 = B[8 * col + i][8 * row + 7];
                    // store A down left to B up right
                    t4 = A[8 * row + 4][8 * col + i];
                    t5 = A[8 * row + 5][8 * col + i];
                    t6 = A[8 * row + 6][8 * col + i];
                    t7 = A[8 * row + 7][8 * col + i];

                    B[8 * col + i][8 * row + 4] = t4;
                    B[8 * col + i][8 * row + 5] = t5;
                    B[8 * col + i][8 * row + 6] = t6;
                    B[8 * col + i][8 * row + 7] = t7;
                    // get A down right and store together with fetched value
                    t4 = A[8 * row + 4][8 * col + i + 4];
                    t5 = A[8 * row + 5][8 * col + i + 4];
                    t6 = A[8 * row + 6][8 * col + i + 4];
                    t7 = A[8 * row + 7][8 * col + i + 4];

                    B[8 * col + i + 4][8 * row + 0] = t0;
                    B[8 * col + i + 4][8 * row + 1] = t1;
                    B[8 * col + i + 4][8 * row + 2] = t2;
                    B[8 * col + i + 4][8 * row + 3] = t3;
                    B[8 * col + i + 4][8 * row + 4] = t4;
                    B[8 * col + i + 4][8 * row + 5] = t5;
                    B[8 * col + i + 4][8 * row + 6] = t6;
                    B[8 * col + i + 4][8 * row + 7] = t7;
                }     
            }
        }

    }
    else if (M == 61 && N == 67) { // A:67*61, B:61*67 
        // split A into 7 1*8 slivers, loop 67 time, deal left 5 col alone  
        for (col = 0; col < 7; col++){
            for (row = 0; row < 67; row++){
                for (j = 0; j < 8; j++){
                    B[8 * col + j][row] = A[row][8 * col + j];
                }
            }
        }

        for (row = 0; row < 67; row++){
            for (col = 55; col < 61; col++){
                B[col][row] = A[row][col];
            }
        }
    }
    else {
        for (i = 0; i < N; i++) {
            for (j = 0; j < M; j++) {
                B[j][i] = A[i][j];
            }
        }    
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

