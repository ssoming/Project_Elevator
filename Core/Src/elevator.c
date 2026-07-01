
#include "elevator.h"

// --- 핀 정의 (사용자 요청 핀 매핑) ---
// Stepper: PA4, PB0, PC1, PC0
GPIO_TypeDef* STEP_PORT[4] = {GPIOA, GPIOB, GPIOC, GPIOC};
uint16_t STEP_PIN[4] = {GPIO_PIN_4, GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_0};

// LED Bar: 1~8 (PC9, PA5, PA7, PB6, PC7, PA9, PA8, PB10)
GPIO_TypeDef* BAR_PORT[8] = {GPIOC, GPIOA, GPIOA, GPIOB, GPIOC, GPIOA, GPIOA, GPIOB};
uint16_t BAR_PIN[8] = {GPIO_PIN_9, GPIO_PIN_5, GPIO_PIN_7, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_9, GPIO_PIN_8, GPIO_PIN_10};

// FND: PC8, PC6, PC5, PA12, PA11, PB12, PB2 (A~G)
GPIO_TypeDef* FND_PORT[7] = {GPIOC, GPIOC, GPIOC, GPIOA, GPIOA, GPIOB, GPIOB};
uint16_t FND_PIN[7] = {GPIO_PIN_8, GPIO_PIN_6, GPIO_PIN_5, GPIO_PIN_12, GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_2};

// 버튼 (PB1, PB15, PB14)
GPIO_TypeDef* BTN_PORT[3] = {GPIOB, GPIOB, GPIOB};
uint16_t BTN_PIN[3] = {GPIO_PIN_1, GPIO_PIN_15, GPIO_PIN_14};

// FND 패턴 (0~9, Common Cathode)
const uint8_t FND_NUM[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

// 스텝모터 시퀀스 (1상 여자)
const uint8_t STEP_SEQ[4][4] = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1} };

// --- 상태 관리 변수 (전역) ---
ElevState currentState = ELEV_IDLE;
int currentFloor = 1;
int targetFloor = 1;

// "마지막으로 실행한 시간"을 저장하는 변수들 (Non-blocking의 핵심)
uint32_t lastMotorTime = 0;
uint32_t lastBarTime = 0;

// 현재 위치 인덱스들
int stepIdx = 0;    // 모터 스텝 (0~3)
int barIdx = 0;     // LED Bar 위치 (0~7)


uint32_t lastBlinkTime = 0;
uint8_t blinkToggleCount = 0;   // 0~4 (토글 4번 = 2회 깜빡임)
uint8_t blinkOn = 0;            // 0:OFF, 1:ON


// --- 내부 함수 프로토타입 ---
void SetFND(int num);
void StepMotorOneStep(int dir);
void UpdateLEDBarOneStep(void);
void StopComponents(void);

// --- 초기화 ---
void Elevator_Init(void) {
    SetFND(currentFloor);
    StopComponents();
}

