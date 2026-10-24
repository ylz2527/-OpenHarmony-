#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifi_connect.h"
#include "lwip/sockets.h"
#include "oc_mqtt.h"
#include "Peripheral.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_log.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"
#include "nfc.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_errno.h"
#include "iot_i2c.h"
#include "hi_i2c.h"
#include "iot_pwm.h"
#include "hi_pwm.h"
#include "iot_uart.h"
#include "hi_uart.h"
#include "cJSON.h"
#include "iot_flash.h"
#include "hal_bsp_nfc.h"
#include "config.h"
#include "mpu6050.h"

// ========== 宏定义 ==========
#define MSGQUEUE_OBJECTS 16
#define UART_BUFF_SIZE 1000

// ========== 华为云三元组 ==========
#define CLIENT_ID   "6a55ccee7f2e6c302f85004e_20260714_0_1_2026071406"
#define USERNAME    "6a55ccee7f2e6c302f85004e_20260714"
#define PASSWORD    "6bd1e93880750f17e45a6034817a88830541aaba5c08490a581228f4b7335e8a"

// ========== 全局变量 ==========
uint8_t uart_buff[UART_BUFF_SIZE] = {0};
uint8_t *uart_buff_ptr = uart_buff;

uint8_t displayBuff_0[20] = {0};
uint8_t displayBuff_1[20] = {0};
uint8_t displayBuff_2[20] = {0};
uint8_t displayBuff_3[20] = {0};
uint8_t displayBuff_4[20] = {0};

uint8_t ssd[20] = {0};
uint8_t secret[20] = {0};
uint8_t Keep_Ssd_Flag = 0;
uint8_t Keep_Sec_Flag = 0;
uint8_t Keep_Ssd_long = 0;
uint8_t Keep_Sec_long = 0;

typedef struct {
    char *Buf;
    uint8_t Idx;
} MSGQUEUE_OBJ_t;

typedef enum {
    en_msg_cmd = 0,
    en_msg_report,
} en_msg_type_t;

typedef struct {
    char *request_id;
    char *payload;
} cmd_t;

typedef struct {
    int lum;
    int temp;
    int hum;
    char Longitude[10];
    char Latitude[9];
} report_t;

typedef struct {
    en_msg_type_t msg_type;
    union {
        cmd_t cmd;
        report_t report;
    } msg;
} app_msg_t;

typedef struct {
    int connected;
    int led;
    int motor;
} app_cb_t;

typedef struct {
    volatile uint16_t latitude;
    volatile uint16_t latitude_Min;
    volatile uint16_t latitude_Sec;
    volatile uint16_t longitude;
    volatile uint16_t longitude_Min;
    volatile uint16_t longitude_Sec;
    uint8_t flags;
    uint8_t Northern;
    uint8_t East;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} GPS_INFO;
GPS_INFO GPS_Store;

static app_cb_t g_app_cb;
MSGQUEUE_OBJ_t msg;
osMessageQueueId_t mid_MsgQueue;
float temperature = 0, humidity = 0;
uint16_t ir = 0, als = 0, ps = 0;
osMutexId_t mutex_id;
app_msg_t *app_msg_down = NULL;
app_msg_t *app_msg_up = NULL;
char *app_msg_request_id = NULL;

uint32_t exec1;
uint32_t timerDelay_1;
osTimerId_t id1;

// ========== 函数声明 ==========
static void deal_report_msg(report_t *report);
static int oc_cmd_get_request_id_len(uint8_t *topic_data, size_t topic_size);
static void oc_cmd_rsp_cb(uint8_t *topic_data, size_t topic_size, uint8_t *recv_data, size_t recv_size, uint8_t **resp_data, size_t *resp_size);
static void deal_cmd_msg(cmd_t *cmd);
static void Timer1_Callback(void);
static void GPS_Parameters(void);
static void Task_NFC(void);

// ========== 引用的函数（MQTT处理） ==========
static void deal_report_msg(report_t *report)
{
    oc_mqtt_profile_service_t service;
    oc_mqtt_profile_kv_t temperature;
    oc_mqtt_profile_kv_t humidity;
    oc_mqtt_profile_kv_t motor;
    oc_mqtt_profile_kv_t Longitude_value;
    oc_mqtt_profile_kv_t Latitude_value;

    service.event_time = NULL;
    service.service_id = "Environment";
    service.service_property = &temperature;
    service.nxt = NULL;
    
    temperature.key = "Temperature";
    temperature.value = &report->temp;
    temperature.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    temperature.nxt = &humidity;
    
    humidity.key = "Humidity";
    humidity.value = &report->hum;
    humidity.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    humidity.nxt = &Longitude_value;
    
    Longitude_value.key = "Longitude";
    Longitude_value.value = &report->Latitude;
    Longitude_value.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    Longitude_value.nxt = &Latitude_value;
    
    Latitude_value.key = "Latitude";
    Latitude_value.value = &report->Longitude;
    Latitude_value.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    Latitude_value.nxt = &motor;
    
    motor.key = "MotorStatus";
    motor.value = g_app_cb.motor ? "ON" : "OFF";
    motor.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    motor.nxt = NULL;
    
    oc_mqtt_profile_propertyreport(USERNAME, &service);
}

