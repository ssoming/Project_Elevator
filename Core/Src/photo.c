#include "photo.h"

#include <string.h>
#include <stdio.h>

#include "I2C_LCD.h"
#include "dht11.h"
#include "usart.h"   // delay_us() 사용

DHT11 dht;

/* 내부 함수 */
static PhotoState photoGetState(const Photo *p, GPIO_PinState s);
static void photoLcdInit(void);
static void photoLcdClear(void);
static void photoLcdPrint(void);

void photoInit(Photo *p, const PhotoConfig *cfg)
{
    if (p == NULL || cfg == NULL) return;

    p->cfg = *cfg;

    p->extiDetected = 0;
    p->extiCount    = 0;
    p->state        = PHOTO_NONE;

    p->lastPollMs = HAL_GetTick();
    p->lastLcdMs  = p->lastPollMs;

    /* LCD 초기화 */
    photoLcdInit();

    /* DHT11 초기화(핀은 cfg로 받음) */
    dht11Init(&dht, p->cfg.dhtPort, p->cfg.dhtPin);

    /* 초기 센서 상태 반영(핀은 cfg로 받음) */
    GPIO_PinState s = HAL_GPIO_ReadPin(p->cfg.photoPort, p->cfg.photoPin);
    p->state = photoGetState(p, s);

    if (p->state == PHOTO_DETECTED)
    {
        photoLcdPrint();
        p->lastLcdMs = HAL_GetTick();
    }
    else
    {
        photoLcdClear();
    }
}

void photoLoop(Photo *p)
{
    if (p == NULL) return;

    uint32_t now = HAL_GetTick();

    if ((now - p->lastPollMs) < p->cfg.pollPeriodMs)
        return;
    p->lastPollMs = now;

    /* EXTI 플래그는 “즉시 반응” 용도로만 소비 (폴링이 핵심) */
    p->extiDetected = 0;

    GPIO_PinState s = HAL_GPIO_ReadPin(p->cfg.photoPort, p->cfg.photoPin);
    PhotoState newState = photoGetState(p, s);

    /* Rising EXTI 없어도 폴링으로 상태변화 감지 => 사라짐 시 LCD OFF */
    if (newState != p->state)
    {
        p->state = newState;

        if (p->state == PHOTO_DETECTED)
        {
            photoLcdPrint();
            p->lastLcdMs = now;
        }
        else
        {
            photoLcdClear();
        }
        return;
    }

    if (p->state == PHOTO_DETECTED &&
        (now - p->lastLcdMs) >= p->cfg.lcdUpdatePeriodMs)
    {
        photoLcdPrint();
        p->lastLcdMs = now;
    }
}

void photoOn(Photo *p)
{
    if (p == NULL) return;

    /* ISR에서는 최소 처리만 */
    p->extiCount++;
    p->extiDetected = 1;
}

/* ===== 내부 구현 ===== */

static PhotoState photoGetState(const Photo *p, GPIO_PinState s)
{
    if (p->cfg.activeLevel == PHOTO_ACTIVE_LOW)
        return (s == GPIO_PIN_RESET) ? PHOTO_DETECTED : PHOTO_NONE;
    else
        return (s == GPIO_PIN_SET) ? PHOTO_DETECTED : PHOTO_NONE;
}

static void photoLcdInit(void)
{
    i2c_lcd_init();
    photoLcdClear();
}

static void photoLcdClear(void)
{
    lcd_command(CLEAR_DISPLAY);
    HAL_Delay(2);
}

static void photoLcdPrint(void)
{
    char line[17];
    uint8_t ok;

    ok = dht11Read(&dht);
    if (!ok) ok = dht11Read(&dht);

    move_cursor(0, 0);
    lcd_string("Welcome"); // 16칸 맞춤(잔상 방지)

    move_cursor(1, 0);
    if (ok)
    {
        /* 16x2 LCD에 안전한 짧은 포맷 */
        snprintf(line, sizeof(line), "Tem:%2dC Hum:%2d%%", (int)dht.temperature, (int)dht.humidity);
    }
    else
    {
        snprintf(line, sizeof(line), "DHT11 error");
    }
    lcd_string(line);

    for (uint8_t i = (uint8_t)strlen(line); i < 16; i++)
        lcd_data(' ');
}
