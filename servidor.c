#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define MAX_BUF 1024
#define Max_transfs 19

//Linked List---------------------------------------------------
typedef struct node{
    int pid; //Pid do exec manager.
    char string[150]; //Pedido do cliente.
    struct node *prox;
} *TLIST;

//Adiciona pedidos á lista.-----------------------------------------------------
void newNode(TLIST *head, int pid, char *string)
{
    TLIST new_node = malloc(sizeof(struct node));
    new_node->pid = pid;
    strcpy(new_node->string, string);
    new_node->prox = NULL;
    if((*head) == NULL){
        (*head) = new_node;
        return;
    }
    TLIST l = (*head);
    while(l->prox != NULL){
        l = l->prox;
    }
    l->prox = new_node;
}
//Remove pedidos da lista.-----------------------------------------------------
void deleteNode(TLIST* head_ref, int pid)
{
    TLIST temp = *head_ref, ant;
 
    // Verifica se o primeiro tem o pid da task que queremos remover.
    if (temp != NULL && temp->pid == pid) {
        *head_ref = temp->prox;
        free(temp);
        return;
    }
 
    // Procura pela task que queremos remover.
    while (temp != NULL && temp->pid != pid) {
        ant = temp;
        temp = temp->prox;
    }
 
    // Não encontrou a task que queremos remover.
    if (temp == NULL)
        return;
 
    // Encontrou a task e remove-a.
    ant->prox = temp->prox;
    free(temp);
}

//Variável global que muda quando é invocado o SIGTERM.---------------------------
int podeReceber = 1;

//Inicializa lista de tasks.----------------------------------------------------
TLIST listaTasks = NULL;

//Sighandler.------------------------------------------------------------
void sigtermHandler(int signum){
    podeReceber = 0;
    write(1, "O servidor nao ira aceitar mais pedidos.\n", 41);
    if(listaTasks == NULL)
        exit(0);

}
//Main-------------------------------------------------------------------------------

//argv[0] - ./execs/servidor
//argv[1] - Ficheiro config com os maximos 
//argv[2] - Pasta com os execs das transformações

