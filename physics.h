/**  @file   physics.h
     @author Nicholas Grace (ngr55), Jack Miller (jmi145)
     @brief  Contains all physics rules for interactivity between elements.
 */


#ifndef PHYSICS_H
#define PHYSICS_H

#include "system.h"

/* Each led on the LED matrix is divided into 100 subpixels for ball movement. */
#define PHYSICS_SUBPIXEL 100

/* Holds all state infomation for the ball and paddle physics. */
typedef struct {
    bool ballActive;
    bool gameOver;
    int16_t ballPosR;
    int16_t ballPosC;
    int8_t ballVelR;
    int8_t ballVelC;
    int8_t paddleC;
    int8_t paddleR;
} PhysicsState_t;

/** Initalizes the physics state.
 * @param ballActive Whether the ball is on this board.
 * @return The physics state with initialized ball position, velocity and paddle posistion.
*/
PhysicsState_t physics_init(bool ballActive);

/** Updates the state of the ball and paddle, handling collisions.
 * @param currentState The current physics state.
 * @return the new physics state.
*/
PhysicsState_t physics_update(PhysicsState_t currentState);

#endif //PHYSICS_H