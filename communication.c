/** @file communication.c
    @author Nicholas Grace (ngr55), Jack Miller (jmi145)
    @brief State machine handling communication over the ir channel.
*/

#include "communication.h"
#include "ir_uart.h"
#include "led.h"
#include "navswitch.h"

/* One Byte codes for communication over the ir. Including data codes, acknowledgements, and bitmasks for various data types that can be recieved. */
#define BLANK_BYTE 0xFF
#define START_CODE 0xFE
#define START_ACK 0xFD
#define END_CODE 0xFC
#define END_ACK 0xFB
#define GAME_OVER_CODE 0xFA
/* Physics ir transfer method: two bytes are transferred over ir each time the ball goes across the screen, within these two bytes there is a 4 bir prefix
    containing a sequence number in the range 0-7, and all other codes must use a prefix out of this range. The last 4 bits are data bits, the first physics
    packet contains the ball's row posistion in its data bits, and the second contains the row direction and column velocity magnitude. The sequence number is
    used to recieve ack's and detect duplicate transmissions. As the sequence number is 0-7, four ball transfers will occur before the sequence number repeats,
    so old transmissions will not be falsely detected as new data. */
#define PREFIX_MASK 0xF0
#define SUFFIX_MASK 0x0F
#define THREE_MASK 0x07
#define ONE_MASK 0x08
/* Physics acknowledgement prefix, the last 4 bits are set to the sequence number. */
#define PHYSICS_ACK 0xD0
#define SEQ_NUMBER_LIMIT 8

/* Communication states. */
typedef enum {
    START_REC,
    START_SEND,
    RECIEVING,
    WAITING,
    SENDING,
    END_ROUND,
    GAME_OVER
} CommunicationState_t;

/* Construct an empty communication packet. */
static CommunicationPacket_t null_packet(void)
{
    CommunicationPacket_t packet = {
        .startGame = false,
        .haveBall = false,
        .endRound = false,
        .physicsInfo = false,
        .gameOver = false,
    };
    return packet;
} 

/** Construct a communication packet indicating the game has started.
 * @param haveBall Whether this board starts with the ball on their side.
 * @return A communication packet with the startGame flag true.
*/
static CommunicationPacket_t game_start_packet(bool haveBall)
{
    CommunicationPacket_t packet = null_packet();
    packet.startGame = true;
    packet.haveBall = haveBall;
    return packet;
}

/** Construct a communication packet indicating the round has ended.
 * @return A communication packet with the endRound flag set to true.
*/
static CommunicationPacket_t end_round_packet(void)
{
    CommunicationPacket_t packet = null_packet();
    packet.endRound = true;
    return packet;
}

/** Construct a communication packet with physics infomation of the ball's state.
 * @param posR The row posistion of the ball (0-6)
 * @param dirR Whether the ball is moving positively in the row direction.
 * @param magC The magnitude of the velocity in the column direction.
 * @return The packet with the physicsInfo flag set true and the infomation included.
*/
static CommunicationPacket_t communication_physics_packet(uint8_t posR, bool dirR, uint8_t magC)
{
    CommunicationPacket_t packet = null_packet();
    packet.physicsInfo = true;
    /* Take only the lowest three bits of these parameters, as only three bits are transferred over the ir. */
    packet.posR = posR & THREE_MASK;
    packet.magC = magC & THREE_MASK;
    packet.dirR = dirR;
    return packet;
}

/** Construct a communication packet indicating the game has ended.
 * @return A communication packet with the gameOver flag set to true.
*/
static CommunicationPacket_t end_game_packet(void) {
    CommunicationPacket_t packet = null_packet();
    packet.gameOver = true;
    return packet;
}

/* The current sequence number of the data being sent over ir, used to order the physics data being sent over multiple transistions. */
static uint8_t physicsSeqNumber = 0;
/* The physics packet to send/recieve over ir. */
static CommunicationPacket_t physicsPacket;

/* The current communication state, this is updated by public function calls or by recieved data. */
static CommunicationState_t currentState;

/** Initializes communication, calling API functions to initialize the led and ir, and setting the initial state. */
void communication_init(void)
{
    led_init();
    ir_uart_init();
    currentState = START_REC;
    physicsPacket = null_packet();
    physicsPacket.physicsInfo = true;
}