int main(int argc, char * argv[]){
    //Sigterm
    if(signal(SIGTERM, sigtermHandler) == SIG_ERR){
        perror("SIGTERM failed: ");
    }

    // Mensagem com o formato suposto.
    if(argc == 1){
        write(1, "./servidor config-file trans-folder\n", 42);
    }//Verifica se a chamada é válida.
    else if(argc != 3 ){
        write(1, "Chamada invalida.\n", 18);
    }
    else{
        //Variaveis partilhadas entre pedidos de proc-file e de status.-----------------
        int *transusados[7];
        //Uso de mmaps para poupar memória pois estas variaveis vão ser usadas por diferentes processos.
        for(int i = 0; i < 7; i++){
            transusados[i] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
            *transusados[i] = 0;
        }
        int *numTask = mmap(NULL, sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        *numTask = 0;

        //Ler o ficheiro dos máximos (config).--------------------------------
        int ficheiro = open(argv[1], O_RDONLY);
        if(ficheiro < 0){
            write(1, "Ficheiro config invalido.\n", 26);
            close(ficheiro);
            return 1;
        }
        //Processar o ficheiro dos máximos.------------------------------------
        char buf[MAX_BUF];
        read(ficheiro, &buf, MAX_BUF);//Guarda o conteudo no buffer.
        close(ficheiro);
        char *cnfglines[7];
        //Divide por \n.
        char *token;
        token = strtok(buf,"\n");
        for(int i = 0; token != NULL; i++){
            cnfglines[i] = token;
            token = strtok(NULL, "\n");
        }
        //Divide cada linha por espaco,
        char *cnfg[7][2];
        for(int i = 0,k=-1; i < 7; i++){
            token = strtok(cnfglines[i], " ");
            for(int j = 0; token != NULL; j++){
                if(j==0){
                    if(strcmp(token, "nop") == 0)  //cnfg[0] nop
                        k = 0;
                    else if(strcmp(token, "bcompress") == 0) //cnfg[1] bcompress
                        k = 1;
                    else if(strcmp(token, "bdecompress") == 0) //cnfg[2] bdecompress
                        k = 2;
                    else if(strcmp(token, "gcompress") == 0) //cnfg[3] gcompress
                        k = 3;
                    else if(strcmp(token, "gdecompress") == 0) //cnfg[4] gdecompress
                        k = 4;
                    else if(strcmp(token, "encrypt") == 0) //cnfg[5] encrypt
                        k = 5;
                    else if(strcmp(token, "decrypt") == 0) //cnfg[6] decrypt
                        k = 6;
            
                }
                cnfg[k][j] = token;
                token = strtok(NULL, " ");
            }
        }
        //Guarda o numero max de ocorrencias concorrentes de cada transformação.
        int transmax[7];
        for(int i = 0; i < 7; i++){
            transmax[i] = atoi(cnfg[i][1]);
        }

        //Fifo Genérico.---------------------------------------------------------
        //Criação do fifo.
        if( mkfifo("./pipeserver/Genérico",0644) < 0){
            perror("Servidor - Fifo: ");
        }
        //Abre o fifo para ler pedidos dos clientes e de si mesmo.
        int fd = open("./pipeserver/Genérico", O_RDONLY);
        if(fd < 0){
            perror("Servidor - Open fifo: ");
        }
        //Abre o fifo para mandar pedidos para si mesmo(adds e removes de tasks).
        int fd2 = open("./pipeserver/Genérico", O_WRONLY);
        if(fd2 < 0){
            perror("Servidor - Open fifo: ");
        }

        //Rececção de pedidos----------------------------------------------------------------
        char buf2[MAX_BUF];
        while( (podeReceber || listaTasks != NULL ) &&(read(fd, &buf2, MAX_BUF)) > 0 ){
            //Parse do pedido recebido.
            int numtransfs = 0;
            char * args[4 + Max_transfs];
            char * token;
            char * pathFifo;
            token = strtok(buf2, ",");
            pathFifo = token;
            int transneeded[7];
            for(int i = 0; i < 7; i++) transneeded[i] = 0;
            for( int i = 0; i < 4+Max_transfs && token != NULL; i++){
                if(i >= 4){
                    numtransfs++;
                    if(strcmp(token, cnfg[0][0] ) == 0){
                        args[i] = cnfg[0][0];
                        transneeded[0]++;
                    }else if(strcmp(token, cnfg[1][0] ) == 0){
                        args[i] = cnfg[1][0];
                        transneeded[1]++;
                    }else if(strcmp(token, cnfg[2][0] ) == 0){
                        args[i] = cnfg[2][0];
                        transneeded[2]++;
                    }else if(strcmp(token, cnfg[3][0] ) == 0){
                        args[i] = cnfg[3][0];
                        transneeded[3]++;
                    }else if(strcmp(token, cnfg[4][0] ) == 0){
                        args[i] = cnfg[4][0];
                        transneeded[4]++;
                    }else if(strcmp(token, cnfg[5][0] ) == 0){
                        args[i] = cnfg[5][0];
                        transneeded[5]++;
                    }else if(strcmp(token, cnfg[6][0] ) == 0){
                        args[i] = cnfg[6][0];
                        transneeded[6]++;
                    }

                }else{
                    args[i] = token;    //args[0] pid | [1] proc-file | [2] inputfile | [3] outputfile | [4] transformações
                }
                token = strtok(NULL, ",");
            }
            // printf("\n%s\n",pathFifo);
            /* for(int i=0;i<7;i++){
               printf("Transf %s:%d \n",cnfg[i][0],transneeded[i]);}*/
        
            //Reconhecer o pedido.------------------------------------------------

            //Pedido para adicionar uma task.
            if(strcmp(args[1],"add") == 0){
                newNode(&listaTasks, atoi(args[0]), args[2]);
                 // write(1,"add",3);

            }//Pedido para remover uma task.
            else if(strcmp(args[1], "remove") == 0){
                deleteNode(&listaTasks, atoi(args[0]));
                //write(1,"delete",6);
            }
            else{//Pedido proc-file | status.
                //Task manager.----------------------------------------
                if(fork() == 0){
                    //Pedido de proc-file.
                    if(strcmp(args[1], "proc-file") == 0){
                        //Abre o fifo para responder ao cliente.
                        int fdR = open(pathFifo, O_WRONLY);
                        if(fdR < 0){
                            perror("Servidor - Open fifo cliente resp: ");
                        }

                        //Se receber um sigterm não recebe mais pedidos.
                        if(!podeReceber){
                            write(fdR, "Servidor nao ira receber mais pedidos.\n", 39);
                            close(fdR);
                            _exit(0);
                        }

                        //Verifica se o servidor tem capacidade para processar o pedido.
                        if(transneeded[0] > transmax[0] || transneeded[1] > transmax[1] ||
                        transneeded[2] > transmax[2] || transneeded[3] > transmax[3] || 
                        transneeded[4] > transmax[4]|| transneeded[5] > transmax[5]|| transneeded[6] > transmax[6]){
                            write(fdR, "Servidor nao tem recursos suficientes para processar este pedido.\n", 67);
                            close(fdR);
                            _exit(0);
                        }

                        pid_t pidmon;

                        //Exec manager.-----------------------------------------------------
                        if((pidmon = fork()) == 0){
                            //Verifica se tem transformações suficientes disponiveis.
                            if(transneeded[0] > (transmax[0] - *transusados[0]) || transneeded[1] > (transmax[1] - *transusados[1]) ||
                            transneeded[2] > (transmax[2] - *transusados[2]) || transneeded[3] > (transmax[3] - *transusados[3]) ||
                            transneeded[4] > (transmax[4] - *transusados[4])|| transneeded[5] > (transmax[5] - *transusados[5])
                            || transneeded[6] > (transmax[6] - *transusados[6])){
                                //Envia mensagem de Pending para o Cliente.
                                write(fdR, "Pending...\n", 11);
                            while(transneeded[0] > (transmax[0] - *transusados[0]) || transneeded[1] > (transmax[1] - *transusados[1]) ||
                            transneeded[2] > (transmax[2] - *transusados[2]) || transneeded[3] > (transmax[3] - *transusados[3]) ||
                            transneeded[4] > (transmax[4] - *transusados[4])
                            || transneeded[5] > (transmax[5] - *transusados[5])|| transneeded[6] > (transmax[6] - *transusados[6]))
                                sleep(1);
                            
                            }

                            //Adiciona as transformações que vai gastar no total.
                            for(int i = 0; i < 7; i++){
                                *transusados[i] += transneeded[i];
                            }

                            //Diz ao cliente que comecou a processar o pedido dele.
                            write(fdR, "Processing...\n", 14);
                            close(fdR);
                            int fdIn,fdOut;

                            //Abre o Input e redireciona para o stdin.
                            if( (fdIn = open(args[2], O_RDONLY)) == -1){
                                perror("Abrir Input: "); return -1;
                            }
                            dup2(fdIn,0); close(fdIn);
                            
                            //Abre o output e redireciona para o stdout.
                            if( (fdOut = open(args[3], O_TRUNC | O_WRONLY | O_CREAT, 0644)) == -1){
                                perror("Abrir output: "); return -1;
                            };
                            dup2(fdOut,1); close(fdOut);

                            //Aplicação das transformações.--------------------------------------------------------
                            int p[2];
                            int i;
                            for(i = 0; i < (numtransfs-1); i++){
                                pipe(p);
                                if(fork() == 0){
                                    dup2(p[1], 1); close(p[0]); close(p[1]);
                                    //Prepara path da transformação.
                                    char transf[40] = "";
                                    strcat(transf, argv[2]); strcat(transf,"/"); strcat(transf, args[i+4]);
                                    //Executa a transformação.
                                    if(execl(transf, args[i+4], NULL) < 0){
                                        perror("Erro transf: ");
                                        _exit(5);
                                    }
                                }
                                else{
                                    dup2(p[0], 0); close(p[0]); close(p[1]);
                                }
                            }
                            //Prepara a path da ultima transformação (stdout -> outputfile)
                            char transf[40] = "";
                            strcat(transf, argv[2]); strcat(transf,"/"); strcat(transf, args[i+4]);
                            //Executa a transformação.
                            if(execl(transf, args[i+4], NULL) < 0){
                                perror("Erro transf: ");
                                _exit(5);
                            }
                        }//Fim do Exec manager.------------------------------------------------
                        else{ // Codigo do Task manager.--------------------

                            char novaTask[190] = "";
                            char temp[150] = "";
                            //Prepara a mensagem do pedido.
                            for(int i = 2; i < (4 + numtransfs) ; i++){
                                strcat(temp, " ");
                                strcat(temp, args[i]);
                            }

                            //Prepara o pedido de add para enviar para si mesmo.
                            sprintf(novaTask, "%d,add,Task #%d: proc-file%s%c",pidmon, *numTask, temp,'\0');
                            *numTask += 1;

                            //Pede a si mesmo para adicionar uma task.
                            write(fd2, &novaTask, strlen(novaTask)+1);

                            int status;
                            //Espera pelo Exec manager.
                            waitpid(pidmon, &status, 0);
                            
                            //Depois de fazer as transformações todas liberta as que usou.
                            for(int i = 0; i < 7; i++){
                                *transusados[i] -= transneeded[i];
                            }
                            
                            //Prepara a mensagem de Concluded para enviar ao cliente.
                            char mensagem[50];
                            char bufer[512];
                            int fdSa,fdOt;
                            fdSa = open(args[2], O_RDONLY);
                            int bytes_input=read(fdSa,&bufer,sizeof(bufer));
                            close(fdSa);
                            fdOt = open(args[3], O_RDONLY);
                            int bytes_output=read(fdOt,&bufer,sizeof(bufer));
                            close(fdOt);
                            sprintf(mensagem,"Concluded (bytes-input: %d, bytes-output: %d)\n %c",bytes_input,bytes_output,'\0');
                            write(fdR, mensagem, sizeof(mensagem));
                            close(fdR);
                            
                            //Prepara o pedido de remove para enviar a si mesmo.
                            sprintf(novaTask, "%d,remove%c",pidmon,'\0');

                            //Pede a si mesmo para remover uma task.
                            write(fd2, novaTask, strlen(novaTask)+1);
                            _exit(0);
                        }
                    }//Fim do proc-file.

                     //Pedido de status.
                    else if(strcmp(args[1], "status") == 0){

                        //Abre fifo para responder ao cliente.
                        int fdR = open(pathFifo, O_WRONLY);
                        if(fdR < 0){
                            perror("Servidor - Open fifo cliente resp: ");
                        }

                        //Envia ao cliente lista de tasks a correr e por correr.
                        char status[MAX_BUF] = "";
                        while (listaTasks != NULL) {
                            sprintf(status, "%s\n%c", listaTasks->string, '\0');
                            write(fdR, status, strlen(status)+1);
                            listaTasks = listaTasks->prox;
                        }
                        //Envia ao cliente o estado das transformações.
                        for(int i = 0; i < 7; i++){
                            sprintf(status, "transf %s: %d/%d (running/max)\n%c",cnfg[i][0], *transusados[i], transmax[i], '\0');
                            write(fdR, status, strlen(status)+1);
                        }
                        _exit(0);
                    }
                }//Fim do Task manager.
            }
        }
        close(fd);
        close(fd2);
    }
    return 0;
}