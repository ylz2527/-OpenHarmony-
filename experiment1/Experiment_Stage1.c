#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_pwm.h"
#include "hi_pwm.h"
#include "iot_uart.h"
#include "hi_uart.h"
#include "iot_errno.h"

#define UART_BUFF_SIZE   1000
#define MSGQUEUE_OBJECTS 16

// 消息队列数据结构
typedef struct {
    char *Buf;
} MSGQUEUE_OBJ_t_rx;

// 定时器对象
typedef struct {
    osTimerId_t id;
    uint32_t timerDelay_1;
    osTimerFunc_t func;
    osTimerType_t type;
} TIMER_OBJ_t;

osMessageQueueId_t mid_MsgQueue;
osMutexId_t mutex_id;

// 函数声明（避免隐式声明警告）
static void F1_Pressed(char *arg);
static void F2_Pressed(char *arg);
static void Timer1_Callback(void);
static void Peripheral_Init(void);
static void Stage1_Task1(void);
static void Stage1_Task2(void);
static void Stage1_Task3(void);

// ========== 按键中断回调 ==========
static void F1_Pressed(char *arg)
{
    (void)arg;
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 1);   // 继电器1开（或G灯亮）
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_8, 1);   // 继电器2开（或B灯亮）
    printf("KEY1! Relay ON\n");
}

static void F2_Pressed(char *arg)
{
    (void)arg;
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 0);   // 继电器1关
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_8, 0);   // 继电器2关
    printf("KEY2! Relay OFF\n");
}

// ========== 定时器回调（蜂鸣器鸣叫） ==========
static void Timer1_Callback(void)
{
    printf("-------------Buzzer is ON-------------\n");
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, 0);   // 低电平鸣叫
    sleep(1);
    printf("-------------Buzzer is OFF------------\n");
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, 1);
}

// ========== 外设初始化 ==========
static void Peripheral_Init(void)
{
    // 1. 初始化所有 GPIO
    IoTGpioInit(HI_IO_NAME_GPIO_0);
    IoTGpioInit(HI_IO_NAME_GPIO_1);
    IoTGpioInit(HI_IO_NAME_GPIO_2);
    IoTGpioInit(HI_IO_NAME_GPIO_6);
    IoTGpioInit(HI_IO_NAME_GPIO_7);
    IoTGpioInit(HI_IO_NAME_GPIO_8);
    IoTGpioInit(HI_IO_NAME_GPIO_11);
    IoTGpioInit(HI_IO_NAME_GPIO_12);
    IoTGpioInit(HI_IO_NAME_GPIO_13);
    IoTGpioInit(HI_IO_NAME_GPIO_14);

    // 2. UART2 复用
    hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_UART2_TXD);
    hi_io_set_func(HI_IO_NAME_GPIO_12, HI_IO_FUNC_GPIO_12_UART2_RXD);

    IotUartAttribute uart_attr;
    uart_attr.baudRate = 9600;
    uart_attr.dataBits = IOT_UART_DATA_BIT_8;
    uart_attr.stopBits = IOT_UART_STOP_BIT_1;
    uart_attr.parity = IOT_UART_PARITY_NONE;
    uart_attr.rxBlock = IOT_UART_BLOCK_STATE_BLOCK;
    IoTUartInit(HI_UART_IDX_2, &uart_attr);

    // 3. 风扇（GPIO1，低电平转动）
    hi_io_set_func(HI_IO_NAME_GPIO_1, HI_IO_FUNC_GPIO_1_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_1, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 1);   // 初始关闭

    // 4. RGB灯：R(GPIO2)用于PWM呼吸灯，G(GPIO7)和B(GPIO8)作为继电器指示
    hi_io_set_func(HI_IO_NAME_GPIO_2, HI_IO_FUNC_GPIO_2_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_2, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_2, 0);

    hi_io_set_func(HI_IO_NAME_GPIO_7, HI_IO_FUNC_GPIO_7_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_7, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 0);

    hi_io_set_func(HI_IO_NAME_GPIO_8, HI_IO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_8, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_8, 0);

    // 5. 蜂鸣器（GPIO0，低电平鸣叫）
    hi_io_set_func(HI_IO_NAME_GPIO_0, HI_IO_FUNC_GPIO_0_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_0, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, 1);

    // 6. PWM（GPIO6，呼吸灯）
    hi_io_set_func(HI_IO_NAME_GPIO_6, HI_IO_FUNC_GPIO_6_PWM3_OUT);
    IoTGpioSetDir(HI_IO_NAME_GPIO_6, IOT_GPIO_DIR_OUT);
    IoTPwmInit(HI_PWM_PORT_PWM3);

    // 7. 按键中断（GPIO14=KEY1，GPIO13=KEY2，下降沿触发）
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_14, IOT_GPIO_DIR_IN);
    hi_io_set_pull(HI_IO_NAME_GPIO_14, HI_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(HI_IO_NAME_GPIO_14, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_FALL_LEVEL_LOW, F1_Pressed, NULL);

    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_13, IOT_GPIO_DIR_IN);
    hi_io_set_pull(HI_IO_NAME_GPIO_13, HI_IO_PULL_UP);   // 修正：原来是GPIO14
    IoTGpioRegisterIsrFunc(HI_IO_NAME_GPIO_13, IOT_INT_TYPE_EDGE,
                           IOT_GPIO_EDGE_FALL_LEVEL_LOW, F2_Pressed, NULL);
}