/** Ends the current round. */
void communication_send_end_round(void)
{
    currentState = END_ROUND;
}

/** Ends the game. */
void communication_send_end_game(void)
{
    currentState = GAME_OVER;
}

/** Sets the physics packet to be sent over ir, and sets the state to sending.
 * @param posR The row posistion of the ball.
 * @param dirR Whether the ball is moving positively in the row direction.
 * @param magC The magnitude of the velocity in the column direction.
*/
void communication_send_physics_info(uint8_t posR, bool dirR, uint8_t magC)
{
    /* Can only transistion to the SENDING state if currently WAITING. */
    if(currentState != WAITING) {
        return;
    }
    currentState = SENDING;
    /* Brief error handling for if the sequence number has got out of sync, as it should always be even at start of state. */
    if(physicsSeqNumber % 2 == 1) {
        physicsSeqNumber++;
    }
    physicsPacket = communication_physics_packet(posR, dirR, magC);
}

/**
 * The communication state machine, updates state based on current state and recieved data from ir, and returns any data recieved.
 * @return A communication packet that can be checked for any flags or infomation recieved.
*/
CommunicationPacket_t communication_update(void) {
    /* Takes any input from the ir channel, and then perform behaviour based on the current state. See the switch statement for details about each state. */

    /* The sequence number must always be in the range 0-7, so that other 4 bit prefixes are possible for the other data codes. */
    physicsSeqNumber = physicsSeqNumber % SEQ_NUMBER_LIMIT;

    /* The byte transmitted over ir, BLANK_BYTE is the code if no byte is recieved. This variable is static and persists between
        function calls, because we may want to check it on a later frame than it is recieved, for this reason after this data has
        been used it often needs to be reset to BLANK_BYTE. */
    static uint8_t readData = BLANK_BYTE;

    /* Whether a byte can be send over the ir channel on this frame. */
    bool sendFrame = false;
    static uint8_t tickCount = 0;
    tickCount = (tickCount + 1) % 2;
    if(tickCount == 0) {
        if(ir_uart_write_ready_p()) {
            sendFrame = true;
        } else {
            tickCount++;
        }
    }

    /* Read a byte from the ir channel if there is a byte to read. */
    if(ir_uart_read_ready_p()) {
        readData = ir_uart_getc();
    }

    led_set(LED1, false);

    switch (currentState) {
        case START_REC:
            /* One of the two setup states. 
                Transistion to START_SEND if the navswitch is pushed. 
                Transistion to RECIEVING and return a game start packet where the ball is not on our side if the start code is recieved.
                Also need to check for the end code, as after the end of a round we transistion back to this state, and so may still need to send end ack. */
            
            led_set(LED1, true);

            if(readData == START_CODE) {
                currentState = RECIEVING;
                return game_start_packet(false);
            } else if(readData == END_CODE && sendFrame) {
                readData = BLANK_BYTE;
                ir_uart_putc(END_ACK);
                break;
            }

            navswitch_update();
            if(navswitch_push_event_p(NAVSWITCH_PUSH)) {
                currentState = START_SEND;
                return null_packet();
            }

            break;
        case START_SEND:
            /* One of two setup states. Send start code on all send frames. 
            Transistion to WAITING and return a game start packet with the ball on our side if a start acknowledgement is recieved.
            Transistion to START_REC if a start code is recieved, just in case both funkits enter the send state simultaeneously.
            Also need to check for the end code, as after the end of a round we transistion back to this state, and so may still need to send end ack. */

            led_set(LED1, true);

            if(readData == START_ACK) {
                readData = BLANK_BYTE;
                currentState = WAITING;
                return game_start_packet(true);
            } else if(readData == END_CODE && sendFrame) {
                readData = BLANK_BYTE;
                ir_uart_putc(END_ACK);
                break;
            } else if(readData == START_CODE) {
                readData = BLANK_BYTE;
                currentState = START_REC;
                break;
            }

            if(sendFrame) {
                ir_uart_putc(START_CODE);
            }

            break;
        case RECIEVING:
            /* Recieve the ball's state from the other funkit when the ball crosses the edge of the screen. physicsSeqNumber is used to tell which byte we want to recieve,
                and the infomation is stored in physicsPacket until we can return it. We also need to sends acks if we recieve a start code.
                Transistion to GAME_OVER or START_REC if the game or round ends.
                Return the physicsPacket and transistion to WAITING once the last physics byte is recieved. */

            if(readData == START_CODE && sendFrame) {
                ir_uart_putc(START_ACK);
                readData = BLANK_BYTE;
                break;
            }

            if(readData == GAME_OVER_CODE) {
                readData = BLANK_BYTE;
                currentState = GAME_OVER;
                return end_game_packet();
            }

            if(readData == END_CODE) {
                readData = BLANK_BYTE;
                currentState = START_REC;
                return end_round_packet();
            }

            /* Send acks for any packet we recieve and store physics infomation if we recieve a packet with the current sequence number. */
            uint8_t seqNumber = (readData & PREFIX_MASK) >> 4;
            uint8_t lastPhyisicsSeq = (physicsSeqNumber + SEQ_NUMBER_LIMIT - 1) % SEQ_NUMBER_LIMIT;
            uint8_t data = readData & SUFFIX_MASK;
            if(seqNumber == physicsSeqNumber) {
                if(sendFrame) {
                    readData = BLANK_BYTE;
                    ir_uart_putc(PHYSICS_ACK | physicsSeqNumber);
                }

                if(physicsSeqNumber % 2 == 0) {
                    physicsPacket.posR = data;
                    physicsSeqNumber++;
                } else {
                    physicsPacket.dirR = (data & ONE_MASK) > 0;
                    physicsPacket.magC = data & THREE_MASK;
                    physicsSeqNumber++;
                    currentState = WAITING;
                    return physicsPacket;
                }
            } else if(seqNumber == lastPhyisicsSeq && sendFrame) {
                readData = BLANK_BYTE;
                ir_uart_putc(PHYSICS_ACK | lastPhyisicsSeq);
            }

            break;
        case WAITING: ;
            /* The state while the ball is on this funkits side, checks for if the last physics seq number is still being transmitted (if other funkit is still
                in SENDING state), and send acknowledgement if so. */

            uint8_t lastPhysicsSeqNumber = (physicsSeqNumber + SEQ_NUMBER_LIMIT - 1) % SEQ_NUMBER_LIMIT;
            uint8_t recievedSeqNumber = (readData & PREFIX_MASK) >> 4;
            if(recievedSeqNumber == lastPhysicsSeqNumber && sendFrame) {
                readData = BLANK_BYTE;
                ir_uart_putc(PHYSICS_ACK | lastPhysicsSeqNumber);
            }

            break;
        case SENDING:
            /* Sends the ball's state over ir when the ball reaches the edge of the screen. The data from physicsPacket is sent over the ir channel, 
                with physicsSeqNumber keeping track of whether we are transfering the first or second byte, which we update once ack recieved.
                Transistion to RECIEVING once an acknowledgement for the second byte is recieved. */

            if((readData & PREFIX_MASK) == PHYSICS_ACK) {
                if((readData & SUFFIX_MASK) == physicsSeqNumber) {
                    readData = BLANK_BYTE;
                    if(physicsSeqNumber % 2 == 0) {
                        physicsSeqNumber++;
                    } else {
                        physicsSeqNumber++;
                        currentState = RECIEVING;
                        break;
                    }
                }
            }

            if(sendFrame) {
                uint8_t seqCode = physicsSeqNumber << 4;
                if(physicsSeqNumber % 2 == 0) {
                    ir_uart_putc(seqCode | physicsPacket.posR);
                } else {
                    ir_uart_putc(seqCode | (THREE_MASK & physicsPacket.magC) | (physicsPacket.dirR ? ONE_MASK : 0x00));
                }
            }

            break;
        case END_ROUND:
            /* The end round state, sends a end code to the other funkit every send frame, and waits for an acknowledgement.
                Transistion to START_REC after acknowledgement recieved. */

            if(readData == END_ACK) {
                currentState = START_REC;
                readData = BLANK_BYTE;
                break;
            }

            if(sendFrame) {
                ir_uart_putc(END_CODE);
            }

            break;
        case GAME_OVER:
            /* Send code to other funkit to notify it that the game is over. */

            if(sendFrame) {
                ir_uart_putc(GAME_OVER_CODE);
            }

            break;
    }
    
    return null_packet();
}