int oc_cmd_get_request_id_len(uint8_t *topic_data, size_t topic_size)
{
    uint8_t *p = strchr((char *)topic_data, '=');
    if (p != NULL) {
        return (topic_size - (p - topic_data) - 1);
    }
    return 0;
}

void oc_cmd_rsp_cb(uint8_t *topic_data, size_t topic_size, uint8_t *recv_data,
                   size_t recv_size, uint8_t **resp_data, size_t *resp_size)
{
    int ret = 0;
    int len = 0;

    len = oc_cmd_get_request_id_len(topic_data, topic_size);
    app_msg_request_id = malloc(len + 1);
    memset(app_msg_request_id, 0x00, len + 1);
    memcpy(app_msg_request_id, topic_data + topic_size - len, len);

    app_msg_down = malloc(sizeof(app_msg_t));
    app_msg_down->msg_type = en_msg_cmd;
    app_msg_down->msg.cmd.payload = (char *)recv_data;
    app_msg_down->msg.cmd.request_id = app_msg_request_id;

    // 发送到消息队列给主任务处理
    ret = osMessageQueuePut(mid_MsgQueue, &app_msg_down, 0U, 0U);
    if (ret != 0) {
        free(app_msg_down);
        free(app_msg_request_id);
    }

    *resp_data = NULL;
    *resp_size = 0;
}

static void deal_cmd_msg(cmd_t *cmd)
{
    cJSON *obj_root;
    cJSON *obj_cmdname;
    cJSON *obj_paras;
    cJSON *obj_para;
    int cmdret = 1;
    oc_mqtt_profile_cmdresp_t cmdresp;

    obj_root = cJSON_Parse(cmd->payload);
    if (NULL == obj_root) {
        cmdret = -1;
        goto EXIT_JSONPARSE;
    }

    obj_cmdname = cJSON_GetObjectItem(obj_root, "command_name");
    if (NULL == obj_cmdname) {
        cmdret = -2;
        goto EXIT_CMDOBJ;
    }

    if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Control_Valves")) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras) {
            cmdret = -3;
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
        if (NULL == obj_para) {
            cmdret = -4;
            goto EXIT_OBJPARA;
        }
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON")) {
            g_app_cb.motor = 1;
            Motor_StatusSet_RGB_R(ON);
            printf("RGB Red On\n");
        } else {
            g_app_cb.motor = 0;
            Motor_StatusSet_RGB_R(OFF);
            printf("RGB Red Off\n");
        }
    } else if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Control_G")) {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras) {
            cmdret = -3;
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "RGB_G");
        if (NULL == obj_para) {
            cmdret = -4;
            goto EXIT_OBJPARA;
        }
        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON")) {
            Motor_StatusSet_RGB_G(ON);
            printf("RGB Green On\n");
        } else {
            Motor_StatusSet_RGB_G(OFF);
            printf("RGB Green Off\n");
        }
    }

    cmdret = 0;

EXIT_OBJPARA:
EXIT_OBJPARAS:
EXIT_CMDOBJ:
    cJSON_Delete(obj_root);
EXIT_JSONPARSE:
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = "Control_Valves";
    oc_mqtt_profile_cmdresp(USERNAME, &cmdresp);
}

// ========== 定时器回调 ==========
void Timer1_Callback()
{
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, IOT_GPIO_VALUE1); // 关闭蜂鸣器
    osTimerStop(id1);
}

// ========== GPS 解析函数 ==========
void GPS_Parameters()
{
    IoTUartRead(HI_UART_IDX_2, uart_buff_ptr, UART_BUFF_SIZE);
    // 查找 GPRMC 语句
    if (strstr((char *)uart_buff_ptr, "GPRMC") == NULL) {
        memset(uart_buff_ptr, 0, UART_BUFF_SIZE);
        return;
    }

    // 解析时间（示例简单实现）
    char *p = strstr((char *)uart_buff_ptr, "GPRMC");
    if (p != NULL) {
        // 定位状态
        if (p[17] == 'A') {
            GPS_Store.flags = 1;
        } else {
            GPS_Store.flags = 0;
        }
        // 纬度解析（示例，实际需更严谨）
        // 此处仅作演示，详细解析可参考任务11
        // ...
    }
    memset(uart_buff_ptr, 0, UART_BUFF_SIZE);
}

