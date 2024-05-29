# UDP P2P Game
Console application/game written in C using UDP Protocol.  
Players connect directly to each other in a peer to peer way. All communication is asynchronous.

This project was developed as part of laboratory assignment on PS course at NCU Toruń.

## Game rules
First player is given a random number between 1 and 10 and then chooses a number that is greater than the number given but only by at most 10. That number is then sent to the other player who also chooses a number greater by at most 10 and sends it back to player 1. This continues until one player chooses 50 which is the biggest number players can choose and that player then loses the game.

## Interface
Since the game is console based the interface consists of text commands. Input validation is implemented. Players can play multiple rounds of the game and each player's score is displayed on the scoreboard.
