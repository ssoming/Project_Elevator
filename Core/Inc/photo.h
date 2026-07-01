#ifndef INC_PHOTO_H_
#define INC_PHOTO_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>


/* 센서 입력 논리 */
typedef enum
{
    PHOTO_ACTIVE_LOW = 0,   // LOW  = 감지
    PHOTO_ACTIVE_HIGH       // HIGH = 감지
} PhotoActiveLevel;

/* 상태 */
typedef enum
{
    PHOTO_NONE = 0,
    PHOTO_DETECTED
} PhotoState;

/* 설정값: main.c에서 핀/정책을 넘김 */
typedef struct
{
    GPIO_TypeDef *photoPort;
    uint16_t      photoPin;

    GPIO_TypeDef *dhtPort;
    uint16_t      dhtPin;

    PhotoActiveLevel activeLevel;

    uint32_t pollPeriodMs;        // 권장: 2~10ms
    uint32_t lcdUpdatePeriodMs;   // 권장: 2000ms
} PhotoConfig;

/* 런타임 객체 */
typedef struct
{
    PhotoConfig cfg;

    volatile uint8_t  extiDetected;
    volatile uint32_t extiCount;

    PhotoState state;
    uint32_t   lastPollMs;
    uint32_t   lastLcdMs;
} Photo;

/* API */
void photoInit(Photo *p, const PhotoConfig *cfg);
void photoLoop(Photo *p);
void photoOn(Photo *p);   // EXTI에서 호출(플래그만)

#endif /* INC_PHOTO_H_ */