// --- 메인 로직 (for문 없음!) ---
void Elevator_Loop(void)
{
    uint32_t now = HAL_GetTick();

    // 0) 도착 깜빡임 상태 처리 (최우선)
    if (currentState == ELEV_ARRIVED_BLINK) {
        if (now - lastBlinkTime >= 200) {   // 200ms 간격 (원하면 조절)
            lastBlinkTime = now;

            blinkOn = !blinkOn;
            if (blinkOn) LEDBar_AllOn();
            else         LEDBar_AllOff();

            blinkToggleCount++;

            // ON/OFF 2회 = 토글 4번
            if (blinkToggleCount >= 4) {
                LEDBar_AllOff();
                currentState = ELEV_IDLE;
            }
        }
        return; // BLINK 중에는 나머지 로직 수행 금지
    }

    // 1) 버튼 입력 (IDLE일 때만)
    if (currentState == ELEV_IDLE) {
        if (HAL_GPIO_ReadPin(BTN_PORT[0], BTN_PIN[0]) == GPIO_PIN_RESET) targetFloor = 1;
        else if (HAL_GPIO_ReadPin(BTN_PORT[1], BTN_PIN[1]) == GPIO_PIN_RESET) targetFloor = 2;
        else if (HAL_GPIO_ReadPin(BTN_PORT[2], BTN_PIN[2]) == GPIO_PIN_RESET) targetFloor = 3;

        if (targetFloor > currentFloor) currentState = ELEV_MOVING_UP;
        else if (targetFloor < currentFloor) currentState = ELEV_MOVING_DOWN;

        // IDLE일 때 LED Bar는 OFF 유지
        LEDBar_AllOff();
    }

    // 2) 이동 처리
    if (currentState != ELEV_IDLE) {
        if (now - lastMotorTime >= 2) {
            int dir = (currentState == ELEV_MOVING_UP) ? 1 : -1;
            StepMotorOneStep(dir);
            lastMotorTime = now;
        }

        if (now - lastBarTime >= 100) {
            UpdateLEDBarOneStep();
            lastBarTime = now;
        }
    }
}
//void Elevator_Loop(void) {
//    uint32_t now = HAL_GetTick(); // 현재 시간 확인
//
//    // 1. 버튼 확인 (IDLE 상태일 때만 입력 받음)
//    if (currentState == ELEV_IDLE) {
//        if (HAL_GPIO_ReadPin(BTN_PORT[0], BTN_PIN[0]) == GPIO_PIN_RESET) targetFloor = 1;
//        else if (HAL_GPIO_ReadPin(BTN_PORT[1], BTN_PIN[1]) == GPIO_PIN_RESET) targetFloor = 2;
//        else if (HAL_GPIO_ReadPin(BTN_PORT[2], BTN_PIN[2]) == GPIO_PIN_RESET) targetFloor = 3;
//
//        // 목표 층이 다르면 상태 변경
//        if (targetFloor > currentFloor) currentState = ELEV_MOVING_UP;
//        else if (targetFloor < currentFloor) currentState = ELEV_MOVING_DOWN;
//    }
//
//    // 2. 움직임 처리 (IDLE이 아닐 때)
//    if (currentState != ELEV_IDLE) {
//
//        // [모터 처리] 2ms 마다 1스텝 이동
//        if (now - lastMotorTime >= 2) {
//            int dir = (currentState == ELEV_MOVING_UP) ? 1 : -1; // 1: 정방향, -1: 역방향
//            StepMotorOneStep(dir);
//            lastMotorTime = now; // 시간 갱신
//        }
//
//        // [LED Bar 처리] 100ms 마다 한 칸 이동
//        if (now - lastBarTime >= 100) {
//            UpdateLEDBarOneStep();
//            lastBarTime = now; // 시간 갱신
//        }
//    } else {
//        // IDLE 상태면 LED Bar 끄기 (한 번만 실행되도록 플래그 처리하면 더 좋음)
//        // 여기선 간단하게 매번 끔 (오버헤드 적음)
//         for(int i=0; i<8; i++) HAL_GPIO_WritePin(BAR_PORT[i], BAR_PIN[i], GPIO_PIN_RESET);
//    }
//}

// --- 센서 인터럽트 처리 (EXTI 콜백에서 호출) ---
void Elevator_SensorCallback(uint16_t GPIO_Pin) {
    int detected = 0;
    // PB5(1층), PB3(2층), PA10(3층)
    if (GPIO_Pin == GPIO_PIN_5) detected = 1;
    else if (GPIO_Pin == GPIO_PIN_3) detected = 2;
    else if (GPIO_Pin == GPIO_PIN_10) detected = 3;

    if (detected != 0) {
        currentFloor = detected;
        SetFND(currentFloor); // 층수 즉시 표시

        // 목표 층 도착 확인
        if (currentFloor == targetFloor) {
            StopComponents();               // 모터 전원 차단은 즉시
            currentState = ELEV_ARRIVED_BLINK;

            // 깜빡임 초기화
            lastBlinkTime = HAL_GetTick();
            blinkToggleCount = 0;
            blinkOn = 0;

            LEDBar_AllOff();                // 시작 상태를 OFF로 통일
        }
    }
}

// --- 하위 제어 함수들 ---

// 모터를 딱 1스텝만 움직이는 함수
void StepMotorOneStep(int dir) {
    if (dir == 1) stepIdx++;
    else stepIdx--;

    // 인덱스 순환 처리 (0~3)
    if (stepIdx > 3) stepIdx = 0;
    if (stepIdx < 0) stepIdx = 3;

    // 핀 설정 (for문 대신 직접 써도 되고, 4번 반복은 매우 짧아서 for문 써도 무방하나 요청대로 품)
    HAL_GPIO_WritePin(STEP_PORT[0], STEP_PIN[0], STEP_SEQ[stepIdx][0]);
    HAL_GPIO_WritePin(STEP_PORT[1], STEP_PIN[1], STEP_SEQ[stepIdx][1]);
    HAL_GPIO_WritePin(STEP_PORT[2], STEP_PIN[2], STEP_SEQ[stepIdx][2]);
    HAL_GPIO_WritePin(STEP_PORT[3], STEP_PIN[3], STEP_SEQ[stepIdx][3]);
}

