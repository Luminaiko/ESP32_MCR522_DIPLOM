/*

-----------------
 MFRC522 | ESP32
-----------------
     SDA | 21
     SCK | 18
    MOSI | 23
    MISO | 19
     IRQ | N/A
     GND | GND
     RST | 22
    3.3V | 3.3V
*/

#include <SPI.h>
#include <Arduino.h>
#include <MFRC522.h>  	//для работы с rfid метками
#include <EEPROM.h>	//для работы с EEPROM
#include <WiFi.h>		//Для подключения к wifi
#include <Thread.h>

#define RST_PIN 22
#define SS_PIN 21

#define UidFreeAdress 200 //Адрес ячейки для хранения свободного адреса для записи 

uint32_t TimerOnShowTags, myTimer2, myTimer3;

int MaxRFIDTags = 50;	//Максимальное количество RFID меток
int maxAvailableAdress = MaxRFIDTags * 4;	//Максимальный адрес занимаемый метками

int deleteAdress; //Начальный адрес для удаления метки если такая есть

unsigned long uidDec, uidDecTemp; // для хранения номера метки в десятичном формате
unsigned long uidAdmin = 3544981781; // Номер метки админа

byte EEPROMstartAddr = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN); // Создание обьекта RFID

unsigned long EEPROMReadUID(byte address) //Чтение карты из EEPROM
{
	long four = EEPROM.read(address);
	long three = EEPROM.read(address + 1);
	long two = EEPROM.read(address + 2);
	long one = EEPROM.read(address + 3);
	
	return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

bool FindingTagsInEEPROM(unsigned long uidDec) //Нахождение метки в массиве EEPROM
{
	for (int i = 0; i < maxAvailableAdress; i+=4)
	{
		Serial.print("Итерация цикла проверки = ");
		Serial.println(i);
		if (uidDec == EEPROMReadUID(i))
		{
			deleteAdress = i;
			Serial.println("Метка есть в базе");
			return true;
		}
	}
	Serial.println("Метки нет в базе");
	return false;
}

void EEPROMWriteUID(unsigned long value) //запись карты в EEPROM
{
		byte four = (value & 0xFF);
		byte three = ((value >> 8) & 0xFF);
		byte two = ((value >> 16) & 0xFF);
		byte one = ((value >> 24) & 0xFF);
		
		EEPROM.write(EEPROM.read(UidFreeAdress), four);
		EEPROM.write(EEPROM.read(UidFreeAdress) + 1, three);
		EEPROM.write(EEPROM.read(UidFreeAdress) + 2, two);
		EEPROM.write(EEPROM.read(UidFreeAdress) + 3, one);

		EEPROM.write(UidFreeAdress, EEPROM.read(UidFreeAdress) + 4);
		EEPROM.commit();
		Serial.println("Метка успешно записана");
}

void DeleteFromEEPROM(unsigned long value) //Удаление метки из EEPROM
{	
		EEPROM.write(deleteAdress, 0);
		EEPROM.write(deleteAdress + 1, 0);
		EEPROM.write(deleteAdress + 2, 0);
		EEPROM.write(deleteAdress + 3, 0);

		EEPROM.write(UidFreeAdress, EEPROM.read(UidFreeAdress) - 4);
		EEPROM.commit();

		Serial.println("Удаление прошло успешно");
	
	
}

void RewriteEEPROMAfterDelete() //Перезапись ячеек на случай удаления
{
	for (int i = deleteAdress; i < maxAvailableAdress; i++)
	{
		byte data = EEPROM.read(i+4);
		Serial.println(i);
		EEPROM.write(i, data);
	}
	EEPROM.commit();
	
}

void ShowUID() //Выводим ID метки в десятичном формате
{  	
	uidDec = 0;		
	for (byte i = 0; i < mfrc522.uid.size; i++) //Функция вывода ID метки
	{
		uidDecTemp = mfrc522.uid.uidByte[i]; // Выдача серийного номера метки.
		uidDec = uidDec * 256 + uidDecTemp;
	}
	Serial.print("Card UID: ");
	Serial.println(uidDec); // Выводим UID метки в консоль.
}


bool IsAdmin(unsigned long card) //Функция проверки является ли карта админской
{
	if(card == uidAdmin) return true;
	else return false;
}

void setup() 
{
	Serial.begin(115200);   // Инициализация сериал порта
  	while (!Serial);      // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  	SPI.begin();          // Init SPI bus
  	mfrc522.PCD_Init();   // Init MFRC522

	EEPROM.begin(1000);

	Serial.println(EEPROM.read(UidFreeAdress));
	
	for (int  i = 0; i < 12; i++)
	{
		//EEPROM.write(i,0);
		Serial.println(EEPROM.read(i));
	}
	//EEPROM.write(UidFreeAdress, 0);
	//EEPROM.commit();

}

void loop() 
{
	if ( ! mfrc522.PICC_IsNewCardPresent()) //Поиск новой метки
	{
    	return;
  	}

	if ( mfrc522.PICC_ReadCardSerial()) 
	{
		if (millis() - TimerOnShowTags >= 2000)
		{
			TimerOnShowTags = millis();
			ShowUID();

			if (FindingTagsInEEPROM(uidDec)) //Если метка есть в базе
			{
				DeleteFromEEPROM(uidDec); //Удаляем
				RewriteEEPROMAfterDelete();
			}
			else //Если нет
			{
				EEPROMWriteUID(uidDec); //Добавляем
			}
			
		}

		
	}
}