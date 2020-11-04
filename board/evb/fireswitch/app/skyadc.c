
#include "rtl876x_rcc.h"
#include "rtl876x_pinmux.h"
#include "rtl876x_adc.h"
#include "bee2_adc_lib.h"
#include "rtl876x_gpio.h"

#include "skyadc.h"
// 转换表，对应bitmap
#define ADC_SCHEDULE_0             	0
#define ADC_SCHEDULE_1            	1
#define ADC_SCHEDULE_2           	2
#define ADC_SCHEDULE_3           	3
#define ADC_SCHEDULE_4           	4
#define ADC_SCHEDULE_5           	5
#define ADC_SCHEDULE_6             	6
#define ADC_SCHEDULE_7             	7
#define ADC_SCHEDULE_8           	8
#define ADC_SCHEDULE_9          	9
#define ADC_SCHEDULE_10       		10
#define ADC_SCHEDULE_11           	11
#define ADC_SCHEDULE_12            	12
#define ADC_SCHEDULE_13            	13
#define ADC_SCHEDULE_14           	14
#define ADC_SCHEDULE_15       		15
// 通道
#define ADC_CHANNEL_4              	4
#define ADC_CHANNEL_5         		5

#define BATT_DETECT					P2_4			// Battery Detect(Adc)
#define ALS_DETECT					P2_5			// Ambient Light Sensor Detect(Adc)
#define ALS_POWER					P3_2			// Ambient Light Sensor Power(IO--O)
#define BATT_POWER					P0_6			// Battery Detect Power(IO--O)	
#define ALS_POWER_PIN 				GPIO_GetPin(ALS_POWER)
#define BATT_POWER_PIN 				GPIO_GetPin(BATT_POWER)
#define ADCPOWER_OPEN               ((BitAction)1)
#define ADCPOWER_CLOSE              ((BitAction)0)


static void Board_ADC_init()
{
    Pad_Config(BATT_DETECT, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
               PAD_OUT_LOW);
    Pad_Config(ALS_DETECT, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_DISABLE,
               PAD_OUT_LOW);
	
    Pinmux_Config(BATT_DETECT, IDLE_MODE);
    Pinmux_Config(ALS_DETECT, IDLE_MODE);
}


static void Driver_ADC_init()
{
    ADC_DeInit(ADC);
    RCC_PeriphClockCmd(APBPeriph_ADC, APBPeriph_ADC_CLOCK, ENABLE);

    ADC_InitTypeDef adcInitStruct;
    ADC_StructInit(&adcInitStruct);
	for(int i=0;i<8;i++)
	{
		adcInitStruct.schIndex[ADC_SCHEDULE_0+i]  = EXT_SINGLE_ENDED(ADC_CHANNEL_4);
		adcInitStruct.schIndex[ADC_SCHEDULE_8+i]  = EXT_SINGLE_ENDED(ADC_CHANNEL_5);    // INTERNAL_VBAT_MODE 内部电压ADC检测 
	}
    adcInitStruct.bitmap              = 0xFFFF;
	adcInitStruct.adcSamplePeriod     = 255;
    ADC_Init(ADC, &adcInitStruct);
    ADC_INTConfig(ADC, ADC_INT_ONE_SHOT_DONE, ENABLE);
	ADC_CalibrationInit();
}


static void Board_ADCPower_init(void)
{
    Pad_Config(ALS_POWER, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_LOW);
	Pinmux_Config(ALS_POWER, DWGPIO);
	
	Pad_Config(BATT_POWER, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE,
               PAD_OUT_LOW);
	Pinmux_Config(BATT_POWER, DWGPIO);
}

static void Driver_ADCPower_init(void)
{
    /* Initialize GPIO peripheral */
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);
	
    GPIO_InitTypeDef GPIO_InitStruct;
	
	// GPIO---Output Init
    GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.GPIO_Pin    = ALS_POWER_PIN | BATT_POWER_PIN; 
    GPIO_InitStruct.GPIO_Mode   = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_ITCmd  = DISABLE;
    GPIO_Init(&GPIO_InitStruct);
    
    GPIO_WriteBit(ALS_POWER_PIN, ADCPOWER_CLOSE);
	GPIO_WriteBit(BATT_POWER_PIN, ADCPOWER_CLOSE);
}