// ========== 任务6：NFC 读取 ==========
void Task_NFC(void)
{
    osMutexAcquire(mutex_id, osWaitForever);

    uint8_t ndefLen = 0;
    uint8_t ndef_Header = 0;

    nfc_Init();

    if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) == 0) {
        printf("Get Header Err\n");
        osMutexRelease(mutex_id);
        return;
    }

    ndefLen += NDEF_HEADER_SIZE;
    printf("ndefLen=%d\n", ndefLen);

    uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
    if (ndefBuff == NULL) {
        printf("Get Buff Err\n");
        osMutexRelease(mutex_id);
        return;
    }

    // 获取 NDEF 数据包
    if (get_NDEFDataPackage(ndefBuff, ndefLen) != 0) {
        printf("get_NDEFDataPackage failed\n");
        free(ndefBuff);
        osMutexRelease(mutex_id);
        return;
    }

    // 查找 'n' 和 ',' 分隔符
    for (size_t i = 0; i < ndefLen; i++) {
        if (0x6e == ndefBuff[i]) {
            Keep_Ssd_Flag = i + 1;
            break;
        }
    }
    for (size_t i = 0; i < ndefLen; i++) {
        if (0x2c == ndefBuff[i]) {
            Keep_Sec_Flag = i + 1;
            break;
        }
    }

    Keep_Ssd_long = Keep_Sec_Flag - Keep_Ssd_Flag - 1;
    Keep_Sec_long = ndefLen - Keep_Sec_Flag;

    for (size_t i = 0; i < Keep_Ssd_long; i++) {
        ssd[i] = ndefBuff[i + Keep_Ssd_Flag];
    }
    for (size_t i = 0; i < Keep_Sec_long; i++) {
        secret[i] = ndefBuff[i + Keep_Sec_Flag];
    }
    free(ndefBuff);

    printf("SSID:%s\r\n", ssd);
    printf("PWD :%s\r\n", secret);

    osMutexRelease(mutex_id);
}

// ========== 任务1：主任务（WiFi + MQTT + 消息处理） ==========
static int Task_Main(void)
{
    app_msg_t *app_msg;

    sleep(3);
    osMutexAcquire(mutex_id, osWaitForever);

    // WiFi 连接（使用 NFC 读取的 SSID 和密码）
    if (WifiConnect("realme GT 7 Pro 8DC8", "12345678") == 0) {
        sprintf((char *)displayBuff_4, " WIFI CONNECT!  ");
    } else {
        sprintf((char *)displayBuff_4, " WIFI DISCONNECT");
    }

    SSD1306_CLS();
    SSD1306_ShowStr(0, 2, (uint8_t *)displayBuff_4, 16);
    sleep(1);
    SSD1306_CLS();

    device_info_init(CLIENT_ID, USERNAME, PASSWORD);
    if (oc_mqtt_init() != 0) {
        printf("Mqtt conn failed\n");
    }
    oc_set_cmd_rsp_cb(oc_cmd_rsp_cb);

    sleep(3);
    osMutexRelease(mutex_id);
    SSD1306_CLS();

    while (1) {
        app_msg = NULL;
        if (osMessageQueueGet(mid_MsgQueue, (void **)&app_msg, NULL, osWaitForever) == 0) {
            switch (app_msg->msg_type) {
                case en_msg_cmd:
                    deal_cmd_msg(&app_msg->msg.cmd);
                    free(app_msg_down);
                    free(app_msg_request_id);
                    break;
                case en_msg_report:
                    deal_report_msg(&app_msg->msg.report);
                    break;
                default:
                    break;
            }
        }
    }
    return 0;
}

// ========== 任务2：传感器采集（温湿度+光照） ==========
static int Task_Sensor(void)
{
    app_msg_up = malloc(sizeof(app_msg_t));
    while (1) {
        // 读取传感器
        SHT20_ReadData(&temperature, &humidity);
        AP3216C_ReadData(&ir, &als, &ps);

        // 填充上报消息
        app_msg_up->msg_type = en_msg_report;
        app_msg_up->msg.report.temp = (int)temperature;
        app_msg_up->msg.report.hum = (int)humidity;
        app_msg_up->msg.report.lum = ps;

        // 发送到消息队列
        osMessageQueuePut(mid_MsgQueue, &app_msg_up, 0U, 0U);

        // 同时更新显示缓冲区
        snprintf((char *)displayBuff_0, sizeof(displayBuff_0), "T:%.1f C  H:%.1f%%", temperature, humidity);
        snprintf((char *)displayBuff_1, sizeof(displayBuff_1), "ALS:%d  PS:%d", als, ps);

        sleep(10); // 10秒上报周期
    }
    return 0;
}

