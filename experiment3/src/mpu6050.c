



#include "mpu6050.h"

static hi_u8 mpu6050_data[14] = {0};   
static hi_u8 data_len = 14;          

static hi_void Mpu6050_I2c_Write(hi_u8 addr, hi_u8 data)
{
    hi_u8 data_send_buf[] = {addr, data};
    hi_i2c_data mpu6050_i2c_data = {0};
    mpu6050_i2c_data.send_buf = data_send_buf;
    mpu6050_i2c_data.send_len = 2;
    hi_i2c_write(HI_I2C_IDX_0, 0xD0, &mpu6050_i2c_data);   
}

static hi_void Mpu6050_I2c_Read(hi_u8 addr, hi_u8* data_receive_buf, hi_u32 data_receive_len)
{
    hi_u8 data_send_buf[] = {addr};
    hi_i2c_data mpu6050_i2c_data = {0};
    mpu6050_i2c_data.send_buf = data_send_buf;
    mpu6050_i2c_data.send_len = 1;
    hi_i2c_write(HI_I2C_IDX_0, 0xD0, &mpu6050_i2c_data);   
    mpu6050_i2c_data.receive_buf = data_receive_buf;
    mpu6050_i2c_data.receive_len = data_receive_len;
    hi_i2c_read(HI_I2C_IDX_0, 0xD1, &mpu6050_i2c_data);
}
hi_void Mpu6050_Init(hi_void)
{
    Mpu6050_I2c_Read(0x75, mpu6050_data, 1);   
    if(mpu6050_data[0] == 0x68) 
    {
        Mpu6050_I2c_Write(0x6b, 0x80);  
        hi_udelay(100 * 1000);         
        Mpu6050_I2c_Write(0x6b, 0x00);  
        Mpu6050_I2c_Write(0x1b, 0x18);  
        Mpu6050_I2c_Write(0x1c, 0x00);  
        Mpu6050_I2c_Write(0x19, 0x07);  
        Mpu6050_I2c_Write(0x1a, 0x06);  
        Mpu6050_I2c_Write(0x38, 0x00);  
        Mpu6050_I2c_Write(0x6a, 0x00);  
        Mpu6050_I2c_Write(0x23, 0x00);  
        Mpu6050_I2c_Write(0x37, 0x80);  
        Mpu6050_I2c_Write(0x6c, 0x00);  
    }
    printf("\n car_mpu6050 [Mpu6050_Init] \n");
}
hi_void Mpu6050_Measure_By(hi_u8 *datas, hi_u8 len)
{
    Mpu6050_I2c_Read(0x3b, datas, len);
}

hi_void Mpu6050_Measure_Sh(hi_s16 *datas)
{
    Mpu6050_Measure_By(mpu6050_data, data_len); 
    datas[0] = (hi_s16)mpu6050_data[0] << 8 | mpu6050_data[1];     
    datas[1] = (hi_s16)mpu6050_data[2] << 8 | mpu6050_data[3];      
    datas[2] = (hi_s16)mpu6050_data[4] << 8 | mpu6050_data[5];     
    datas[3] = (hi_s16)mpu6050_data[6] << 8 | mpu6050_data[7];      
    datas[4] = (hi_s16)mpu6050_data[8] << 8 | mpu6050_data[9];      
    datas[5] = (hi_s16)mpu6050_data[10] << 8 | mpu6050_data[11];   
    datas[6] = (hi_s16)mpu6050_data[12] << 8 | mpu6050_data[13];    

}


