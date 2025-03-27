#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h> 
#include <sys/time.h> 
#include <signal.h>
#include <errno.h>

#define ROWS 6
#define COLS 7
#define N 4

#define BUFFER_SIZE 100

#define TIME_LIMIT 50000

pid_t target_pid = -1;
long micro_seconds;

typedef enum {
    EMPTY = 0,
    PLAYER_X = 1,
    PLAYER_Y = 2
} Cell;

typedef enum {
    ARENA_SUCCESS_X_WIN = 1,
    ARENA_SUCCESS_Y_WIN = 2,
    ARENA_SUCCESS_DRAW = 3,

    ARENA_FAIL_RUN_X = -1,
    ARENA_FAIL_INVALID_OUTPUT_X = -2,
    ARENA_FAIL_UPDATE_BOARD_X = -3,

    ARENA_FAIL_RUN_Y = -4,
    ARENA_FAIL_INVALID_OUTPUT_Y = -5,
    ARENA_FAIL_UPDATE_BOARD_Y = -6
} ArenaResult;

typedef enum {
    AGENT_SUCCESS = 0,
    AGENT_TIMEOUT = -12,
    AGENT_RUNTIME_ERROR = -13,
    AGENT_ENDED_BY_UNKNOWN_SIGNAL = -14,
    AGENT_STOPPED = -15,
    AGENT_CONTINIED = -16,
    AGENT_DIED_UNEXPECTEDLY = -17
} AgentResult;

typedef enum {
    PROCESS_ERROR_SET_SIGNAL_TIMER = -21,
    PROCESS_ERROR_WRITE_TO_BUFFER = -22,
    PROCESS_ERROR_READ_FROM_BUFFER = -23
} ProcessError;

void
setAgentPath (char** agent_a, char** agent_a_path, 
                char** agent_b, char** agent_b_path)
{
    *agent_a = "agent_a";
    *agent_a_path = "my_agent.c";
    *agent_b = "agent_b";
    *agent_b_path = "my_agent_2.c";
}

void
printBoard (int** board)
{
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++) {
            printf("%2d ", board[i][j]);
        }
        printf("\n");
    }
}

