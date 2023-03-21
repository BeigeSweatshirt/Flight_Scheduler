# Flight_Scheduler 
A flight reservation simulator. A client can attempt to reserve seats on a server, and the server responds with whether or not the seat is available. The client can be configured to automatically generate seats, or manually prompt the user to specify a seat. The IP, port and timeout of the client can be configured in the config.ini file. The server can be configured to use any seating dimensions.

## How to Build
Download the repo:
git clone https://github.com/BeigeSweatshirt/Flight_Scheduler

Compile:
cd Process_Scheduler
gcc -o client client.c
gcc -o server server.c -pthread

## How to Run:
./server X Y

Where X and Y are the dimensions of the seats on a plane.

./client (manual|automatic) [config]

Where
- manual: the user will be prompted each time to choose a seat to reserve
- automatic: the client will automatically generate seats
- config: an (optional) path to a config file, allowing a custom IP, port and timeout.
