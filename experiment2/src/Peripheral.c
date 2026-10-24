#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "cmsis_os2.h"
#include "Peripheral.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_errno.h"
#include "iot_i2c.h"
#include "hi_i2c.h"
#include "hal_bsp_log.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"
#include "hi_adc.h"
#include "iot_uart.h"
#include "hi_uart.h"
#include "iot_pwm.h"
#include "hi_pwm.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_log.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"
#include "nfc.h"


/***************************************************************
* 函数名称: F1_Pressed
* 说    明: 按键中断
* 参    数: 无
* 返 回 值: 无
***************************************************************/
static void F1_Pressed(char *arg)
{
    (void)arg; 
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 0);//设置GPIO_1输出低电平打开风扇
    printf("KEY1!\n");
}
static void F2_Pressed(char *arg)
{
    (void)arg;
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 1);//设置GPIO_1输出高电平关闭风扇
    printf("KEY2!\n");
}


/***************************************************************
* 函数名称: Peripheral_IO_Init
* 说    明: Peripheral_GPIO初始化
* 参    数: 无
* 返 回 值: 无
***************************************************************/
static void Peripheral_IO_Init(void)
{
    //GPIO功能初始化
    IoTGpioInit(HI_IO_NAME_GPIO_0);
    IoTGpioInit(HI_IO_NAME_GPIO_1);
    IoTGpioInit(HI_IO_NAME_GPIO_2);
    IoTGpioInit(HI_IO_NAME_GPIO_6);
    IoTGpioInit(HI_IO_NAME_GPIO_7);
    IoTGpioInit(HI_IO_NAME_GPIO_8);
    IoTGpioInit(HI_IO_NAME_GPIO_9);
    IoTGpioInit(HI_IO_NAME_GPIO_10);
    IoTGpioInit(HI_IO_NAME_GPIO_11);
    IoTGpioInit(HI_IO_NAME_GPIO_12);
    IoTGpioInit(HI_IO_NAME_GPIO_13);
    IoTGpioInit(HI_IO_NAME_GPIO_14);

    //GPIO_11复用为UART2_TXD
    hi_io_set_func(HI_IO_NAME_GPIO_11, HI_IO_FUNC_GPIO_11_UART2_TXD);
    //GPIO_12复用为UART2_RXD
    hi_io_set_func(HI_IO_NAME_GPIO_12, HI_IO_FUNC_GPIO_12_UART2_RXD);

    //Fan：低电平触发
    hi_io_set_func(HI_IO_NAME_GPIO_1, HI_IO_FUNC_GPIO_1_GPIO);//风扇：低电平触发
    IoTGpioSetDir(HI_IO_NAME_GPIO_1, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 1);

    //R：高电平触发
    hi_io_set_func(HI_IO_NAME_GPIO_2, HI_IO_FUNC_GPIO_2_GPIO);//设置GPIO_2的复用功能为普通GPIO
    IoTGpioSetDir(HI_IO_NAME_GPIO_2, IOT_GPIO_DIR_OUT);    //设置GPIO_2为输出模式
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_2, 0);//设置GPIO_2为输出低电平

    //G：高电平触发
    hi_io_set_func(HI_IO_NAME_GPIO_7, HI_IO_FUNC_GPIO_7_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_7, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 0);

    //B：高电平触发
    hi_io_set_func(HI_IO_NAME_GPIO_8, HI_IO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_8, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_8, 0);

    //蜂鸣器：低电平触发
    hi_io_set_func(HI_IO_NAME_GPIO_0, HI_IO_FUNC_GPIO_0_GPIO);//GPIO功能复用为普通GPIO功能
    IoTGpioSetDir(HI_IO_NAME_GPIO_0, IOT_GPIO_DIR_OUT);//设置GPIO功能为输出
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_0, 1);//设置GPIO为低电平

    hi_io_set_func(HI_IO_NAME_GPIO_6, HI_IO_FUNC_GPIO_6_PWM3_OUT);//设置GPIO_6引脚复用功能为PWM
    IoTGpioSetDir(HI_IO_NAME_GPIO_6, IOT_GPIO_DIR_OUT);//设置GPIO_6引脚为输出模式
    IoTPwmInit(HI_PWM_PORT_PWM3);//初始化PWM端口

    //初始化F1按键，设置为下降沿触发中断
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_GPIO);//设置GPIO功能
    IoTGpioSetDir(HI_IO_NAME_GPIO_14, IOT_GPIO_DIR_IN);//设置GPIO输入
    hi_io_set_pull(HI_IO_NAME_GPIO_14, HI_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(HI_IO_NAME_GPIO_14, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_FALL_LEVEL_LOW, F1_Pressed, NULL);

    //初始化F2按键，设置为下降沿触发中断
    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_GPIO);
    IoTGpioSetDir(HI_IO_NAME_GPIO_13, IOT_GPIO_DIR_IN);
    hi_io_set_pull(HI_IO_NAME_GPIO_14, HI_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(HI_IO_NAME_GPIO_13, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_FALL_LEVEL_LOW, F2_Pressed, NULL);

    //GPIO_10复用为I2C0_SDA
    hi_io_set_func(HI_IO_NAME_GPIO_10, HI_IO_FUNC_GPIO_10_I2C0_SDA);
    //GPIO_9复用为I2C0_SCL
    hi_io_set_func(HI_IO_NAME_GPIO_9, HI_IO_FUNC_GPIO_9_I2C0_SCL);
    hi_i2c_init(HI_I2C_IDX_0, 400000);
    hi_i2c_set_baudrate(HI_I2C_IDX_0, 400000);

}


