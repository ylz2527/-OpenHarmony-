#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
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
#include "hi_adc.h"
#include "hal_bsp_nfc.h"
#include "config.h"
#include "mpu6050.h"

// ========== 全局变量 ==========
uint8_t displayBuff_0[20] = {0};
uint8_t displayBuff_1[20] = {0};
uint8_t displayBuff_2[20] = {0};
uint8_t displayBuff_3[20] = {0};

osSemaphoreId_t sem1;
float temperature = 0, humidity = 0;
uint16_t ir = 0, als = 0, ps = 0;
hi_s16 mpu_dates[7];
hi_u8 mpu_ds[14] = {0};
hi_u8 mpu_ln = 14;
#define IoT_KEY2_GPIO_13  13

// ========== 获取电压 ==========
float Get_Voltage(void)
{
    unsigned int ret;
    unsigned short data;
    ret = hi_adc_read(HI_ADC_CHANNEL_6, &data, HI_ADC_EQU_MODEL_8,
                      HI_ADC_CUR_BAIS_DEFAULT, 0xff);
    if (ret != 0) return -1;
    return (float)data * 1.8 * 4 / 4096.0;
}

// ========== 任务1：采集传感器数据 ==========
static void Stage2_Task1(void)
{
    float Voltage;
    Peripheral_Init();
    hi_io_set_pull(IoT_KEY2_GPIO_13, HI_IO_PULL_UP);
    SSD1306_CLS();
    Mpu6050_Init();

    while (1) {
        osSemaphoreRelease(sem1);
        osSemaphoreRelease(sem1);
        SHT20_ReadData(&temperature, &humidity);
        AP3216C_ReadData(&ir, &als, &ps);
        Mpu6050_Measure_By(mpu_ds, mpu_ln);
        Mpu6050_Measure_Sh(mpu_dates);
        Voltage = Get_Voltage();
        (void)Voltage; // 暂不处理
        sleep(2);
    }
}


// ======================== 任务2：OLED 显示 ========================
static void Stage2_Task2(void)
{
    sleep(3);
    SSD1306_CLS();

    while (1) {
        osSemaphoreAcquire(sem1, osWaitForever);
        SSD1306_CLS();

        snprintf((char *)displayBuff_0, sizeof(displayBuff_0),
                 "T:%.1fC H:%.1f%%", temperature, humidity);
        SSD1306_ShowStr(0, 2, (uint8_t *)displayBuff_0, 16);

        snprintf((char *)displayBuff_1, sizeof(displayBuff_1),
                 "ALS:%4d V:%.2fV", als, Get_Voltage());
        SSD1306_ShowStr(0, 4, (uint8_t *)displayBuff_1, 16);

        snprintf((char *)displayBuff_2, sizeof(displayBuff_2),
                 "AccX:%5d", mpu_dates[0]);
        SSD1306_ShowStr(0, 6, (uint8_t *)displayBuff_2, 16);

        printf("T=%.1f H=%.1f ALS=%d V=%.2f AccX=%d AccY=%d AccZ=%d GyroX=%d GyroY=%d GyroZ=%d\n",
               temperature, humidity, als, Get_Voltage(),
               mpu_dates[0], mpu_dates[1], mpu_dates[2],
               mpu_dates[3], mpu_dates[4], mpu_dates[5]);

        sleep(1);
    }
}
// ========== 任务3：NFC 读取 ==========
static void Stage2_Task3(void)
{
    uint8_t ndefLen = 0, ndef_Header = 0;
    uint8_t ssd[20] = {0}, secret[20] = {0};
    uint8_t Keep_Ssd_Flag = 0, Keep_Sec_Flag = 0;
    uint8_t Keep_Ssd_long = 0, Keep_Sec_long = 0;

    sleep(1);
    if (nfc_Init() != true) {
        printf("nfc_Init Failed\n");
        return;
    }

    while (1) {
        osSemaphoreAcquire(sem1, osWaitForever);
        if (NT3HReadHeaderNfc(&ndefLen, &ndef_Header) == 0) {
            continue;
        }
        ndefLen += NDEF_HEADER_SIZE;
        uint8_t *ndefBuff = (uint8_t *)malloc(ndefLen + 1);
        if (ndefBuff == NULL) continue;
        if (get_NDEFDataPackage(ndefBuff, ndefLen) != 0) {
            free(ndefBuff);
            continue;
        }
        for (size_t i = 0; i < ndefLen; i++) {
            if (0x6e == ndefBuff[i]) { Keep_Ssd_Flag = i + 1; break; }
        }
        for (size_t i = 0; i < ndefLen; i++) {
            if (0x2c == ndefBuff[i]) { Keep_Sec_Flag = i + 1; break; }
        }
        Keep_Ssd_long = Keep_Sec_Flag - Keep_Ssd_Flag - 1;
        Keep_Sec_long = ndefLen - Keep_Sec_Flag;
        for (size_t i = 0; i < Keep_Ssd_long && i < 19; i++)
            ssd[i] = ndefBuff[i + Keep_Ssd_Flag];
        ssd[Keep_Ssd_long] = '\0';
        for (size_t i = 0; i < Keep_Sec_long && i < 19; i++)
            secret[i] = ndefBuff[i + Keep_Sec_Flag];
        secret[Keep_Sec_long] = '\0';
        printf("NFC SSID: %s\n", ssd);
        printf("NFC PWD : %s\n", secret);
        free(ndefBuff);
        sleep(3);
    }
}

// ========== 任务创建 ==========
static void Experiment_Stage2(void)
{
    osThreadAttr_t attr;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 4;

    attr.name = "Stage2_Task1";
    attr.priority = 25;
    osThreadNew((osThreadFunc_t)Stage2_Task1, NULL, &attr);

    attr.name = "Stage2_Task2";
    attr.priority = 25;
    osThreadNew((osThreadFunc_t)Stage2_Task2, NULL, &attr);

    attr.name = "Stage2_Task3";
    attr.priority = 25;
    osThreadNew((osThreadFunc_t)Stage2_Task3, NULL, &attr);

    sem1 = osSemaphoreNew(4, 0, NULL);
    if (sem1 == NULL) printf("Failed to create Semaphore1!\n");
}

APP_FEATURE_INIT(Experiment_Stage2);