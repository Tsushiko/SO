#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUF 512

//argv[0] - ./execs/cliente
//argv[1] - proc-file| status
//argv[2] - input file
//argv[3] - out file
//argv[x] - transformações, o x varia entre 4 e 21(19|Max_transf + 3|(proc-file etc...))

int main(int argc, char * argv[]){
    //Abertura do pipe para enviar os pedidos.
    int fdServidor = open("./pipeserver/Genérico",O_WRONLY);
    if(fdServidor < 0){
        printf("Servidor nao esta operacional.\n");
    
    } else { //Existe pipe genérico e um servidor disponível.
        if (argc ==1){ // Mensagem com o formato suposto.
            printf("./execs/cliente proc-file inputfile outputfile (transformações) ...\n./execs/cliente status\n");

        }//Pedido proc-file.
        else if( strcmp(argv[1], "proc-file") == 0 && argc > 4){ 
            //Verifica se o ficheiro de input existe.
            int fdIn = open(argv[2], O_RDONLY);
            if(fdIn < 0){
                printf("Ficheiro de input nao existe ou nao foi possivel abrir.\n");
                return 1;
            }
            close(fdIn);
            
            //Verifica se ficheiro output ja existe.
            int fdOut = open(argv[3], O_RDONLY);
            if(fdOut > 0){
                printf("O Ficheiro \"%s\" já existe.\n",argv[3]);
                return 1;
            }
            close(fdOut);
            //Verifica se as transformações são validas.
            for(int i = 4; i < argc; i++){
                if( strcmp(argv[i], "nop" ) != 0 &&
                    strcmp(argv[i], "bdecompress" ) != 0 &&
                    strcmp(argv[i], "bcompress" ) != 0 &&
                    strcmp(argv[i], "gcompress")  != 0 &&
                    strcmp(argv[i], "gdecompress")  != 0 &&
                    strcmp(argv[i], "encrypt") != 0 &&
                    strcmp(argv[i], "decrypt")  != 0 )
                {
                    printf("Chamada inválida, filtro: %s não é válido\n", argv[i]);
                    return 1;
                }
            }
            
            //Cria fifo para receber dados do servidor.
            char path[17] = "";
            pid_t r=getpid();
            sprintf(path, "./pipeclient/%d", r);
            //printf("%s\n",path);
            if( mkfifo(path, 0644) < 0){
                perror("Cliente - Criar/Abrir fifo: ");
                return 1;
            }

            //Prepara o pedido.
            char pedido[100] = "";
            strcat(pedido, path); // path do fifo
            strcat(pedido, ",");
            strcat(pedido, argv[1]); // proc-file
            strcat(pedido, ",");
            strcat(pedido, argv[2]); // input file
            strcat(pedido, ","); 
            strcat(pedido, argv[3]); // output file
            for(int i = 4; i < argc; i++){
                strcat(pedido, ",");
                strcat(pedido, argv[i]);
            }
            strcat(pedido, "\0");
            // printf("%s\n",pedido );

            //Envia o pedido para o servidor.
            write(fdServidor, &pedido, strlen(pedido)+1);

            //Abre fifo para receber dados.
            int fdR = open(path, O_RDONLY);
            if( fdR < 0){
                perror("Cliente - Criar/Abrir fifo: ");
            }
            
            //Lê do fifo.
            int bytes_read;
            char buf[MAX_BUF];
            while( (bytes_read = read(fdR, &buf, MAX_BUF)) > 0 ){
                write(1, &buf, bytes_read);
            }
        }//Pedido status.
       else if( strcmp(argv[1], "status") == 0){

            //Cria fifo para receber dados do servidor.
            char path[15] = "";
            sprintf(path, "./pipeclient/%d", getpid());
            if( mkfifo(path, 0644) < 0){
                perror("Cliente - Criar/Abrir fifo: ");
                return 1;
            }
            //Prepara o pedido.
            char pedido[25] = "";
            strcat(pedido, path); // path do fifo
            strcat(pedido, ",status"); // status / argv[1]
            strcat(pedido, "\0");

            //Envia o pedido para o servidor.
            write(fdServidor, &pedido, strlen(pedido)+1);
            
            //Abre fifo para receber dados.
            int fdR = open(path, O_RDONLY);
            if( fdR < 0){
                perror("Cliente - Criar/Abrir fifo: ");
            }

            //Lê do fifo.
            int bytes_read;
            char buf[MAX_BUF];
            while( (bytes_read = read(fdR, &buf, MAX_BUF)) > 0 ){
                write(1, &buf, bytes_read);
            }
            close(fdR);
        }else{
            printf("Chamada Invalida.\n");
        }
    }
    return 0;
}