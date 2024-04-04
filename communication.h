/** @file communication.h
    @author Nicholas Grace (ngr55), Jack Miller (jmi145)
    @brief State machine handling communication over the ir channel.
*/

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "system.h"

/* Holds all infomation that may be transmitted over ir. */
typedef struct {
    bool startGame;
    bool haveBall;
    bool endRound;
    bool gameOver;
    bool physicsInfo;
    uint8_t posR;
    bool dirR;
    uint8_t magC;
} CommunicationPacket_t;

/** Initializes communication, calling API functions to initialize the led and ir, and setting the initial state. */
void communication_init(void);

/** Ends the current round. */
void communication_send_end_round(void);

/** Ends the game. */
void communication_send_end_game(void);

/** Sets the physics packet to be sent over ir, and sets the state to sending.
 * @param posR The row posistion of the ball.
 * @param dirR Whether the ball is moving positively in the row direction.
 * @param magC The magnitude of the velocity in the column direction.
*/
void communication_send_physics_info(uint8_t posR, bool dirR, uint8_t magC);

/**
 * The communication state machine, updates state based on current state and recieved data from ir, and returns any data recieved.
 * @return A communication packet that can be checked for any flags or infomation recieved.
*/
CommunicationPacket_t communication_update(void);

#endif //COMMUNICATION_H