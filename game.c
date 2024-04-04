/**  @file   game.c
     @author Nicholas Grace (ngr55), Jack Miller (jmi145)
     @brief  Main c file of the pong game, handles game state and calls physics and communication functions.
 */

#include "system.h"
#include "ledmat.h"
#include "pacer.h"
#include "navswitch.h"
#include "physics.h"
#include "communication.h"
#include <stdlib.h>

/* Constants. */
#define REFRESH_RATE 50
#define NUM_COLS 5
#define SCORE_FIRST_COL 3
#define WINNING_SCORE 3

/* Bitmask for each column of the display. */
uint8_t display[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

typedef enum {
    GAME_START,
    GAME_ACTIVE,
    GAME_END
} GameState_t;

/** Entry point. */
int main (void)
{
    /* Initialise all necessary api functions for operation.*/
    system_init ();
    navswitch_init ();
    ledmat_init();
    pacer_init(NUM_COLS * REFRESH_RATE);

    /* Initialise game state, communication module and physics state. */
    GameState_t gameState = GAME_START;
    communication_init();
    PhysicsState_t physicsState = physics_init(false);

    uint8_t opponentScore = 0;
    uint8_t score = 0;

    uint8_t column = 0;
    while (1)
    {
        pacer_wait();

        /* Each column is refreshed sequentially at 250Hz, game update is 50Hz. */
        if(column == 0) {
            /* Reset display. */
            for(uint8_t col=0; col<NUM_COLS; col++) {
                display[col] = 0x00;
            }

            /* Check for recieved data from the other funkit, and respond accordingly. */
            CommunicationPacket_t packet = communication_update();
            if(packet.startGame) {
                gameState = GAME_ACTIVE;
                physicsState = physics_init(packet.haveBall);
            } else if(packet.physicsInfo) {
                /* Ball transfers on to this boards display, update ball state accordingly. */
                physicsState.ballActive = true;
                physicsState.ballPosR = packet.posR * PHYSICS_SUBPIXEL+PHYSICS_SUBPIXEL / 2;
                physicsState.ballPosC = 0;
                physicsState.ballVelR = packet.dirR ? abs(physicsState.ballVelR) : -abs(physicsState.ballVelR);
                physicsState.ballVelC = packet.magC;
            } else if(packet.endRound) {
                /* Recieved end round signal so update our score. */
                physicsState.gameOver = true;
                score++;
                gameState = GAME_START;
            } else if(packet.gameOver) {
                /* Only update score if this is the first reception of the game over signal over ir. */
                if(score != WINNING_SCORE && opponentScore != WINNING_SCORE) {
                    score++;
                }
                gameState = GAME_END;
            }

            /* Display score if GAME_START or GAME_END, double width if GAME_END. 
                Update physics and display ball and paddle if GAME_ACTIVE. */
            if(gameState == GAME_START || gameState == GAME_END) {
                for(uint8_t col = SCORE_FIRST_COL; col > SCORE_FIRST_COL - score; col--) {
                    display[col] |= BIT(5);
                    if(gameState == GAME_END) {
                        display[col] |= BIT(4);
                    }
                }
                for(uint8_t col = SCORE_FIRST_COL; col > SCORE_FIRST_COL - opponentScore; col--) {
                    display[col] |= BIT(1);
                    if(gameState == GAME_END) {
                        display[col] |= BIT(2);
                    }
                }
            } else if(gameState == GAME_ACTIVE) {
                physicsState = physics_update(physicsState);

                /* If game over flag is true then the ball went out on this board, so increase opponent score and send the relevant end message over ir. */
                if(physicsState.gameOver) {
                    gameState = GAME_START;
                    opponentScore++;
                    if(opponentScore == WINNING_SCORE) {
                        gameState = GAME_END;
                        communication_send_end_game();
                    } else {
                        communication_send_end_round();
                    }
                }

                /* Display paddle. */
                display[physicsState.paddleC] |= BIT(physicsState.paddleR);
                display[physicsState.paddleC] |= BIT(physicsState.paddleR + 1);

                /* Display ball or send ball transfer over ir. */
                if(physicsState.ballActive) {
                    display[physicsState.ballPosC / PHYSICS_SUBPIXEL] |= BIT(physicsState.ballPosR / PHYSICS_SUBPIXEL);
                } else{
                    /* This function only uses this info if it is the first time it was called per ball transfer (checks if WAITING or SENDING). */
                    communication_send_physics_info(physicsState.ballPosR / PHYSICS_SUBPIXEL, physicsState.ballVelR >= 0, abs(physicsState.ballVelC));
                }
            }
        }
        
        ledmat_display_column(display[column], column);
        column = (column + 1) % NUM_COLS;
    }

    for(uint8_t col = 0; col < NUM_COLS; col++) {
        ledmat_display_column(0x00, column);
    }
}
