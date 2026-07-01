
#ifndef INC_ELEVATOR_H_
#define INC_ELEVATOR_H_

#include "main.h"

// 엘리베이터 상태
//typedef enum {
//    ELEV_IDLE,          // 정지 상태
//    ELEV_MOVING_UP,     // 올라가는 중
//    ELEV_MOVING_DOWN    // 내려가는 중
//} ElevState;

typedef enum {
    ELEV_IDLE = 0,
    ELEV_MOVING_UP,
    ELEV_MOVING_DOWN,
    ELEV_ARRIVED_BLINK
} ElevState;

void Elevator_Init(void);
void Elevator_Loop(void); // 메인 while문에서 계속 호출될 함수
void Elevator_SensorCallback(uint16_t GPIO_Pin); // 인터럽트 발생 시 호출

void LEDBar_AllOn(void);
void LEDBar_AllOff(void);


#endif /* INC_ELEVATOR_H_ */
