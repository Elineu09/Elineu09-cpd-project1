#include <stdio.h>
#include <stdlib.h>

int n, L; 
int **board; 

void printBoard();
int isValidPlacement(int row, int col, int num);
int solveSudoku();

int main(int argc, char *argv[]){

	if(argc != 2){
		printf("Erro: O programa recebe 2 argumentos!\nFormato: %s <ficheiro.txt>\n",argv[0]);
		return 1;
	}

	FILE * file = fopen(argv[1], "r");
	if (!file){
		printf("Erro ao abrir o ficheiro: %s\n",argv[1]);
		return 1;
	}

	fscanf(file, "%d", &L);

	if (L < 2 || L > 9){
		printf("L de estar entre 2 e 9!\n");
		return 1;
	}

	n = L * L;

	// Fazer alocação dinâmica da matriz 
	board = (int **) malloc(n * sizeof(int *)); 
	for (int i = 0; i < n; i++){
		board[i] = (int *) malloc(n * sizeof(int));
	}

	// Adicionar os elementos na matriz
	for (int i = 0; i < n; i++){
		for (int j = 0; j < n; j++){
			fscanf(file, "%d", &board[i][j]);
		}
	}
	
	// Fechar o ficheiro
	fclose(file);

	// Imprimir a matriz
	if (solveSudoku()){
		printBoard();
	} else {
		printf("Nenhuma Solução");
	}

	return 0;
}

void printBoard(){
	for (int i = 0; i < n; i++){
		for (int j = 0; j < n; j++){
			printf("%d ",board[i][j]);
		}
		printf("\n");
	}
}

int isValidPlacement(int row, int col, int num){
	for(int i = 0; i < n; i++){
		if(board[row][i] == num){
			return 0;
		}
	}

	for (int i = 0; i < n; i++){
		if (board[i][col] == num){
			return 0;
		}
	}

	int startRow = (row / L) * L; 
	int startCol = (col / L) * L;

	for (int i = 0; i < L; i++){
		for (int j = 0; j < L; j++){
			if (board[startRow + i][startCol + j] == num){
				return 0;
			}
		}
	}

	return 1;
}

int solveSudoku(){
	int row = -1;
	int col = -1;
	int found = 0;

	for (int i = 0; i < n && !found; i++){
		for (int j = 0; j < n; j++){
			if (board[i][j] == 0){
				row = i;
				col = j;
				found = 1;
				break;
			}
		}
	}

	if (!found){
		return 1;
	}

	for (int num = 1; num <= n; num++){

		if(isValidPlacement(row, col, num)) {
			board[row][col] = num;

			if (solveSudoku()){
				return 1;
			}

			board[row][col] = 0;
		}
	}

	return 0;
}



