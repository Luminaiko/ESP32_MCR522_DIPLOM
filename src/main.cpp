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


Thread showUIDonTime = Thread();


int MaxRFIDTags = 50;	//Максимальное количество RFID меток
int maxAvailableAdress = MaxRFIDTags * 4;	//Максимальный адрес занимаемый метками

unsigned long CardUIDeEPROMread[] = { //Массив для меток
0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29, 
30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49};

unsigned long uidDec, uidDecTemp; // для хранения номера метки в десятичном формате
unsigned long uidAdmin = 3544981781; // Номер метки админа

byte EEPROMstartAddr = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN); // Создание обьекта RFID

void EEPROMWriteUID(int address, unsigned long value) //запись карты в EEPROM
{
	byte four = (value & 0xFF);
	byte three = ((value >> 8) & 0xFF);
	byte two = ((value >> 16) & 0xFF);
	byte one = ((value >> 24) & 0xFF);
	
	EEPROM.write(address, four);
	EEPROM.write(address + 1, three);
	EEPROM.write(address + 2, two);
	EEPROM.write(address + 3, one);

	EEPROM.write(UidFreeAdress, address + 5);
	EEPROM.commit();

	
}

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
	for (int i = 0; i < maxAvailableAdress; i+=5)
	{
		if (uidDec == EEPROMReadUID(i))
		{
			Serial.println("Метка есть в базе");
			return true;
		}
	}
	return false;
}

void ShowUID() //Выводим ID метки в десятичном формате
{  	
	uidDec = 0;		
	for (byte i = 0; i < mfrc522.uid.size; i++) //Функция вывода ID метки
	{
		uidDecTemp = mfrc522.uid.uidByte[i]; // Выдача серийного номера метки.
		uidDec = uidDec * 256 + uidDecTemp;
	}
	Serial.println("Card UID: ");
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

	EEPROMstartAddr = EEPROM.read(UidFreeAdress); //Чтение свободного адреса для записи ячеек
	Serial.println(EEPROMstartAddr);

	showUIDonTime.onRun(ShowUID);
	showUIDonTime.setInterval(5000);
}

void loop() 
{
	if ( ! mfrc522.PICC_IsNewCardPresent()) //Поиск новой метки
	{
    	return;
  	}

	if ( mfrc522.PICC_ReadCardSerial()) 
	{
		if (showUIDonTime.shouldRun())
		{	
			showUIDonTime.run();
			if (FindingTagsInEEPROM(uidDec))
			{
				
			}
			else 
			{
				Serial.println("НЕТУ");
			}
			
		}
		
	}
}