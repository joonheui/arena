#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#define ROWS 6
#define COLS 7
#define N 4

#define MAX_RETRY_COUNT 1000

typedef enum {
    EMPTY = 0,
    PLAYER_X = 1,
    PLAYER_Y = 2
} Cell;

int**
addEmptyRows (int row)
{
    int** row_array = (int**)malloc(row * sizeof(int*));
    if (row_array == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }
    
    return row_array;
}

int**
addEmptyColumns (int** board, int row, int column)
{
    for (int i = 0; i < row; i++) {
        board[i] = (int*)malloc(column * sizeof(int));
        if (board[i] == NULL) {
            perror("Memory allocation failed");
            exit(1);
        }
    }

    return board;
}

int** 
createEmptyBoard (int row, int column) 
{
    int** board = addEmptyRows(row);
    board = addEmptyColumns(board, row, column);

    return board;
}

bool
initEmptyBoard (int row, int column, int** board)
{
    if (board == NULL) 
        return false;

    for (int i = 0; i < row; i++) {
        if (board[i] == NULL) 
            return false; 
        for (int j = 0; j < column; j++) {
            board[i][j] = 0;
        }
    }

    return true;
}

void
setStoneCount (int* total_stone_count, int* x_stone_count, int* y_stone_count)
{
    *total_stone_count = rand() % 42;
    *x_stone_count = (*total_stone_count + 1) / 2;
    *y_stone_count = *total_stone_count - *x_stone_count;
}

int
tryPlaceStone (int col, int player, int** board)
{
    for (int row = ROWS - 1; row >= 0 ; row--) {
        if (board[row][col] == 0) {
            board[row][col] = player;
            return 1;
        }
    }
    return 0;
}

int
setNewBoard (int total_stone_count, int x_stone_count, int y_stone_count, int** board)
{
    int player = PLAYER_X;
    int placed_stone_count = 0;

    while (placed_stone_count <= total_stone_count) {
        int selected_column = rand() % COLS;
        if (tryPlaceStone(selected_column, player, board)) {
            placed_stone_count++;

            if (player == PLAYER_X && x_stone_count > 0) {
                x_stone_count--;
                player = PLAYER_Y;
            }
            else if (player == PLAYER_Y && y_stone_count > 0) {
                y_stone_count--;
                player = PLAYER_X;
            }
        }
        else {
            continue;
        }
    }
    return true;
}

int
isHorizontalConnected (int** board)
{
    for (int row = ROWS - 1; row >= 0; row--) {
        for (int col = 0; col <= COLS - N; col++) {
            if (board[row][col] && 
                board[row][col] == board[row][col + 1] &&
                board[row][col] == board[row][col + 2] && 
                board[row][col] == board[row][col + 3]) {
                return true;
            }
        }
    }
    return false;
}

int
isVerticalConnected (int** board)
{
    for (int col = 0; col < COLS; col++) {
        for (int row = ROWS - 1; row > ROWS - N; row--) {
            if (board[row][col] && 
                board[row][col] == board[row - 1][col] &&
                board[row][col] == board[row - 2][col] && 
                board[row][col] == board[row - 3][col]) {
                return true;
            }
        }
    }
    return false;
}

int
isRightDiagonalConnected (int** board)
{
    for (int row = ROWS - N; row >= 0; row--) {
        for (int col = 0; col <= COLS - N; col++) {
            if (board[row][col] && 
                board[row][col] == board[row + 1][col + 1] &&
                board[row][col] == board[row + 2][col + 2] && 
                board[row][col] == board[row + 3][col + 3]) {
                return true;
            }
        }
    }
    return false;
}

int
isLeftDiagonalConnected (int** board)
{
    for (int row = ROWS - N; row >= 0; row--) {
        for (int col = COLS - 1; col >= COLS - N; col--) {
            if (board[row][col] && 
                board[row][col] == board[row + 1][col - 1] &&
                board[row][col] == board[row + 2][col - 2] && 
                board[row][col] == board[row + 3][col - 3]) {
                return true;
            }
        }
    }
    return false; 
}

int
isFourStonesConnected (int** board)
{
    if (isHorizontalConnected(board) ||
        isVerticalConnected(board) || 
        isRightDiagonalConnected(board) ||
        isLeftDiagonalConnected(board)) {
        return true;
    }
    return false;
}

int
trySetBoard (int total_stone_count, int x_stone_count, int y_stone_count, int** board)
{
    int try_count = 0;

    while (try_count < MAX_RETRY_COUNT) {
        initEmptyBoard(ROWS, COLS, board);

        if (!setNewBoard(total_stone_count, x_stone_count, y_stone_count, board)) {
            continue;
        }

        if (!isFourStonesConnected(board)) {
            return true;
        }

        try_count++;
    }

    return false;
}

int
getNextPlayer (int total_stone_count)
{
    if (total_stone_count %= 2) {
        return PLAYER_X;
    }
    else {
        return PLAYER_Y;
    }
}

void
printNextPlayer (int total_stone_count)
{
    if (getNextPlayer(total_stone_count) == 1) {
        printf("1\n");
    }
    else {
        printf("2\n");
    }
}

void
printBoard (int row, int column, int** board)
{
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < column; j++) {
            printf("%2d ", board[i][j]);
        }
        printf("\n");
    }
}

int 
main () 
{
    int** board = createEmptyBoard(ROWS, COLS);
    if (board == NULL) {
        goto err_1;
    }

    srand(time(NULL));
    int total_stone_count, x_stone_count, y_stone_count;
    setStoneCount(&total_stone_count, &x_stone_count, &y_stone_count);

    if (!trySetBoard(total_stone_count, x_stone_count, y_stone_count, board)) {
        goto err_2;
    }

    printNextPlayer(total_stone_count);
    printBoard(ROWS, COLS, board);

    for (int i = 0; i < ROWS; i++) {
        free(board[i]);
    }
    free(board);

    return EXIT_SUCCESS;

err_1:
    fprintf(stderr, "ERROR :: Memory allocation failed.\n");
    exit(EXIT_FAILURE);
err_2:
    fprintf(stderr, "ERROR :: Try setting board failed.\n");
    exit(EXIT_FAILURE);
}