//// LED Bar를 딱 한 칸만 옮기는 함수
//void UpdateLEDBarOneStep(void) {
//    // 1. 현재 켜진 LED 끄기
//    HAL_GPIO_WritePin(BAR_PORT[barIdx], BAR_PIN[barIdx], GPIO_PIN_RESET);
//
//    // 2. 인덱스 이동
//    if (currentState == ELEV_MOVING_UP) {
//        barIdx++;
//        if (barIdx > 7) barIdx = 0; // 1 -> 8 순환
//    } else {
//        barIdx--;
//        if (barIdx < 0) barIdx = 7; // 8 -> 1 순환
//    }
//
//    // 3. 새 LED 켜기
//    HAL_GPIO_WritePin(BAR_PORT[barIdx], BAR_PIN[barIdx], GPIO_PIN_SET);
//}

void LEDBar_AllOn(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9,  GPIO_PIN_SET);   // 1
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5,  GPIO_PIN_SET);   // 2
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7,  GPIO_PIN_SET);   // 3
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6,  GPIO_PIN_SET);   // 4
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7,  GPIO_PIN_SET);   // 5
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9,  GPIO_PIN_SET);   // 6
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8,  GPIO_PIN_SET);   // 7
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);   // 8
}

void LEDBar_AllOff(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8,  GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
}


// LED Bar를 딱 한 칸만 옮기는 함수 (방향 반전)
void UpdateLEDBarOneStep(void)
{
    uint8_t physIdx;

    /* 현재 LED 끄기 (반전된 실제 인덱스) */
    physIdx = 7 - barIdx;
    HAL_GPIO_WritePin(BAR_PORT[physIdx], BAR_PIN[physIdx], GPIO_PIN_RESET);

    /* 논리적 인덱스 이동 */
    if (currentState == ELEV_MOVING_UP) {
        barIdx++;
        if (barIdx > 7) barIdx = 0;
    } else {
        if (barIdx == 0) barIdx = 7;
        else barIdx--;
    }

    /* 새 LED 켜기 (반전된 실제 인덱스) */
    physIdx = 7 - barIdx;
    HAL_GPIO_WritePin(BAR_PORT[physIdx], BAR_PIN[physIdx], GPIO_PIN_SET);
}


//void SetFND(int num) {
//    uint8_t ptrn = FND_NUM[num];
//    // FND 핀 설정 (비트 연산으로 각 핀 제어)
//    for(int i=0; i<7; i++) {
//        // 이 for문은 논블로킹에 영향 없음 (GPIO 제어는 수 마이크로초 소요)
//        // 하지만 for문이 싫다면 7줄로 풀어서 쓰면 됩니다.
//        HAL_GPIO_WritePin(FND_PORT[i], FND_PIN[i], (ptrn & (1<<i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
//    }
//}

void SetFND(int num) {
    // 0~9 범위를 벗어나지 않도록 방어 코드 (필요 시)
    if (num < 0 || num > 9) return;

    uint8_t ptrn = FND_NUM[num];

    // FND_PORT와 FND_PIN 배열에 정의된 순서대로 (a, b, c, d, e, f, g) 제어
    // (ptrn & (1 << 0)) 은 패턴의 0번째 비트(a세그먼트)가 1인지 확인합니다.
    HAL_GPIO_WritePin(FND_PORT[0], FND_PIN[0], (ptrn & (1 << 0)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // a
    HAL_GPIO_WritePin(FND_PORT[1], FND_PIN[1], (ptrn & (1 << 1)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // b
    HAL_GPIO_WritePin(FND_PORT[2], FND_PIN[2], (ptrn & (1 << 2)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // c
    HAL_GPIO_WritePin(FND_PORT[3], FND_PIN[3], (ptrn & (1 << 3)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // d
    HAL_GPIO_WritePin(FND_PORT[4], FND_PIN[4], (ptrn & (1 << 4)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // e
    HAL_GPIO_WritePin(FND_PORT[5], FND_PIN[5], (ptrn & (1 << 5)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // f
    HAL_GPIO_WritePin(FND_PORT[6], FND_PIN[6], (ptrn & (1 << 6)) ? GPIO_PIN_SET : GPIO_PIN_RESET); // g
}

void StopComponents(void) {
    // 모터 핀 모두 Low (전력 소모 방지)
    HAL_GPIO_WritePin(STEP_PORT[0], STEP_PIN[0], GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEP_PORT[1], STEP_PIN[1], GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEP_PORT[2], STEP_PIN[2], GPIO_PIN_RESET);
    HAL_GPIO_WritePin(STEP_PORT[3], STEP_PIN[3], GPIO_PIN_RESET);

    // LED Bar 모두 끄기
//    for(int i=0; i<8; i++) HAL_GPIO_WritePin(BAR_PORT[i], BAR_PIN[i], GPIO_PIN_RESET);
    LEDBar_AllOff();
}