static void SkyAdc_Init(void)
{
	Board_ADC_init();
    Driver_ADC_init();
}


void Sky_ADC_POWER_Init(void)
{
    SkyAdc_Init();
    Board_ADCPower_init();
    Driver_ADCPower_init();
}



static uint16_t calc_avg(uint16_t *arr, uint8_t index, uint8_t arr_len)
{
	if(arr_len <= 2)
		return 0;
    
    uint32_t avg = arr[0];
    uint16_t max = arr[0];
    uint16_t min = arr[0];
	
	for (int i = 1; i < arr_len; i++)
    {
        if (arr[i] > max)
        {
            max = arr[i];
        }
        if (arr[i] < min)
        {
            min = arr[i];
        }
        avg += arr[i];
    }
    avg = (avg - max - min) / (arr_len-2);

	return avg;
}


void HAL_SkyAdc_Sample(uint16_t *bat_dat, uint16_t *lp_dat)
{
	uint16_t sample_data[8];
    uint16_t vol[8];
	uint16_t batt_data[8];
	uint16_t lp_data[8];
	
	GPIO_WriteBit(ALS_POWER_PIN, ADCPOWER_OPEN);
	GPIO_WriteBit(BATT_POWER_PIN, ADCPOWER_OPEN);
	for(int i=0;i<8;i++)
	{
		ADC_ErrorStatus ErrorStatus;
		ADC_Cmd(ADC, ADC_One_Shot_Mode, ENABLE);
		while(ADC_GetIntFlagStatus(ADC, ADC_INT_ONE_SHOT_DONE) == RESET);
		ADC_ClearINTPendingBit(ADC, ADC_INT_ONE_SHOT_DONE);
		for (int j=0;j<8;j++)
        {
            sample_data[j] = ADC_ReadByScheduleIndex(ADC, ADC_SCHEDULE_0+j);
            vol[j] = ADC_GetVoltage(DIVIDE_SINGLE_MODE, sample_data[j], &ErrorStatus);
        }
        batt_data[i] = calc_avg(vol, 0, 8);
		
        for (int j=0;j<8;j++)
        {
            sample_data[j] = ADC_ReadByScheduleIndex(ADC, ADC_SCHEDULE_8+j);
            vol[j] = ADC_GetVoltage(DIVIDE_SINGLE_MODE, sample_data[j], &ErrorStatus);
        }
        lp_data[i] = calc_avg(vol, 1, 8);
	}
	GPIO_WriteBit(ALS_POWER_PIN, ADCPOWER_CLOSE);
	GPIO_WriteBit(BATT_POWER_PIN, ADCPOWER_CLOSE);
	
	*bat_dat = calc_avg(batt_data, 0, 8);
	*lp_dat = calc_avg(lp_data, 1, 8);
}


void HAL_Adc_Dlps_Control(bool isenter)
{
    PAD_OUTPUT_VAL outval;
	
	uint8_t val = 0; 
    val = GPIO_ReadOutputDataBit(ALS_POWER_PIN);
	if(val){
		outval = PAD_OUT_HIGH;
	}else{
		outval = PAD_OUT_LOW;
	}
    
    if(isenter){
		Pad_Config(ALS_POWER, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, outval);
    }else{
        Pad_Config(ALS_POWER, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, outval);	
    }
    
    val = GPIO_ReadOutputDataBit(BATT_POWER_PIN);
	if(val){
		outval = PAD_OUT_HIGH;
	}else{
		outval = PAD_OUT_LOW;
	}
    
    if(isenter){
        Pad_Config(BATT_POWER, PAD_SW_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, outval);
    }else{
        Pad_Config(BATT_POWER, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_NONE, PAD_OUT_ENABLE, outval);
    }
    
}