// ========== 任务3：MPU6050 加速度检测 ==========
static void Task_Mpu(void)
{
    sleep(10);
    Mpu6050_Init();
    hi_s16 mpu_dates[7];
    hi_u8 mpu_ds[14] = {0};
    hi_u8 mpu_ln = 14;
    uint8_t mpu_keep_flag = 0;

    exec1 = 1U;
    id1 = osTimerNew(Timer1_Callback, osTimerPeriodic, &exec1, NULL);
    if (id1 != NULL) {
        timerDelay_1 = 300U;
    }

    while (1) {
        Mpu6050_Measure_By(mpu_ds, mpu_ln);
        Mpu6050_Measure_Sh(mpu_dates);

        // 检测静止状态：加速度 X,Y,Z 接近0（静止时Z约16384，此处简化为判断变化量）
        // 如果静止，启动定时器鸣叫蜂鸣器
        if (abs(mpu_dates[0]) < 100 && abs(mpu_dates[1]) < 100 && abs(mpu_dates[2] - 16384) < 500) {
            mpu_keep_flag++;
            if (mpu_keep_flag >= 5) { // 连续5次检测到静止
                // 开启蜂鸣器（低电平鸣叫）
                IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, IOT_GPIO_VALUE0);
                osTimerStart(id1, timerDelay_1); // 3秒后自动关闭
                mpu_keep_flag = 0;
            }
        } else {
            mpu_keep_flag = 0;
        }
        sleep(1);
    }
}

// ========== 任务4：GPS 数据采集 ==========
static void Task_GPS(void)
{
    while (1) {
        GPS_Parameters();
        osDelay(100);
    }
}

// ========== 任务5：OLED 显示与蜂鸣器初始鸣叫 ==========
static void Task_Disp(void)
{
    // 开机蜂鸣器鸣叫一声
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, IOT_GPIO_VALUE0);
    osDelay(100);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, IOT_GPIO_VALUE1);

    while (1) {
        // 循环显示传感器数据
        SSD1306_CLS();
        SSD1306_ShowStr(0, 0, (uint8_t *)displayBuff_0, 16);
        SSD1306_ShowStr(0, 2, (uint8_t *)displayBuff_1, 16);
        SSD1306_ShowStr(0, 4, (uint8_t *)displayBuff_4, 16);
        // 可显示更多信息
        osDelay(2000); // 2秒刷新一次
    }
}

// ========== 初始化入口 ==========
static void Experiment_Stage3(void)
{
    mid_MsgQueue = osMessageQueueNew(MSGQUEUE_OBJECTS, sizeof(app_msg_t), NULL);
    if (mid_MsgQueue == NULL) {
        printf("MQ Err\n");
    }

    Peripheral_Init();
    SSD1306_CLS();
    nfc_Init();

    // 创建互斥锁
    mutex_id = osMutexNew(NULL);
    if (mutex_id == NULL) {
        printf("Mutex Err\n");
    }

    osThreadAttr_t attr = {0};
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;

    // 任务1：主任务（WiFi + MQTT）
    attr.name = "Task_Main";
    attr.stack_size = 10240;
    attr.priority = 23;
    if (osThreadNew((osThreadFunc_t)Task_Main, NULL, &attr) == NULL) {
        printf("Task_Main Err\n");
    }

    // 任务2：传感器采集
    attr.name = "Task_Sensor";
    attr.stack_size = 4096;
    attr.priority = 24;
    if (osThreadNew((osThreadFunc_t)Task_Sensor, NULL, &attr) == NULL) {
        printf("Task_Sensor Err\n");
    }

    // 任务3：MPU6050
    attr.name = "Task_Mpu";
    attr.stack_size = 2048;
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)Task_Mpu, NULL, &attr) == NULL) {
        printf("Task_Mpu Err\n");
    }

    // 任务4：GPS
    attr.name = "Task_GPS";
    attr.stack_size = 2048;
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)Task_GPS, NULL, &attr) == NULL) {
        printf("Task_GPS Err\n");
    }

    // 任务5：OLED显示
    attr.name = "Task_Disp";
    attr.stack_size = 2048;
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)Task_Disp, NULL, &attr) == NULL) {
        printf("Task_Disp Err\n");
    }

    // 任务6：NFC读取
    attr.name = "Task_NFC";
    attr.stack_size = 2048;
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)Task_NFC, NULL, &attr) == NULL) {
        printf("Task_NFC Err\n");
    }
}

APP_FEATURE_INIT(Experiment_Stage3);
