#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  // The int array containing all the cell values from Sudoku
  uint8_t *data;
  // The length of a single row in the sudoku.
  // Since the sudoku is a square, this is also the length of the columns.
  size_t rowLength;
  // The length of the `data` array.
  // This value should be `rowLength²`.
  size_t length;
} Sudoku;

typedef int bool;
const bool TRUE = 1;
const bool FALSE = 0;

const uint16_t CHECKER = 0b111111111;

size_t idx_fromCordsToIdx(size_t row_length, size_t rowIdx, size_t colIdx) {
  return rowIdx * row_length + colIdx;
}

bool validate_columns(Sudoku *sudoku) {
  for (size_t colIdx = 0; colIdx < sudoku->rowLength; colIdx++) {
    uint16_t marker = 0;
    for (size_t rowIdx = 0; rowIdx < sudoku->rowLength; rowIdx++) {
      size_t idx = idx_fromCordsToIdx(sudoku->rowLength, rowIdx, colIdx);
      uint8_t cell_val = sudoku->data[idx];
      uint16_t marker_col = 1 << cell_val;
      marker |= marker_col;
    }

    if (CHECKER != marker) {
      return FALSE;
    }
  }
  return TRUE;
}

bool validate_rows(Sudoku *sudoku) {
  for (size_t rowIdx = 0; rowIdx < sudoku->rowLength; rowIdx++) {
    uint16_t marker = 0;
    for (size_t colIdx = 0; colIdx < sudoku->rowLength; colIdx++) {
      size_t idx = idx_fromCordsToIdx(sudoku->rowLength, rowIdx, colIdx);
      uint8_t cell_val = sudoku->data[idx];
      uint16_t marker_col = 1 << cell_val;
      marker |= marker_col;
    }

    if (CHECKER != marker) {
      return FALSE;
    }
  }
  return TRUE;
}

bool validate_submatrices(const Sudoku *sudoku) {
  for (size_t start_row_col = 0; start_row_col < sudoku->rowLength;
       start_row_col += 3) {
    uint16_t marker = 0;

    for (size_t colIdx = start_row_col; colIdx < start_row_col + 3; colIdx++) {
      for (size_t rowIdx = start_row_col; rowIdx < start_row_col + 3;
           rowIdx++) {

        size_t idx = idx_fromCordsToIdx(sudoku->rowLength, rowIdx, colIdx);
        uint8_t cell_val = sudoku->data[idx];
        uint16_t marker_col = 1 << cell_val;
        marker |= marker_col;
      }
    }

    if (CHECKER != marker) {
      return FALSE;
    }
  }

  return TRUE;
}

void thread_column_checker(void *arg) {
  Sudoku *data = arg;
  bool columns_are_valid = validate_columns(data);

  pthread_exit(&columns_are_valid);
}

void thread_row_checker(void *arg) {
  Sudoku *data = arg;
  bool rows_are_valid = validate_rows(data);
  pthread_exit(&rows_are_valid);
}

int main(int argc, char **argv) {
  const size_t ROW_LENGTH = 9;
  if (argc != 2) {
    fprintf(stderr, "Invalid number of arguments received! Please provide a "
                    "file with the solution to check.");
    return 1;
  }

  char *file_name = argv[1];
  int fd = open(file_name, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Couldn't open the file for reading data!");
    return 1;
  }

  char file_data[ROW_LENGTH * ROW_LENGTH];
  ssize_t bytes_read = read(fd, file_data, ROW_LENGTH * ROW_LENGTH);
  if (bytes_read == -1) {
    fprintf(stderr, "Failed to read bytes from file `%s`!", file_name);
    return 1;
  }

  if (bytes_read == 0 || bytes_read != ROW_LENGTH * ROW_LENGTH) {
    fprintf(stderr, "The file has an insufficient data! %zu != %zd",
            ROW_LENGTH * ROW_LENGTH, bytes_read);
    return 1;
  }

  uint8_t sudoku_data[ROW_LENGTH * ROW_LENGTH];
  for (size_t i = 0; i < ROW_LENGTH * ROW_LENGTH; i++) {
    char file_byte = file_data[i];

    if (file_byte < '1' || file_byte > '9') {
      fprintf(stderr, "Only numbers between 1-9 are allowed on a sudoku!");
      return 1;
    }

    sudoku_data[i] = (uint8_t)(file_byte - '0');
  }

  Sudoku sudoku = {
      .rowLength = ROW_LENGTH,
      .length = ROW_LENGTH * ROW_LENGTH,
      .data = sudoku_data,
  };

  bool valid_submatrices = validate_submatrices(&sudoku);

  pid_t process_id = getpid();
  int son_pid = fork();
  if (son_pid == -1) {
    fprintf(stderr, "An error has ocurred trying to fork!");
    return 1;
  } else if (son_pid == 0) {
    // Son branch...
    int pid_str_size = (int)(ceil(log10(process_id) + 1.0) * sizeof(char));
    char str_pid[pid_str_size];
    if (sprintf(str_pid, "%d", process_id) < 0) {
      fprintf(stderr,
              "Failed to write the `process_id` into the buffer! pid: %d",
              process_id);
      exit(1);
    }

    if (-1 == execlp("ps", "-p", process_id, "-lLf")) {
      fprintf(stderr, "Failed to execute `ps` command with execlp! pid: %d",
              process_id);
      exit(1);
    }

  } else {
    // Father branch...
    pthread_t thread_id;
    if (0 != pthread_create(&thread_id, NULL, (void *)thread_column_checker,
                            &sudoku)) {
      fprintf(stderr, "Failed to create thread for validating columns!");
      exit(1);
    }

    bool valid_cols;
    if (0 != pthread_join(thread_id, (void *)&valid_cols)) {
      fprintf(stderr, "Failed to join into the validating columns thread!");
      exit(1);
    }

    int current_thread_id = syscall(SYS_gettid);
    if (-1 == current_thread_id) {
      fprintf(stderr, "Failed to obtain the current tid!");
      exit(1);
    }

    wait(&son_pid);

    if (0 !=
        pthread_create(&thread_id, NULL, (void *)thread_row_checker, &sudoku)) {
      fprintf(stderr, "Failed to create thread for validating rows!");
      exit(1);
    }

    bool valid_rows;
    if (0 != pthread_join(thread_id, (void *)&valid_rows)) {
      fprintf(stderr, "Failed to join into the validating columns thread!");
      exit(1);
    }

    bool is_valid = valid_rows && valid_cols && valid_submatrices;
    if (is_valid) {
      printf("The solution is valid!");
    } else {
      printf("The solution is not valid!");
    }

    son_pid = fork();
    if (son_pid == -1) {
      fprintf(stderr, "An error has ocurred trying to fork!");
      return 1;
    } else if (son_pid == 0) {
      // Another son...
      int pid_str_size = (int)(ceil(log10(process_id) + 1.0) * sizeof(char));
      char str_pid[pid_str_size];
      if (sprintf(str_pid, "%d", process_id) < 0) {
        fprintf(stderr,
                "Failed to write the `process_id` into the buffer! pid: %d",
                process_id);
        exit(1);
      }

      if (-1 == execlp("ps", "-p", process_id, "-lLf")) {
        fprintf(stderr, "Failed to execute `ps` command with execlp! pid: %d",
                process_id);
        exit(1);
      }
    } else {
      // Father again...
      wait(&son_pid);
      return 0;
    }
  }
}
