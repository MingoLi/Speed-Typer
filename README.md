# To compile
- make

# To host a server
- ./server

# To play as a client
- ./client
- make suer you have the same number of clients as the number of players required (2 clients by default).

# Bonus
- count down implemented
- number of players can be modified by changing the defined constent:  `#define NUMPLAYERS`

# Protocol
- R - registration
- C - count down
- W - word send from server to player
- T - word typed by player
- E - bad attempt on typing
- L - you loss
- V - you win
- e - player exit from the game
- D - server down
