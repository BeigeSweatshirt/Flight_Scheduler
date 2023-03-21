/*https://stackoverflow.com/questions/48413430/c-code-to-read-config-file-and-parse-directives 
* 
*/ 

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <unistd.h>

const int DEFAULT_PORT = 5432;
int timeout = 10; //...sloppy, but it works.

#define CONFIG_SIZE (256)
#define HOST_SET (1)
#define PORT_SET (2)
#define TIMEOUT_SET (3)

typedef struct {
    int row;
    int col;
} Seat;

typedef struct config {
  unsigned int set;
  char host[45];
  unsigned int port;
  unsigned int timeout;
} CONFIG;

bool has_valid_args(int argc, char * argv []);
bool correct_num_args(int argc);
bool mode_specified(char * mode);
int get_server_handle();
Seat *get_seats_dimensions(int sock);
void init_server_defaults(struct sockaddr_in *server);
void init_server_fromfile(struct sockaddr_in *server, char * file);
int parse_config(char *buf, CONFIG *config);
int get_random_delay();
Seat *get_random_seat(Seat *xy);
Seat *get_seat_manually(Seat *xy);
bool valid_number(char * c, int length);
long seat_status(Seat *seat, int sock);


int main(int argc, char * argv[]) {
   if (!has_valid_args(argc, argv)) {
      printf("Correct usage is: ./server (manual|automatic) [CONFIG_FILE]\n");
      exit(EXIT_FAILURE);
   }

   srand(time(NULL));

   struct sockaddr_in server;
   if (argc == 2) init_server_defaults(&server);
   else init_server_fromfile(&server, argv[2]);

   int sock = get_server_handle(&server);
   Seat xy = *get_seats_dimensions(sock);
   Seat seat;

   bool all_seats_full = false;
   bool is_auto = (strcmp("manual", argv[1]) != 0); 
   while(!all_seats_full) {
      if (is_auto) {
         seat = *get_random_seat(&xy);
         sleep(get_random_delay());
      }
      else seat = *get_seat_manually(&xy);

      printf("Attempting to reserve seat(%d, %d). ", seat.row, seat.col);
      long status = seat_status(&seat, sock);
      if (status == 1) printf("Success\n");
      else {
         printf("Failed: ");
         if (status == 0) printf("Seat reserved\n");
         else if (status == -1) {
            printf("All seats reserved.\n");
            status = seat_status(&seat, sock);
            all_seats_full = true;
         }
      }
   }

   /* deallocate storage */
   close(sock);
   return 0;
}

bool has_valid_args(int argc, char * argv []) {
   if (!correct_num_args(argc)) return false;
   if (!mode_specified(argv[1])) return false;
   return true;
}

bool correct_num_args(int argc) {
   if (argc != 2 && argc != 3) {
    printf("Invalid number of parameters\n");
    return false;
  }
  return true;
}

bool mode_specified(char * mode) {
   if ((strcmp("automatic", mode) != 0) &&
   (strcmp("auto", mode) != 0) &&
   (strcmp("manual", mode) != 0)) {
      printf("Invalid control mode\n");
      return false;
   }
   return true;
}

void init_server_defaults(struct sockaddr_in *server) {
   server->sin_family = AF_INET;
   server->sin_addr.s_addr = INADDR_ANY;
   server->sin_port = htons(DEFAULT_PORT); 
}

void init_server_fromfile(struct sockaddr_in *server, char * file) {
   FILE *f = fopen(file, "r");
   char buf[CONFIG_SIZE];
   CONFIG config[1];
   config->set = 0u;
   int line_number = 0;

   if (f == NULL) {
      printf("invalid file");
      exit(EXIT_FAILURE);
   }

   while (fgets(buf, sizeof buf, f)) {
      ++line_number;
      int err = parse_config(buf, config);
      if (err) fprintf(stderr, "error line %d: %d\n", line_number, err);
   }

   printf("[host=%s,port=", config->set & HOST_SET ? config->host : "<unset>");
   if (config->set & PORT_SET) printf("%u,timeout=", config->port); else printf("<unset>]");
   if (config->set & TIMEOUT_SET) printf("%u]\n", config->timeout); else printf("<unset>]");
   
   server->sin_family = AF_INET;
   server->sin_addr.s_addr = inet_addr(config->host);
   server->sin_port = htons(config->port);

}

