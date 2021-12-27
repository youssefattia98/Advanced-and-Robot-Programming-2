/*
compile with: gcc prod.c -o prod -lrt -pthread
run with: ./prod 104857600 102400 3 4 5
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <strings.h>
#include <signal.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <ctype.h>

#define SHMOBJ_PATH "/shm_AOS"
#define SEM_PATH_1 "/sem_AOS_1"
#define SEM_PATH_2 "/sem_AOS_2"
#define SEM_PATH_3 "/sem_AOS_3"

void Randomdata(unsigned char* data){
  FILE *Randomdatafd;
  Randomdatafd = fopen("./array","r");

  if ( Randomdatafd ){
    while(fscanf(Randomdatafd, "%s", data)!=EOF);
  }
  else{
    printf("Failed to open the file\n");
  }
  fclose(Randomdatafd);
}

void error(char *msg, int fd){
    perror(msg);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    exit(0);
}


int Asize, CDsize, choice =0;
int main(int argc, char * argv[]){
  Asize = atoi(argv[1]);
  CDsize = atoi(argv[2]);
  choice = atoi(argv[3]);
  unsigned char *A= (unsigned char*) malloc(Asize*sizeof(unsigned char)); //array of size 0 to 100 mb according to user input
  unsigned char *CD= (unsigned char*) malloc(CDsize*sizeof(unsigned char)); //array of size 0 to 10kb according to user input
  Randomdata(A); //read random data into array A

  if(choice == 1){
    //unnamed pipes
    int pfd[2];
    pfd[0] = atoi(argv[4]);
    pfd[1] = atoi(argv[5]);
    close(pfd[0]); //close the read side
    int bytes = write(pfd[1],A,strlen(A)); //write to pipe
  }

  else if (choice == 2){
    //named pipes
    char * namedpipe = "/tmp/namedpipe"; 
    mkfifo(namedpipe, 0666); //create namepipe
    int fd = open(namedpipe, O_WRONLY); //open fifo as write only
    write(fd, A, strlen(A)); //write in fifo
  }


  else if (choice == 3){
    //sockets
    int sockfd, newsockfd, portno, clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
      error("ERROR opening socket",sockfd);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 4000;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
      error("ERROR on binding",sockfd);
    }
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0){
      error("ERROR on accept",newsockfd);
    }
    bzero(buffer,256);
    n = read(newsockfd,buffer,255);
    if (n < 0){
      error("ERROR reading from socket",newsockfd);
    }
    n = write(newsockfd,A,strlen(A));
    if (n < 0){
      error("ERROR writing to socket",newsockfd);
    }
    shutdown(newsockfd, SHUT_RDWR);
    close(newsockfd); 
  }


  else if(choice == 4){
    //shared memory
    char * ptr;
    int shared_seg_size = CDsize; //int having size of the shared memoery 
    int shmfd = shm_open(SHMOBJ_PATH, O_CREAT | O_RDWR, 0666); //open and create the shared memory as read and write
    ftruncate(shmfd, shared_seg_size); //sets the size of the memory 
    ptr = mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0); //mapping of the memory segment referenced by the file descriptor returned by shm_open()
    //creates other two semaphores
    sem_t * sem_id1= sem_open(SEM_PATH_1, O_CREAT | O_RDWR, 0666, 1); 
    sem_t * sem_id2= sem_open(SEM_PATH_2, O_CREAT | O_RDWR, 0666, 1);
    sem_t* mutexCircBuffer= sem_open(SEM_PATH_3, O_CREAT | O_RDWR, 0666,1);
    //intlaizes these two sempahores
    sem_init(sem_id1, 1, 1); // initialized to 1
    sem_init(sem_id2, 1, 0); // initialized to 0
  
    CD=A;
    for(int i=0;i<Asize;i=i+shared_seg_size){
      sem_wait(sem_id1); //wait for a signal from the cons
      sem_wait(mutexCircBuffer);

      memcpy (ptr, CD, shared_seg_size);
      CD= CD+shared_seg_size;
      printf("i am at ittraion: %d\n",(i/shared_seg_size));

      sem_post(mutexCircBuffer);
      sem_post(sem_id2);//send a signal to the consumer
    }

    shm_unlink(SHMOBJ_PATH);
    sem_close(sem_id1);
    sem_close(sem_id2);
    sem_close(mutexCircBuffer);
    sem_unlink(SEM_PATH_1);
    sem_unlink(SEM_PATH_2);
    sem_unlink(SEM_PATH_3);
    munmap(ptr, shared_seg_size);
  }

  else{
    printf("Fatal error in choice\n");
    exit(1);
  }
  return(0);
}