// ========== 任务1：呼吸灯 + 串口自收发（先互斥锁） ==========
static void Stage1_Task1(void)
{
    uint32_t i;
    int ret;
    uint8_t uart_buff_recv[UART_BUFF_SIZE] = {0};
    const char *test_data = "It is UART TX Data!\r\n";

    // 获取互斥锁，模拟占用资源10秒
    osMutexAcquire(mutex_id, osWaitForever);
    sleep(10);
    osMutexRelease(mutex_id);

    // 串口发送测试
    ret = IoTUartWrite(HI_UART_IDX_2, (unsigned char *)test_data, strlen(test_data));
    if (ret <= 0) {
        printf("Uart2 TX Failed, Error[%d]\n", ret);
    } else {
        usleep(10000);
        ret = IoTUartRead(HI_UART_IDX_2, uart_buff_recv, sizeof(uart_buff_recv));
        if (ret > 0) {
            printf("Uart2 RX: [%s]\n", uart_buff_recv);
        } else {
            printf("Uart2 RX Failed, Error[%d]\n", ret);
        }
    }

    // 呼吸灯循环
    while (1) {
        printf("%s is Running!\n", __FUNCTION__);
        for (i = 1; i < 100; i++) {
            IoTPwmStart(HI_PWM_PORT_PWM3, i, 40000);
            usleep(10000);
        }
        sleep(1);
        for (i = 100; i > 0; i--) {
            IoTPwmStart(HI_PWM_PORT_PWM3, i, 40000);
            usleep(10000);
        }
        sleep(1);
    }
}

// ========== 任务2：风扇控制 + 消息发送 ==========
static void Stage1_Task2(void)
{
    MSGQUEUE_OBJ_t_rx Msgq;
    Msgq.Buf = "It is MsgQ Data for testing message queue";

    sleep(3);   // 等待系统稳定

    // 获取互斥锁，开启风扇
    osMutexAcquire(mutex_id, osWaitForever);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 0);   // 低电平，风扇转动
    osMutexRelease(mutex_id);

    while (1) {
        printf("%s is Running!\n", __FUNCTION__);
        if (osMessageQueuePut(mid_MsgQueue, &Msgq, 0U, 0U) != osOK) {
            printf("Message Queue Put failed\n");
        }
        sleep(3);
    }
}

// ========== 任务3：定时器控制蜂鸣器 + 消息接收 ==========
static void Stage1_Task3(void)
{
    TIMER_OBJ_t timer_obj;
    MSGQUEUE_OBJ_t_rx Msgq;
    osStatus_t status;

    // 创建周期性定时器（每10秒鸣叫1秒）
    timer_obj.timerDelay_1 = 1000;   // 1000 ticks = 10秒（1 tick=10ms）
    timer_obj.func = Timer1_Callback;
    timer_obj.type = osTimerPeriodic;
    timer_obj.id = osTimerNew(timer_obj.func, timer_obj.type, NULL, NULL);
    if (timer_obj.id != NULL) {
        if (osTimerStart(timer_obj.id, timer_obj.timerDelay_1) != osOK) {
            printf("Timer start failed\n");
        }
    }

    while (1) {
        printf("%s is Running!\n", __FUNCTION__);
        status = osMessageQueueGet(mid_MsgQueue, &Msgq, NULL, 0U);
        if (status == osOK) {
            printf("Message Queue Get: [%s]\n", Msgq.Buf);
        } else {
            printf("Message Queue Get failed: %d\n", status);
        }
        sleep(3);
    }
}

// ========== 主初始化函数 ==========
static void Experiment_Stage1_Init(void)
{
    // 外设初始化
    Peripheral_Init();

    // 创建消息队列
    mid_MsgQueue = osMessageQueueNew(MSGQUEUE_OBJECTS, sizeof(MSGQUEUE_OBJ_t_rx), NULL);
    if (mid_MsgQueue == NULL) {
        printf("Failed to create Message Queue!\n");
    }

    // 创建互斥锁
    mutex_id = osMutexNew(NULL);
    if (mutex_id == NULL) {
        printf("Failed to create mutex!\n");
    }

    osThreadAttr_t attr = {0};
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;
    attr.priority = 25;

    // 创建任务1
    attr.name = "Stage1_Task1";
    if (osThreadNew((osThreadFunc_t)Stage1_Task1, NULL, &attr) == NULL) {
        printf("Failed to create Stage1_Task1!\n");
    }

    // 创建任务2
    attr.name = "Stage1_Task2";
    if (osThreadNew((osThreadFunc_t)Stage1_Task2, NULL, &attr) == NULL) {
        printf("Failed to create Stage1_Task2!\n");
    }

    // 创建任务3
    attr.name = "Stage1_Task3";
    if (osThreadNew((osThreadFunc_t)Stage1_Task3, NULL, &attr) == NULL) {
        printf("Failed to create Stage1_Task3!\n");
    }
}

APP_FEATURE_INIT(Experiment_Stage1_Init);
