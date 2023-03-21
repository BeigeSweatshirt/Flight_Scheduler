/* threading with sockets https://www.youtube.com/watch?v=Pg_4Jz8ZIH4
* guide on how to correctly dereference null pointers: https://www.youtube.com/watch?v=dQw4w9WgXcQ
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define RED     "\x1b[31m"
#define WHITE   "\x1b[0m"

typedef struct {
    int row;
    int col;
} Seat;

const int DEFAULT_PORT = 5432;
const int BACKLOG = 50;
static char** seats;
static Seat xy;
pthread_mutex_t lock;

void * new_client(void *client_socket);
bool has_valid_seats(int argc, char * argv[]);
bool correct_num_args(int argc);
bool valid_input (int rows, int cols);
void print_seats();
void init_seats();
int init_server();
void init_server_defaults(struct sockaddr_in *server);
void reserve_seat(Seat *seat);
bool seat_status(Seat *seat);
bool all_seats_reserved();

int main(int argc, char * argv[]) {
   // collect input from user
   if (argc == 3) {
      if(!has_valid_seats(argc, argv)) {
      printf("Correct usage is: ./server ROWS COLUMNS [CONFIG_FILE]\n");
      exit(EXIT_FAILURE);
      }
      xy.row = (int) strtod(argv[1], NULL);
      xy.col = (int) strtod(argv[2], NULL);
   } else {
      xy.row = 10;
      xy.col = 10;
   }

   init_seats();

   int server_sock = init_server();
   int client_sock = 0;

   pthread_t t;
   if (pthread_mutex_init(&lock, NULL) != 0) {
      printf("\n mutex init has failed\n");
      return 1;
   }

   
   while(!all_seats_reserved()) {
      if ((client_sock = accept(server_sock, NULL, NULL)) < 0) {
      perror("could not accept client connection");
      }
      pthread_create(&t, NULL, new_client, &client_sock);
      printf("test");
   }

   // // deallocate resources
   for(int i = 0; i < xy.row; i++) {
      free(seats[i]);
   }
   free(seats);
   close(client_sock);
   exit(EXIT_SUCCESS);
}

void * new_client(void *p_client_sock) {
   int client_sock = *((int*)p_client_sock);
   send(client_sock, &xy, sizeof(xy), 0);

   Seat seat;
   long seat_available;
   while(true) {
      recv(client_sock, &seat, sizeof(Seat), 0);
      seat_available = 0;
      seat_available = seat_status(&seat);
      send(client_sock, &seat_available, sizeof(seat_available), 0);
      if (seat_available) reserve_seat(&seat);
      print_seats();
      if (all_seats_reserved()) {
         seat_available = -1;
         send(client_sock, &seat_available, sizeof(seat_available), 0);
         exit(EXIT_SUCCESS);
      }
   }
   return NULL;
}

bool has_valid_seats(int argc, char * argv[]) {
   int rows = (int) strtod(argv[1], NULL);
   int cols = (int) strtod(argv[2], NULL);
   return valid_input(rows, cols);
}

bool valid_input (int rows, int cols) {
   if ((rows <= 0) || (cols <= 0)) {
        printf("One or more parameters was invalid. All parameters should be positive integers.\n");
        return false;
    }
    return true;
}

void init_seats() {
   // initialize seat matrix
   seats = (char **)malloc(xy.row*sizeof(int*));
   for (int i = 0; i < xy.row; i++) {
            seats[i] = (char*)malloc(xy.col*sizeof(xy.col));
   }
   for (int i = 0; i < xy.row; i++) {
      for (int j = 0; j < xy.col; j++) {
            seats[i][j] = 'A';
      }
   }
}

void init_server_defaults(struct sockaddr_in *server) {
   server->sin_family = AF_INET;
   server->sin_addr.s_addr = INADDR_ANY;
   server->sin_port = htons(DEFAULT_PORT); 
}

void print_seats() {
   char c;
   printf("\n");
   for (int i = 0; i < xy.row; i++) {
            for (int j = 0; j < xy.col; j++) {
               c = seats[i][j];
               if (c == 'R') printf("%s", RED);
               else printf("%s", WHITE);
               printf("%c", seats[i][j]);
            }
            printf("\n");
   }
   printf("\n");
   printf("%s", WHITE);
}

void reserve_seat(Seat *seat) {
   pthread_mutex_lock(&lock);
   seats[seat->row][seat->col] = 'R';
   pthread_mutex_unlock(&lock);
}

bool seat_status(Seat *seat) {
        if (seats[seat->row][seat->col] == 'A') return 1;
        else return 0;
}

bool all_seats_reserved() {
   for (int i = 0; i < xy.row; i++) {
      for (int j = 0; j < xy.col; j++){
         if (seats[i][j] == 'A') return false;
      }
   }
   return true;
}

int init_server() {
   // initialize server properties
   struct sockaddr_in server;
   init_server_defaults(&server);

   // create socket
   int server_sock;
   if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("Failed to create socket");
      exit(EXIT_FAILURE);
   }

   int opt = 1;
   /* helps with recycling IP/ports */
   if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
      perror("setsockopt failed");
      exit(EXIT_FAILURE);
   }

   // bind socket
   if(bind(server_sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
      perror("error binding");
      exit(EXIT_FAILURE);
   }

   // listen on socket
   if (listen(server_sock, BACKLOG) < 0) {
      perror("listening for clients failed");
      exit(EXIT_FAILURE);
   }
   return server_sock;
}