int
isHorizontalConnected (int** board, int player)
{
    for (int row = ROWS - 1; row >= 0; row--) {
        for (int col = 0; col <= COLS - N; col++) {
            if (player == board[row][col] && 
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
isVerticalConnected (int** board, int player)
{
    for (int col = 0; col < COLS; col++) {
        for (int row = ROWS - 1; row > ROWS - N; row--) {
            if (player == board[row][col] && 
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
isRightDiagonalConnected (int** board, int player)
{
    for (int row = ROWS - N; row >= 0; row--) {
        for (int col = 0; col <= COLS - N; col++) {
            if (player == board[row][col] && 
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
isLeftDiagonalConnected (int** board, int player)
{
    for (int row = ROWS - N; row >= 0; row--) {
        for (int col = COLS - 1; col >= COLS - N; col--) {
            if (player == board[row][col] && 
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
isFourStonesConnected (int** board, int player)
{
    if (isHorizontalConnected(board, player) ||
        isVerticalConnected(board, player) || 
        isRightDiagonalConnected(board, player) ||
        isLeftDiagonalConnected(board, player)) {
        return true;
    }
    return false;
}

/* */

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

/* */

int
isMakePipeSuccess (int fd[2])
{
    if (pipe(fd) == -1) {
        return false;
    }
    return true;
}

int
isForkSuccess (pid_t pid)
{
    if (pid == -1) {
        return false;
    }
    return true;
}

/* */

void
compileAgentChildProcess (int fd[2], char* agent_path, char* agent_exe)
{
    close(fd[0]);
    dup2(fd[1], STDERR_FILENO); 
    close(fd[1]);

    execl("/usr/bin/gcc", "gcc", "-o", agent_exe, agent_path, 0x0);

    perror("Execl Failed");
    exit(EXIT_FAILURE);
}

void
compileAgentParentProcess (int fd[2], char* output_buffer, size_t buffer_size)
{
    close(fd[1]); 
    wait(0);

    ssize_t count = read(fd[0], output_buffer, buffer_size - 1);
    close(fd[0]);

    if (count > 0) {
        output_buffer[count] = '\0';
        printf("Compile Error\n");
    } else {
        output_buffer[0] = '\0';
    }
}

int
makeCompileAgentFork (int fd[2], char* agent_path, char* agent_exe, 
                        char* output_buffer, size_t buffer_size)
{
    pid_t compile_pid = fork();

    if (!isForkSuccess(compile_pid)) {
        perror("Fork Failed");
        return -1;
    }

    if (compile_pid == 0) {
        compileAgentChildProcess(fd, agent_path, agent_exe);
    }
    else {
        compileAgentParentProcess(fd, output_buffer, buffer_size);
    }

    return 0;
}

int 
compileAgent (char* agent_path, char* agent_exe) 
{
    int fd[2];
    char output_buffer[BUFFER_SIZE];

    if (!isMakePipeSuccess(fd)) {
        return -1;
    }

    if (makeCompileAgentFork(fd, agent_path, agent_exe, output_buffer, BUFFER_SIZE) == -1) {
        return -1;
    }

    if (output_buffer[0] != '\0') {
        return -1;
    }

    return 0;
}

/* */
void 
handleAlarm (int sig) 
{
    if (!(kill(target_pid, 0))) {
        kill(target_pid, SIGKILL);
    }
}

void 
setSigAction () 
{
    struct sigaction sa;
    sa.sa_handler = handleAlarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Sigaction Error");
        exit(EXIT_FAILURE);
    }
}

void 
setItimer (long time_limit) 
{
    struct itimerval timer; 

    timer.it_value.tv_sec = time_limit / 1000000;
    timer.it_value.tv_usec = time_limit % 1000000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("Setitimer Error");
        exit(EXIT_FAILURE);
    }
}

int 
getChildStatus (int status) 
{
    if (WIFEXITED(status)) {
        return AGENT_SUCCESS;
    } 
    else if (WIFSIGNALED(status)) { 
        if (WTERMSIG(status) == 9) {
            return AGENT_TIMEOUT;
        } 
        else if (WTERMSIG(status) == 8 || WTERMSIG(status) == 11) {
            return AGENT_RUNTIME_ERROR;
        }
        else {
            return AGENT_ENDED_BY_UNKNOWN_SIGNAL;
        }
    } 
    else if (WIFSTOPPED(status)) {
        return AGENT_STOPPED;
    } 
    else if (WIFCONTINUED(status)) {
        return AGENT_CONTINIED;
    } 
    else {
        return AGENT_DIED_UNEXPECTEDLY;
    }
}

/* */

void
runAgentChildProcess (int fd[2], int fd2[2], char* agent, char* program_name)
{
    close(fd[1]);
    dup2(fd[0], STDIN_FILENO);
    close(fd[0]); 

    close(fd2[0]);
    dup2(fd2[1], STDOUT_FILENO);
    close(fd2[1]);

    execl(program_name, agent, 0x0);
        
    perror("Execl failed");
    exit(EXIT_FAILURE);
}

int
tryPutDataToInputBuffer (int* input_buffer, int** board, int player)
{
    input_buffer[0] = player;

    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < COLS; ++j) {
            input_buffer[1 + i * COLS + j] = board[i][j];
        }
    }

    return true;
}

int
isWriteInputSuccess (int fd[1], int* input_buffer, int** board, int player)
{
    if (!(tryPutDataToInputBuffer(input_buffer, board, player))) {
        return false;
    }

    size_t total_bytes = sizeof(int) * (1 + ROWS * COLS);
    size_t num_write, total = 0;
    char* ptr = (char*)input_buffer;

    while (total < total_bytes) {
        num_write = write(fd[1], ptr + total, total_bytes - total);
        if (num_write == -1) {
            perror("Write Failed");
            return false;
        }
        total += num_write;
    }

    return true;
}

int
isReadOutputSuccess (int fd2[2], char* output_buffer)
{
    ssize_t num_read, total = 0;

    while ((num_read = read(fd2[0], output_buffer + total, BUFFER_SIZE - 1 - total)) > 0 || 
    (num_read == -1 && errno == EINTR)) {
        if (num_read == -1 && errno == EINTR) {
            continue;  // 시그널로 인터럽트된 경우 다시 시도
        }

        total += num_read;
        if (total >= BUFFER_SIZE - 1)
            break;
    }

    if (num_read == -1) {
        perror("Read Failed");
        return false;
    }

    output_buffer[total] = '\0';

    return true;
}

int
setSignalAndTimer (int agent_pid)
{
    target_pid = agent_pid;
    setSigAction();
    setItimer(5000000);
    return true;
}

int
runAgentParentProcess (int** board, int agent_pid, int fd[2], int fd2[2], 
                        int* input_buffer, char* output_buffer, int player)
{
    if (!(setSignalAndTimer(agent_pid))) {
        return PROCESS_ERROR_SET_SIGNAL_TIMER;
    }

    close(fd[0]);
    close(fd2[1]);

    if (!(isWriteInputSuccess(fd, input_buffer, board, player))) {
        return PROCESS_ERROR_WRITE_TO_BUFFER;
    }
    close(fd[1]);

    if (!(isReadOutputSuccess(fd2, output_buffer))) {
        return PROCESS_ERROR_READ_FROM_BUFFER;
    }
    close(fd2[0]);

    int status;
    waitpid(agent_pid, &status, 0);

    return getChildStatus(status);
}

int
makeRunAgentFork (int** board, int fd[2], int fd2[2], char* agent_exe, 
                int* input_buffer, char* output_buffer, int player)
{
    char *program_name;
    asprintf(&program_name, "./%s", agent_exe);

    pid_t agent_pid = fork();

    if (!isForkSuccess(agent_pid)) {
        perror("Fork Failed");
        return -1;
    }

    if (agent_pid == 0) {
        runAgentChildProcess(fd, fd2, agent_exe, program_name);
    }
    else {
        return runAgentParentProcess(board, agent_pid, fd, fd2, input_buffer, output_buffer, player);
    }

    return 0;
}

int
runAgent (int** board, char* agent, int* input_buffer, char* output_buffer, int player)
{
    int fd[2];
    int fd2[2];

    if (!isMakePipeSuccess(fd) || !isMakePipeSuccess(fd2)) {
        return -1;
    }

    int result = makeRunAgentFork(board, fd, fd2, agent, input_buffer, output_buffer, player);

    return result; 
}

int 
getColumn (char column)
{
    switch (tolower(column)) {
        case 'a':
            return 0;
        case 'b':
            return 1;
        case 'c':
            return 2;
        case 'd':
            return 3;
        case 'e':
            return 4;
        case 'f':
            return 5;
        case 'g':
            return 6;
        default:
            return -1;
    }
}

int
tryUpdateBoard (int col, int player, int** board)
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
updateResult (int** board, char* agent, int player, int* input_buffer, char* output_buffer)
{
    int run_agent_result = runAgent(board, agent, input_buffer, output_buffer, player);
    if (run_agent_result < 0) {
        return run_agent_result;
    }
    // if (runAgent(board, agent, input_buffer, output_buffer, player) == -1) {
    //     return (player == PLAYER_X) ? ARENA_FAIL_RUN_X : ARENA_FAIL_RUN_Y;
    // }

    int column = getColumn(output_buffer[0]);
    printf("Selected column: %s", output_buffer);

    if (column < 0 || column > 6) {
        return (player == PLAYER_X) ? ARENA_FAIL_INVALID_OUTPUT_X : ARENA_FAIL_INVALID_OUTPUT_Y;
    }

    if (!tryUpdateBoard(column, player, board)) {
        return (player == PLAYER_X) ? ARENA_FAIL_UPDATE_BOARD_X : ARENA_FAIL_UPDATE_BOARD_Y;
    }

    printBoard(board);

    if (isFourStonesConnected(board, player)) {
        return (player == PLAYER_X) ? ARENA_SUCCESS_X_WIN : ARENA_SUCCESS_Y_WIN;
    }

    return 0;
}

void
setRunningTime (struct timeval start_time, struct timeval end_time)
{
    micro_seconds += (end_time.tv_sec - start_time.tv_sec) * 1000000L + (end_time.tv_usec - start_time.tv_usec);
}

int
tryArenaAndGetResult (int** board, char* agent_a, char* agent_b)
{
    char output_buffer[BUFFER_SIZE];
    int input_buffer[BUFFER_SIZE];
    int result;
    int turns = 0;
    int maximum_turns = ROWS * COLS;

    while (turns < maximum_turns) {
        printf("------------------------\n");

        printf("Turn: Agent %d\n", PLAYER_X);
        result = updateResult(board, agent_a, PLAYER_X, input_buffer, output_buffer);
        if (result != 0) return result;
        turns++;

        if (turns == ROWS * COLS) break;

        printf("------------------------\n");

        printf("Turn: Agent %d\n", PLAYER_Y);
        result = updateResult(board, agent_b, PLAYER_Y, input_buffer, output_buffer);
        if (result != 0) return result;
        turns++;
    }

    return ARENA_SUCCESS_DRAW;
}

int 
main (int argc, char *argv[])
{
    struct timeval start_time, end_time;

    int** board = createEmptyBoard(ROWS, COLS);
    if (board == NULL) {
        goto err_board;
    }

    initEmptyBoard(ROWS, COLS, board);

    char* agent_a;
    char* agent_a_path;
    char* agent_b;
    char* agent_b_path;
    setAgentPath(&agent_a, &agent_a_path, &agent_b, &agent_b_path);

    if (compileAgent(agent_a_path, agent_a) == -1 ||
        compileAgent(agent_b_path, agent_b)) {
        goto err_compile;
    }

    gettimeofday(&start_time, NULL);

    int result = tryArenaAndGetResult(board, agent_a, agent_b);

    gettimeofday(&end_time, NULL);
    setRunningTime (start_time, end_time);

    printf("------------------------\n");
    switch (result) {
        case ARENA_SUCCESS_X_WIN:
            printf("WIN: Agent 1\n");
            break;
        case ARENA_SUCCESS_Y_WIN:
            printf("WIN: Agent 2\n");
            break;
        case ARENA_SUCCESS_DRAW:
            printf("DRAW\n");
            break;
        case ARENA_FAIL_RUN_X:
            goto err_run_x;
        case ARENA_FAIL_INVALID_OUTPUT_X:
            goto err_invalid_x;
        case ARENA_FAIL_UPDATE_BOARD_X:
            goto err_failed_x;
        case ARENA_FAIL_RUN_Y:
            goto err_run_y;
        case ARENA_FAIL_INVALID_OUTPUT_Y:
            goto err_invalid_y;
        case ARENA_FAIL_UPDATE_BOARD_Y:
            goto err_failed_y;
        case AGENT_TIMEOUT:
            goto err_timeout;
        case AGENT_RUNTIME_ERROR:
            goto err_rumtime_error;
        case AGENT_ENDED_BY_UNKNOWN_SIGNAL:
            goto err_unknown_signal;
        case AGENT_DIED_UNEXPECTEDLY:
            goto err_unexpected_exit;
        case PROCESS_ERROR_SET_SIGNAL_TIMER:
            goto err_process_set_signal_timer;
        case PROCESS_ERROR_WRITE_TO_BUFFER:
            goto err_write_buffer;
        case PROCESS_ERROR_READ_FROM_BUFFER:
            goto err_read_buffer;
        default:
            goto err;
    }

    printf("Running Time: %ld (ms)\n", micro_seconds);
    exit(EXIT_SUCCESS);


success:
    exit(EXIT_SUCCESS);
err:
    fprintf(stderr, "ERROR :: Unknown Error.\n");
    exit(EXIT_FAILURE);
err_board:
    fprintf(stderr, "ERROR :: Memory allocation failed.\n");
    exit(EXIT_FAILURE);
err_compile:
    fprintf(stderr, "ERROR :: Compile Agent Error.\n");
    exit(EXIT_FAILURE);
err_run_x:
    fprintf(stderr, "ERROR :: Agent 1 Run Failed.\n");
    exit(EXIT_FAILURE);
err_invalid_x:
    fprintf(stderr, "ERROR :: Agent 1 Invalid Output.\n");
    exit(EXIT_FAILURE);
err_failed_x:
    fprintf(stderr, "ERROR :: Update board Failed After Agent 1.\n");
    exit(EXIT_FAILURE);
err_run_y:
    fprintf(stderr, "ERROR :: Agent 2 Run Failed.\n");
    exit(EXIT_FAILURE);
err_invalid_y:
    fprintf(stderr, "ERROR :: Agent 2 Invalid Output.\n");
    exit(EXIT_FAILURE);
err_failed_y:
    fprintf(stderr, "ERROR :: Update board Failed After Agent 2.\n");
    exit(EXIT_FAILURE);
err_timeout:
    fprintf(stderr, "ERROR :: TIMEOUT\n");
    exit(EXIT_FAILURE);
err_rumtime_error:
    fprintf(stderr, "ERROR :: RUN TIME ERROR\n");
    exit(EXIT_FAILURE);
err_unknown_signal:
    fprintf(stderr, "ERROR :: Agent Exit By Unknown Signal.\n");
    exit(EXIT_FAILURE);
err_unexpected_exit:
    fprintf(stderr, "ERROR :: Agent Exit Unexpectedly.\n");
    exit(EXIT_FAILURE);
err_process_set_signal_timer:
    fprintf(stderr, "ERROR :: Set Signal & Timer Failed.\n");
    exit(EXIT_FAILURE);
err_write_buffer:
    fprintf(stderr, "ERROR :: Write To Pipe Failed.\n");
    exit(EXIT_FAILURE);
err_read_buffer:
    fprintf(stderr, "ERROR :: Read From Pipe Failed.\n");
    exit(EXIT_FAILURE);
}