#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <mpi.h>

// Definição das Tags de Comunicação conforme o Relatório do Grupo 1
#define TAG_WORK 1    // Mestre envia trabalho (tabuleiro parcial)
#define TAG_REQ  2    // Escravo pede trabalho
#define TAG_SOL  3    // Escravo envia solução encontrada
#define TAG_END  4    // Mestre ordena paragem/término

int n, L;

// Protótipos das funções de validação e busca
int isValidPlacement(int *b, int row, int col, int num);
int solveSudokuSerial(int *b, int row, int col);
int workerSolveHybrid(int *local_board, int *solved_board);
void generateTasks(int *initial_board, int row, int col, int current_depth, int target_depth, int **task_pool, int *task_count);

int main(int argc, char *argv[]) {
    int provided;
    // Inicializa o ambiente MPI com suporte a múltiplas threads (necessário para Híbrido)
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Variável para controlo do tempo de execução real
    double exec_time;

    // --- FASE DO MESTRE (RANK 0) ---
    if (rank == 0) {
        if (argc != 2) {
            fprintf(stderr, "Erro: Formato %s <ficheiro.txt>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        FILE *file = fopen(argv[1], "r");
        if (!file) {
            fprintf(stderr, "Erro ao abrir o ficheiro.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }

        if (fscanf(file, "%d", &L) != 1) {
            fprintf(stderr, "Erro ao ler L.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
        n = L * L;

        // Alocação do tabuleiro principal como array 1D (Memória Contígua)
        int *main_board = (int *)malloc(n * n * sizeof(int));
        for (int i = 0; i < n * n; i++) {
            if (fscanf(file, "%d", &main_board[i]) != 1) {
                fprintf(stderr, "Erro ao ler dados do tabuleiro.\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
                return 1;
            }
        }
        fclose(file);

        // Envia os metadados (L e n) para todos os escravos via Broadcast
        MPI_Bcast(&L, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

        // Geração do Pool de Tarefas (Aglomeração baseada na árvore de decisão)
        int max_tasks = 20000; // Capacidade máxima do pool de tarefas temporárias
        int **task_pool = (int **)malloc(max_tasks * sizeof(int *));
        for (int i = 0; i < max_tasks; i++) {
            task_pool[i] = (int *)malloc(n * n * sizeof(int));
        }
        int task_count = 0;

        // Define profundidade da sub-árvore baseada no tamanho do problema
        int target_depth = (n <= 4) ? 1 : 3; 
        generateTasks(main_board, 0, 0, 0, target_depth, task_pool, &task_count);

        // Buffer para receber soluções ou requisições
        int *recv_buffer = (int *)malloc(n * n * sizeof(int));
        int *final_solution = NULL;
        int solution_found = 0;

        // Sincronização e Início da Medição de Tempo Exclusiva do Algoritmo
        MPI_Barrier(MPI_COMM_WORLD);
        exec_time = -omp_get_wtime();

        int tasks_sent = 0;
        int active_workers = size - 1;

        // Distribuição Inicial de Trabalho (Mapeamento Dinâmico)
        for (int worker = 1; worker < size; worker++) {
            if (tasks_sent < task_count) {
                MPI_Send(task_pool[tasks_sent], n * n, MPI_INT, worker, TAG_WORK, MPI_COMM_WORLD);
                tasks_sent++;
            } else {
                MPI_Send(NULL, 0, MPI_INT, worker, TAG_END, MPI_COMM_WORLD);
                active_workers--;
            }
        }

        // Loop de Escalonamento Dinâmico (Task Pool / Mestre-Escravo)
        while (active_workers > 0) {
            MPI_Status status;
            // Aguarda qualquer mensagem vinda de qualquer escravo
            MPI_Recv(recv_buffer, n * n, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int src_worker = status.MPI_SOURCE;

            if (status.MPI_TAG == TAG_SOL) {
                // Solução encontrada por um escravo! Salvaguarda o resultado
                solution_found = 1;
                final_solution = (int *)malloc(n * n * sizeof(int));
                memcpy(final_solution, recv_buffer, n * n * sizeof(int));

                // Sinaliza imediatamente o término precoce (Broadcast de Término) para todos
                for (int worker = 1; worker < size; worker++) {
                    if (worker != src_worker) {
                        // Envia tag de finalização para interromper escravos ocupados
                        MPI_Send(NULL, 0, MPI_INT, worker, TAG_END, MPI_COMM_WORLD);
                    }
                }
                active_workers = 0; // Quebra o ciclo de escalonamento
                break;
            } 
            else if (status.MPI_TAG == TAG_REQ) {
                // Escravo solicita mais trabalho
                if (tasks_sent < task_count && !solution_found) {
                    MPI_Send(task_pool[tasks_sent], n * n, MPI_INT, src_worker, TAG_WORK, MPI_COMM_WORLD);
                    tasks_sent++;
                } else {
                    // Sem mais tarefas disponíveis, envia sinal de paragem
                    MPI_Send(NULL, 0, MPI_INT, src_worker, TAG_END, MPI_COMM_WORLD);
                    active_workers--;
                }
            }
        }

        // Finalização da contagem de tempo real do algoritmo
        exec_time += omp_get_wtime();
        fprintf(stderr, "%.1fs\n", exec_time);

        // Saída de Dados estrita para o STDOUT conforme especificação
        if (solution_found && final_solution != NULL) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    printf("%d ", final_solution[i * n + j]);
                }
                printf("\n");
            }
            free(final_solution);
        } else {
            printf("Nenhuma solução\n");
        }

        // Libertação da memória do Mestre
        for (int i = 0; i < max_tasks; i++) free(task_pool[i]);
        free(task_pool);
        free(main_board);
        free(recv_buffer);

    } 
    // --- FASE DOS ESCRAVOS (RANK > 0) ---
    else {
        // Recebe os metadados globais dimensionais
        MPI_Bcast(&L, 1, MPI_INT, 0, MPI_COMM_WORLD);
        n = L * L;

        int *local_board = (int *)malloc(n * n * sizeof(int));
        int *solved_board = (int *)malloc(n * n * sizeof(int));

        // Sincroniza início com o mestre
        MPI_Barrier(MPI_COMM_WORLD);

        while (1) {
            MPI_Status status;
            // Escravos bloqueiam à espera de uma mensagem do mestre (Trabalho ou Fim)
            MPI_Recv(local_board, n * n, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

            if (status.MPI_TAG == TAG_END) {
                break; // Finaliza o processo escravo de forma limpa
            }

            // Executa a computação usando a abordagem híbrida OpenMP interna
            int success = workerSolveHybrid(local_board, solved_board);

            if (success) {
                // Envia a solução imediatamente para o Mestre
                MPI_Send(solved_board, n * n, MPI_INT, 0, TAG_SOL, MPI_COMM_WORLD);
            } else {
                // Notifica que concluiu o ramo sem sucesso e solicita nova tarefa
                MPI_Send(NULL, 0, MPI_INT, 0, TAG_REQ, MPI_COMM_WORLD);
            }
        }

        free(local_board);
        free(solved_board);
    }

    MPI_Finalize();
    return 0;
}

// Verifica se um número pode ser colocado numa célula específica (Adaptado para 1D)
int isValidPlacement(int *b, int row, int col, int num) {
    for (int i = 0; i < n; i++) {
        if (b[row * n + i] == num || b[i * n + col] == num) return 0;
    }

    int startRow = (row / L) * L;
    int startCol = (col / L) * L;
    for (int i = 0; i < L; i++) {
        for (int j = 0; j < L; j++) {
            if (b[(startRow + i) * n + (startCol + j)] == num) return 0;
        }
    }
    return 1;
}

// Algoritmo clássico sequencial de Backtracking (Operando no array plano 1D)
int solveSudokuSerial(int *b, int row, int col) {
    if (col == n) {
        row++;
        col = 0;
    }
    if (row == n) return 1;
    if (b[row * n + col] != 0) return solveSudokuSerial(b, row, col + 1);

    for (int num = 1; num <= n; num++) {
        if (isValidPlacement(b, row, col, num)) {
            b[row * n + col] = num;
            if (solveSudokuSerial(b, row, col + 1)) return 1;
            b[row * n + col] = 0; // Backtrack
        }
    }
    return 0;
}

// Geração de tarefas por árvore de decisão parcial executada pelo Mestre (Aglomeração)
void generateTasks(int *initial_board, int row, int col, int current_depth, int target_depth, int **task_pool, int *task_count) {
    if (col == n) {
        row++;
        col = 0;
    }
    if (row == n || current_depth == target_depth) {
        memcpy(task_pool[*task_count], initial_board, n * n * sizeof(int));
        (*task_count)++;
        return;
    }
    if (initial_board[row * n + col] != 0) {
        generateTasks(initial_board, row, col + 1, current_depth, target_depth, task_pool, task_count);
        return;
    }

    for (int num = 1; num <= n; num++) {
        if (isValidPlacement(initial_board, row, col, num)) {
            initial_board[row * n + col] = num;
            generateTasks(initial_board, row, col + 1, current_depth + 1, target_depth, task_pool, task_count);
            initial_board[row * n + col] = 0; 
        }
    }
}

// --- EXPLORAÇÃO LOCAL HÍBRIDA POR PARTE DO ESCRAVO (OpenMP) ---
int workerSolveHybrid(int *local_board, int *solved_board) {
    int target_row = -1, target_col = -1;

    // Encontra a primeira célula vazia enviada pelo Mestre neste sub-tabuleiro
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (local_board[i * n + j] == 0) {
                target_row = i;
                target_col = j;
                break;
            }
        }
        if (target_row != -1) break;
    }

    // Se o tabuleiro já veio preenchido, valida e retorna
    if (target_row == -1) {
        memcpy(solved_board, local_board, n * n * sizeof(int));
        return 1;
    }

    int found = 0;

    // Paralelização do laço de alternativas usando OpenMP no nível do nó
    // O balanceamento dinâmico lida com a imprevisibilidade de caminhos complexos
    #pragma omp parallel for shared(found, solved_board) schedule(dynamic)
    for (int num = 1; num <= n; num++) {
        // Se outra thread local ou processo remoto encontrou a solução, aborta cedo
        if (found) continue;

        if (isValidPlacement(local_board, target_row, target_col, num)) {
            // Cada thread precisa da sua cópia privada do tabuleiro para o seu próprio Backtracking
            int *thread_board = (int *)malloc(n * n * sizeof(int));
            memcpy(thread_board, local_board, n * n * sizeof(int));
            thread_board[target_row * n + target_col] = num;

            // Explora recursivamente de forma serial a sub-árvore local atribuída
            if (solveSudokuSerial(thread_board, target_row, target_col + 1)) {
                #pragma omp critical
                {
                    if (!found) {
                        found = 1;
                        memcpy(solved_board, thread_board, n * n * sizeof(int));
                    }
                }
            }
            free(thread_board);
        }
    }
    return found;
}