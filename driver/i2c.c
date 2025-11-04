/* Original work Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Modified work Copyright 2024 kamilsss655
 * https://github.com/kamilsss655
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "bsp/dp32g030/gpio.h"
#include "bsp/dp32g030/portcon.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/systick.h"

/* Conservative I2C bit-bang timing. Increase if your EEPROM/adapter needs slower clock. */
#ifndef I2C_DELAY_US
#define I2C_DELAY_US 2
#endif

void I2C_Start(void)
{	
	/* Ensure pins are in open-drain/tri-state mode for start */
	PORTCON_PORTA_OD &= ~PORTCON_PORTA_OD_A11_MASK;
	PORTCON_PORTA_IE |= PORTCON_PORTA_IE_A11_BITS_ENABLE;

	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_DelayUs(I2C_DELAY_US);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_DelayUs(I2C_DELAY_US);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_DelayUs(I2C_DELAY_US);
}

void I2C_Stop(void)
{
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_DelayUs(I2C_DELAY_US);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
	SYSTICK_DelayUs(I2C_DELAY_US);
	GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
	SYSTICK_DelayUs(I2C_DELAY_US);
	/* leave pins in open-drain default */
	PORTCON_PORTA_IE &= ~PORTCON_PORTA_IE_A11_MASK;
	PORTCON_PORTA_OD |= PORTCON_PORTA_OD_A11_BITS_ENABLE;
}

uint8_t I2C_Read(bool bFinal) {
  uint8_t i, Data = 0;

  /* Configure SDA as input (tri-state/open-drain), allow reading */
  PORTCON_PORTA_IE |= PORTCON_PORTA_IE_A11_BITS_ENABLE;
  PORTCON_PORTA_OD &= ~PORTCON_PORTA_OD_A11_MASK;
  GPIOA->DIR &= ~GPIO_DIR_11_MASK;

  for (i = 0; i < 8; i++) {
    GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
    SYSTICK_DelayUs(I2C_DELAY_US);
    
    GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
    SYSTICK_DelayUs(I2C_DELAY_US);
    
    Data <<= 1;
    if (GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA)) {
      Data |= 1U;
    }
    
    GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
  }

  /* Restore SDA as output (open-drain) */
  PORTCON_PORTA_IE &= ~PORTCON_PORTA_IE_A11_MASK;
  PORTCON_PORTA_OD |= PORTCON_PORTA_OD_A11_BITS_ENABLE;
  GPIOA->DIR |= GPIO_DIR_11_BITS_OUTPUT;

  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
  /* send ACK/NACK bit (master drives SDA) */
  if (bFinal) {
    GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA); /* NACK: release SDA */
  } else {
    GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA); /* ACK: pull SDA low */
  }

  SYSTICK_DelayUs(I2C_DELAY_US);
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
  SYSTICK_DelayUs(I2C_DELAY_US);
  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);

  return Data;
}

int I2C_Write(uint8_t Data) {
  uint8_t i;
  int ret = -1;
  
  /* ensure SCL low to start sending bits */
  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
  SYSTICK_DelayUs(I2C_DELAY_US);
  
  for (i = 0; i < 8; i++) {
    if (Data & 0x80) GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
    else GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA);
    Data <<= 1;
    
    SYSTICK_DelayUs(I2C_DELAY_US);
    GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
    SYSTICK_DelayUs(I2C_DELAY_US);
    GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
    SYSTICK_DelayUs(I2C_DELAY_US);
  }

  /* Release SDA and switch to input to read ACK from slave */
  PORTCON_PORTA_IE |= PORTCON_PORTA_IE_A11_BITS_ENABLE;
  PORTCON_PORTA_OD &= ~PORTCON_PORTA_OD_A11_MASK;
  GPIOA->DIR &= ~GPIO_DIR_11_MASK;
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA); /* release line */
  
  SYSTICK_DelayUs(I2C_DELAY_US);
  GPIO_SetBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
  SYSTICK_DelayUs(I2C_DELAY_US);
  
  /* wait for ACK (SDA pulled low by slave). Use short intervals to avoid long blocking. */
  for (i = 0; i < 50; i++) { /* 50 * ~I2C_DELAY_US ~ conservative timeout */
    if (!GPIO_CheckBit(&GPIOA->DATA, GPIOA_PIN_I2C_SDA)) {
      ret = 0;
      break;
    }
    SYSTICK_DelayUs(5);
  }

  GPIO_ClearBit(&GPIOA->DATA, GPIOA_PIN_I2C_SCL);
  /* Restore SDA as output open-drain */
  PORTCON_PORTA_IE &= ~PORTCON_PORTA_IE_A11_MASK;
  PORTCON_PORTA_OD |= PORTCON_PORTA_OD_A11_BITS_ENABLE;
  GPIOA->DIR |= GPIO_DIR_11_BITS_OUTPUT;
  
  return ret;
}

int I2C_WriteBuffer(const void *pBuffer, uint8_t Size) {
  if (!pBuffer) return -1;
  
  const uint8_t *pData = (const uint8_t *)pBuffer;
  while (Size--) {
    if (I2C_Write(*pData++) < 0) {
      I2C_Stop(); 
      return -1;
    }
  }
  return 0;
}

int I2C_ReadBuffer(void *pBuffer, uint8_t Size) {
  if (!pBuffer || Size == 0) return -1;
  
  uint8_t *pData = (uint8_t *)pBuffer;
  
  for (uint8_t i = 0; i < Size; i++) {
    pData[i] = I2C_Read(i == Size-1); 
    SYSTICK_DelayUs(I2C_DELAY_US);
  }
  
  return Size;
}