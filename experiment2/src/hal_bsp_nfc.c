#include "hal_bsp_nfc.h"
#include "hal_bsp_log.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "iot_gpio.h"
#include "hi_io.h"
#include "iot_errno.h"
#include "hi_errno.h"
#include "iot_i2c.h"
#include "hi_i2c.h"


/**
 * @brief  从Page页中组成NDEF协议的包裹
 * @note   
 * @param  *dataBuff: 最终的内容
 * @param  dataBuff_MaxSize: 存储缓冲区的长度
 * @retval 
 */
uint32_t get_NDEFDataPackage(uint8_t *dataBuff, const uint16_t dataBuff_MaxSize)
{
    if(dataBuff == NULL || dataBuff_MaxSize <= 0)
    {
        console_log_error("dataBuff==NULL or dataBuff_MaxSize<=0");
        return HI_ERR_FAILURE;
    }

    uint8_t userMemoryPageNum = 0; // 用户的数据操作页数

    // 算出要取多少页
    if (dataBuff_MaxSize <= NFC_PAGE_SIZE)
        userMemoryPageNum = 1; // 1页
    else
        userMemoryPageNum = (dataBuff_MaxSize / NFC_PAGE_SIZE) + ((dataBuff_MaxSize % NFC_PAGE_SIZE) > 0 ? 1 : 0); // 需要访问多少页

    // 内存拷贝
    uint8_t *p_buff = (uint8_t *)malloc(userMemoryPageNum * NFC_PAGE_SIZE);
    if (p_buff == NULL)
    {
        console_log_error("p_buff == NULL.");
        return HI_ERR_FAILURE;
    }

    // 读取数据
    for (int i = 0; i < userMemoryPageNum; i++)
    {
        if (NT3HReadUserData(i) == true)
        {
            memcpy(p_buff + i * NFC_PAGE_SIZE, nfcPageBuffer, NFC_PAGE_SIZE);
        }
    }
    
    memcpy(dataBuff, p_buff, dataBuff_MaxSize);
    
    free(p_buff);
    p_buff = NULL;

    return HI_ERR_SUCCESS;
}


/**
 * @brief  NFC引脚初始化
 * @note   
 * @retval 
 */
uint32_t nfc_Init(void)
{
    uint32_t result;

    IoTGpioInit(HI_IO_NAME_GPIO_9);
    IoTGpioInit(HI_IO_NAME_GPIO_10);

    //GPIO_10复用为I2C0_SDA
    hi_io_set_func(HI_IO_NAME_GPIO_10, HI_IO_FUNC_GPIO_10_I2C0_SDA);

    //GPIO_9复用为I2C0_SCL
    hi_io_set_func(HI_IO_NAME_GPIO_9, HI_IO_FUNC_GPIO_9_I2C0_SCL);

    //baudrate: 400kbps
    result = hi_i2c_init(HI_I2C_IDX_0, 400000);
    result = hi_i2c_set_baudrate(HI_I2C_IDX_0, 400000);


    if(result != HI_ERR_SUCCESS)
    {
        console_log_error("I2C nfc Init status is 0x%x!!!", result);
        return result;
    }
    console_log_success("I2C nfc Init is succeeded!!!");

    return true;
}