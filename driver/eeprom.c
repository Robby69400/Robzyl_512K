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

#include <stddef.h>
#include <string.h>

#include "driver/eeprom.h"
#include "driver/i2c.h"
#include "driver/system.h"

// Obsługa pamięci do 512KB (4Mbit)
// Adres bazowy I2C to 0xA0. Bity adresu powyżej 16 (A16, A17) są mapowane na bity 1 i 2 adresu urządzenia.
// 0x10000 (64KB) -> 0xA2
// 0x20000 (128KB) -> 0xA4
// 0x30000 (192KB) -> 0xA6

void EEPROM_ReadBuffer(uint32_t Address, void *pBuffer, uint16_t Size)
{
	// Oblicz adres urządzenia I2C uwzględniając "strony" 64KB
	// Przesunięcie o 15 bitów mapuje bit 16 adresu na bit 1 adresu I2C (wartość 2)
	uint8_t devAddr = 0xA0 | ((Address >> 15) & 0x0E);

	I2C_Start();
	I2C_Write(devAddr);
	I2C_Write((Address >> 8) & 0xFF);
	I2C_Write((Address >> 0) & 0xFF);
	I2C_Start();
	I2C_Write(devAddr | 1); // Read mode
	I2C_ReadBuffer(pBuffer, Size);
	I2C_Stop();
}

void EEPROM_WriteBuffer(uint32_t Address, const void *pBuffer)
{
	if (pBuffer == NULL) return;
	
	uint8_t buffer[8];
	EEPROM_ReadBuffer(Address, buffer, 8);
	if (memcmp(pBuffer, buffer, 8) != 0)
	{
		uint8_t devAddr = 0xA0 | ((Address >> 15) & 0x0E);

		I2C_Start();
		I2C_Write(devAddr);
		I2C_Write((Address >> 8) & 0xFF);
		I2C_Write((Address >> 0) & 0xFF);
		I2C_WriteBuffer(pBuffer, 8);
		I2C_Stop();
	}

	// give the EEPROM time to burn the data in (apparently takes 5ms)
	SYSTEM_DelayMs(20);
}