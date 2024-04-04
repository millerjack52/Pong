/**  @file   physics.c
     @author Nicholas Grace (ngr55), Jack Miller (jmi145)
     @brief  Contains all physics rules for interactivity between elements.
 */

#include "physics.h"
#include "navswitch.h"
#include <stdlib.h>

/* Constants. */
#define BALL_INIT_R 300
#define BALL_INIT_C 0
#define BALL_INIT_VEL 5
#define BALL_MAX_VEL_C 7

#define PADDLE_INIT_R 2
#define PADDLE_COL 4
#define PADDLE_FORWARD_COL 3
#define PADDLE_FORWARD_TICKS 8
#define PADDLE_MAX_R 5

#define LEFT_EDGE 50
#define RIGHT_EDGE 650
#define BOTTOM_EDGE 0
#define TOP_EDGE 450
#define PADDLE_EDGE 350
#define PADDLE_FORWARD_EDGE 250
#define REVERSE_R 699


/** Initalizes the physics state.
 * @param ballActive Whether the ball is on this board.
 * @return The physics state with initialized ball position, velocity and paddle posistion.
*/
PhysicsState_t physics_init(bool ballActive)
{
    PhysicsState_t physicsState = {
        .ballActive = ballActive,
        .gameOver = false,
        .ballPosR = BALL_INIT_R,
        .ballPosC = BALL_INIT_C,
        .ballVelR = BALL_INIT_VEL,
        .ballVelC = BALL_INIT_VEL,
        .paddleR = PADDLE_INIT_R,
        .paddleC = PADDLE_COL
    };
    return physicsState;
}

/** Updates the state of the ball and paddle, handling collisions.
 * @param currentState The current physics state.
 * @return the new physics state.
*/
PhysicsState_t physics_update(PhysicsState_t currentState)
{
    navswitch_update();

    if(navswitch_push_event_p(NAVSWITCH_SOUTH)) {
        currentState.paddleR++;
    } else if(navswitch_push_event_p(NAVSWITCH_NORTH)) {
        currentState.paddleR--;
    }

    /* Check for a forward input and if so paddle is forward for PADDLE_FORWARD_TICKS frames, with a seperate variable for the physics so only hits the ball once. */
    static uint8_t pushtick = 0;
    static int8_t paddlePhysicsCol = PADDLE_COL;
    if(navswitch_push_event_p(NAVSWITCH_WEST) || pushtick>0) {
        currentState.paddleC = PADDLE_FORWARD_COL;
        if(pushtick == 0) {
            pushtick = PADDLE_FORWARD_TICKS;
            paddlePhysicsCol = PADDLE_FORWARD_COL;
        }
        pushtick--;
    } else {
        currentState.paddleC = PADDLE_COL;
        paddlePhysicsCol = PADDLE_COL;
    }

    if(currentState.paddleR > PADDLE_MAX_R) currentState.paddleR = PADDLE_MAX_R;
    if(currentState.paddleR < 0) currentState.paddleR = 0;

    /* If ball is not active we return after the paddle movement is complete. */
    if(!currentState.ballActive) {
        return currentState;
    }

    currentState.ballPosR += currentState.ballVelR;
    currentState.ballPosC += currentState.ballVelC;
    
    /* Collision. */
    if(currentState.ballPosR < LEFT_EDGE) {
        currentState.ballPosR = LEFT_EDGE + (LEFT_EDGE - currentState.ballPosR);
        currentState.ballVelR = -currentState.ballVelR;
    }
    if(currentState.ballPosR >= RIGHT_EDGE) {
        currentState.ballPosR = RIGHT_EDGE - 1 - (currentState.ballPosR - RIGHT_EDGE);
        currentState.ballVelR = -currentState.ballVelR; 
    }
    /* Transistion edge (to other funkit). */
    if(currentState.ballPosC < BOTTOM_EDGE) {
        currentState.ballActive = false;
        currentState.ballPosC = abs(currentState.ballPosC);
        currentState.ballVelC = abs(currentState.ballVelC);
        currentState.ballPosR = REVERSE_R - currentState.ballPosR;
        currentState.ballVelR = -currentState.ballVelR;
        return currentState;
    }
    /* Losing edge. */
    if(currentState.ballPosC >= TOP_EDGE) {
        currentState.gameOver = true;
        return currentState;
    }

    /* Paddle collision. */
    if(currentState.ballPosC >= PADDLE_EDGE) {
        uint8_t tmpr = currentState.ballPosR / PHYSICS_SUBPIXEL;
        if(tmpr == currentState.paddleR || tmpr == currentState.paddleR + 1) {
            currentState.ballPosC = PADDLE_EDGE - 1 - (currentState.ballPosC - PADDLE_EDGE);
            currentState.ballVelC = -abs(currentState.ballVelC);
        }
    }
    /* If paddle is forward, increase column speed of ball. */
    if(currentState.ballPosC >= PADDLE_FORWARD_EDGE && paddlePhysicsCol == PADDLE_FORWARD_COL) {
        uint8_t tmpr = currentState.ballPosR / PHYSICS_SUBPIXEL;
        if(tmpr == currentState.paddleR || tmpr == currentState.paddleR + 1) {
            currentState.ballVelC = -abs(currentState.ballVelC) - 1;
            if(currentState.ballVelC < -BALL_MAX_VEL_C) currentState.ballVelC = -BALL_MAX_VEL_C;
            paddlePhysicsCol = PADDLE_COL; 
        }
    }

    return currentState;
}