/***************************************************************
* 函数名称: Peripheral_Init
* 说    明: 初始化Peripheral
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void Peripheral_Init(void)
{
    Peripheral_IO_Init();//外设IO口初始化

    //串口硬件初始化
    IotUartAttribute uart_attr;
    uart_attr.baudRate = 115200;//波特率: 115200
    uart_attr.dataBits = IOT_UART_DATA_BIT_8;//数据位: 8bits
    uart_attr.stopBits = IOT_UART_STOP_BIT_1;//停止位：1bit
    uart_attr.parity = IOT_UART_PARITY_NONE;//校验位：None
    uart_attr.rxBlock = IOT_UART_BLOCK_STATE_BLOCK;
    uart_attr.rxBlock = IOT_UART_BLOCK_STATE_BLOCK;
    IoTUartInit(HI_UART_IDX_2, &uart_attr);
    
    SSD1306_Init(); // OLED 显示屏初始化
    SHT20_Init(); // SHT20初始化
    AP3216C_Init(); // 三合一传感器初始化
}

/***************************************************************
* 函数名称: Light_StatusSet
* 说    明: 灯状态设置
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Light_StatusSet(Peripheral_Status_ENUM status)
{
    if (status == ON)
        //设置GPIO_6输出高电平点亮灯
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_6, 1);
    if (status == OFF)
        //设置GPIO_6输出低电平关闭灯
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_6, 0);
}

/***************************************************************
* 函数名称: Motor_StatusSet
* 说    明: 电机状态设置
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Motor_StatusSet_Fan(Peripheral_Status_ENUM status)
{
    if (status == ON)
        //设置GPIO_1输出低电平打开电机
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 0);

    if (status == OFF)
        //设置GPIO_1输出高电平关闭电机
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_1, 1);
}


/***************************************************************
* 函数名称: Motor_StatusSet
* 说    明: 电机状态设置
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Motor_StatusSet_Water(Peripheral_Status_ENUM status)
{
    if (status == ON)
        //设置GPIO_5输出高电平打开电机
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_5, 1);
    if (status == OFF)
        //设置GPIO_5输出低电平关闭电机
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_5, 0);
}


/***************************************************************
* 函数名称: Motor_StatusSet_RGB_R
* 说    明: RGB_R状态控制
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Motor_StatusSet_RGB_R(Peripheral_Status_ENUM status)
{
    if (status == ON)
        //设置GPIO_2输出高电平
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_2, 1);
    if (status == OFF)
        //设置GPIO_2输出低电平
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_2, 0);
}


/***************************************************************
* 函数名称: Motor_StatusSet_RGB_G
* 说    明: RGB_G状态控制
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Motor_StatusSet_RGB_G(Peripheral_Status_ENUM status)
{
    if (status == ON)
        //设置GPIO_7输出高电平
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 1);
    if (status == OFF)
        //设置GPIO_7输出低电平
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 0);
}

/***************************************************************
* 函数名称: Motor_StatusSet_RGB_B
* 说    明: RGB_B状态控制
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Motor_StatusSet_RGB_B(Peripheral_Status_ENUM status)
{
    if (status == ON)
        //设置GPIO_B输出高电平打开
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_8, 1);
    if (status == OFF)
        //设置GPIO_B输出低电平关闭
        IoTGpioSetOutputVal(HI_IO_NAME_GPIO_8, 0);
}
double density, voltage;
int adcvalue;
#define  COV_RATIO 0.03          
#define  NO_DUST_VOLTAGE 230            
#define  SYS_VOLTAGE 7200 

/***** 获取电压值函数 *****/
unsigned int GetVoltage(void)
{
    unsigned int ret;
    unsigned short data;
    ret = hi_adc_read(HI_ADC_CHANNEL_2, &data, HI_ADC_EQU_MODEL_8, HI_ADC_CUR_BAIS_DEFAULT, 0xff);//adc读取
    if (ret != IOT_SUCCESS)
        printf("ADC Read Fail\n");//判断是否ADC读取成功
    return data;
}

//递推平均滤波算法
double Filter(int m)
{
    static int flag_first = 0, _buff[10], sum;
    const int _buff_max = 10;
    int i;
    if (flag_first == 0)
    {
        flag_first = 1;
        for (i = 0, sum = 0; i < _buff_max; i++)
        {
            _buff[i] = m;
            sum += _buff[i];
        }
    }
    else
    {
        sum -= _buff[0];
        for (i = 0; i < (_buff_max - 1); i++)
        {
            _buff[i] = _buff[i + 1];
        }
        _buff[9] = m;
        sum += _buff[9];
    }
    return  sum/10.0;
}

double GetGP2Y(void)
{
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 1);//设置GPIO_7为输出低电平
    usleep(280);
	adcvalue = GetVoltage();
    IoTGpioSetOutputVal(HI_IO_NAME_GPIO_7, 0);//设置GPIO_7为输出低电平
	adcvalue = Filter(adcvalue);
	voltage = (SYS_VOLTAGE / 4096.0) * adcvalue * 11;
	if(voltage >= NO_DUST_VOLTAGE)
    {
        voltage -= NO_DUST_VOLTAGE;
        density = voltage * COV_RATIO;
    }
    else
        density = 0;
    return density;
 }