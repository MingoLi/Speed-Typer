# To compile
- make

# To host a server
- ./server

# To play as a client
- ./client
- make suer you have the same number of clients as the number of players required (2 clients by default).

# Sample output
- Client side
```
$ ./server
starting game, waiting for player...
Player 0 id 9533 registerd
Player 1 id 9534 registerd
Game start

Player 0 id 9533 typed: role
Player 0 id 9533 typed: odds
Player 0 id 9533 typed: bran
Player 1 id 9534 typed: role
Player 1 id 9534 typed: odds
Player 0 id 9533 typed: fora
Player 0 id 9533 typed: waag
Player 1 id 9534 typed: bran
Player 0 id 9533 typed: spot
Player 0 id 9533 typed: cavy
Player 0 id 9533 typed: clee
Player 0 id 9533 typed: abed
Player 0 id 9533 typed: Argo
Player 9533 WIIIIIIIN!!!!
```

- Player 0 side 
```
$ ./client
Done registeration, game will start once all player stand by
Player pipe connnected
ready
set
go
Type: role
role
Type: odds
odds
Type: bran
bran
Type: fora
fora
Type: waag
waag
Type: spot
spot
Type: cavy
cavy
Type: clee
clee
Type: abed
abed
Type: Argo
Argo
You Win
```

- Player 1 side
```
$ ./client
Done registeration, game will start once all player stand by
Player pipe connnected
ready
set
go
Type: role
role
Type: odds
odds
Type: bran
bran
Type: fora
You Loss
```


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
