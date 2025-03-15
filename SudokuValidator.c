#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

  const Sudoku sudoku = {
      .rowLength = ROW_LENGTH,
      .length = ROW_LENGTH * ROW_LENGTH,
      .data = sudoku_data,
  };
}

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

bool validate_row(Sudoku *sudoku, size_t rowIdx) {
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

bool validate_submatrix(Sudoku *sudoku, size_t topRowIdx, size_t topColIdx) {
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