int parse_config(char *buf, CONFIG *config) {
   char dummy[CONFIG_SIZE];
   if (sscanf(buf, " %s", dummy) == EOF) return 0; // blank line
   if (sscanf(buf, " %[#]", dummy) == 1) return 0; // comment
   if (sscanf(buf, " IP = %s", config->host) == 1) {
    if (config->set & HOST_SET) return HOST_SET; // error; host already set
    config->set |= HOST_SET;
    return 0;
  }
  if (sscanf(buf, " Port = %u", &config->port) == 1) {
    if (config->set & PORT_SET) return PORT_SET; // error; port already set
    config->set |= PORT_SET;
    return 0;
  }
  if (sscanf(buf, " Timeout = %u", &config->timeout) == 1) {
    if (config->set & TIMEOUT_SET) return TIMEOUT_SET; // error; timeout already set
    config->set |= TIMEOUT_SET;
    return 0;
  }
  return 3; //syntax error
}

int get_random_delay() {
   return 3 + (2 * (rand() % 3));
}

int get_server_handle(struct sockaddr_in *server){
   // Create a network socket
   int sock;
   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("Failed to create socket");
      exit(EXIT_FAILURE);
   }

   // Attempt a connection. if not possible, wait 
   for (int i = 0; i < timeout; i++) {
      if (connect(sock, (struct sockaddr *) server, sizeof(*server)) >= 0) return sock;
      printf("couldn't to connect to server. retrying...\n");
      sleep(1);
   }
   perror("failed to connect to server");
   exit(EXIT_FAILURE);
   return sock;
}

Seat *get_seats_dimensions(int sock) {
   Seat *xy = malloc(sizeof *xy);
   recv(sock, xy, sizeof(Seat), 0);
   printf("Plane size:\n");
   printf("Rows: %u\n", xy->row);
   printf("Cols: %u\n", xy->col);
   printf("======================================================");
   printf("\n\n");
   return xy;
}

Seat *get_random_seat(Seat *xy) {
   Seat *seat = malloc(sizeof *seat);
   seat->row = rand() % xy->row;
   seat->col = rand() % xy->col;
   return seat;
}

Seat *get_seat_manually(Seat *xy) {
   Seat *seat = malloc(sizeof *seat);
   char rst[256], cst[256];
   char* str_end;
   bool valid_input=false;
   while(!valid_input) {
      printf("Row: ");
      scanf("%s", &rst);
      printf("Column: ");
      scanf("%s", &cst);
      if ((valid_number(rst, xy->row) && (valid_number(cst, xy->col)))) {
         valid_input = true;
      } else {
         printf("\ninvalid coordinates. row/cols must be positive integers between 0 and 1-row/column.\n\n");
      }
   }
   seat->row = (int) strtod(rst, &str_end);
   seat->col = (int) strtod(cst, &str_end);
   return seat;
}

bool valid_number(char * c, int length) {
   char* str_end;
   int value = (int) strtod(c, &str_end);
   if (*str_end != '\0') {
      printf("test");
      return false;
   }
   if (value < 0) return false;
   if (value >= length) return false;
   return true;
}

long seat_status(Seat *seat, int sock) {
   long seat_available;
   send(sock, seat, sizeof(Seat), 0);
   recv(sock, &seat_available, sizeof(seat_available), 0);
   return seat_available;
}




/*
1. get dimensions
while (1) {
   2. get random seat within dimensions
   3. attempt seat
   4. get bool whether or not seat was reserved
   5. show result
}
*/