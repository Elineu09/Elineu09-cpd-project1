#include <stdio.h>
#include <stdlib.h>

int n, L;
int **board;

// Protótipos
void printBoard();
int isValidPlacement(int row, int col, int num);
int solveSudoku(int row, int col); // Agora recebe a posição atual

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Erro: Formato %s <ficheiro.txt>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) return 1;

    fscanf(file, "%d", &L);
    n = L * L;

    board = (int **)malloc(n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        board[i] = (int *)malloc(n * sizeof(int));
        for (int j = 0; j < n; j++) {
            fscanf(file, "%d", &board[i][j]);
        }
    }
    fclose(file);

    if (solveSudoku(0, 0)) { // Começa na posição 0,0
        printBoard();
    } else {
        printf("Nenhuma Solução\n");
    }

    // Nota: Em C profissional, deveríamos dar free(board) aqui.
    return 0;
}

int isValidPlacement(int row, int col, int num) {
    for (int i = 0; i < n; i++) {
        if (board[row][i] == num || board[i][col] == num) return 0;
    }

    int startRow = (row / L) * L;
    int startCol = (col / L) * L;
    for (int i = 0; i < L; i++) {
        for (int j = 0; j < L; j++) {
            if (board[startRow + i][startCol + j] == num) return 0;
        }
    }
    return 1;
}

int solveSudoku(int row, int col) {
    // Avançar para a próxima célula vazia
    if (col == n) {
        row++;
        col = 0;
    }
    if (row == n) return 1; // Fim do tabuleiro
    if (board[row][col] != 0) return solveSudoku(row, col + 1);

    for (int num = 1; num <= n; num++) {
        if (isValidPlacement(row, col, num)) {
            board[row][col] = num;
            if (solveSudoku(row, col + 1)) return 1;
            board[row][col] = 0; // Backtrack
        }
    }
    return 0;
}

void printBoard() {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) printf("%d ", board[i][j]);
        printf("\n");
    }
}