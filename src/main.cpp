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
#include <MFRC522.h>  	
#include <EEPROM.h>	
#include <WiFi.h>		
#include <Thread.h>

#define RST_PIN 22
#define SS_PIN 21

#define RfidFreeAddress 200 //Адрес ячейки для хранения свободного адреса для записи 
#define SsidAdress 495 //Адрес ячеййки для хранения SSID роутера
#define PasswordAddress 480 //адрес для хранения пароля от роутера

const char* ssid;
const char* password;

uint32_t TimerOnShowTags;
int MaxRFIDTags = 50;	//Максимальное количество RFID меток
int maxAvailableAdress = MaxRFIDTags * 4;	//Максимальный адрес занимаемый метками
int deleteAdress; //Начальный адрес для удаления метки если такая есть
unsigned long uidDec, uidDecTemp; // для хранения номера метки в десятичном формате
unsigned long uidAdmin = 3544981781; // Номер метки админа

long timeStart;                 //  время, в которое сработала карта
long timeMasterStart;           //  время, в которое запустился цикл добавления новой метки
const int openTime = 200;     //  время, мс, на которое открывается замок
const int masterTime = 7000;  //  время, на которое активируется режим добавления новой метки

bool ReadWriteMode = false; //Флаг для режима записи

MFRC522 mfrc522(SS_PIN, RST_PIN); // Создание обьекта RFID

unsigned long ReadRfidEEPROM(byte address) //Чтение карты из EEPROM
{
	long four = EEPROM.read(address);
	long three = EEPROM.read(address + 1);
	long two = EEPROM.read(address + 2);
	long one = EEPROM.read(address + 3);
	
	return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

bool FindRfidEEPROM(unsigned long uidDec) //Нахождение метки в массиве EEPROM
{
	for (int i = 0; i < maxAvailableAdress; i+=4)
	{
		if (uidDec == ReadRfidEEPROM(i))
		{
			deleteAdress = i;
			Serial.println("Метка есть в базе");
			return true;
		}
	}
	Serial.println("Метки нет в базе");
	return false;
}

void WriteRfidEEPROM(unsigned long value) //запись карты в EEPROM
{
		byte four = (value & 0xFF);
		byte three = ((value >> 8) & 0xFF);
		byte two = ((value >> 16) & 0xFF);
		byte one = ((value >> 24) & 0xFF);
		
		EEPROM.write(EEPROM.read(RfidFreeAddress), four);
		EEPROM.write(EEPROM.read(RfidFreeAddress) + 1, three);
		EEPROM.write(EEPROM.read(RfidFreeAddress) + 2, two);
		EEPROM.write(EEPROM.read(RfidFreeAddress) + 3, one);

		EEPROM.write(RfidFreeAddress, EEPROM.read(RfidFreeAddress) + 4);
		EEPROM.commit();
		Serial.println("Метка успешно записана");
}

void DeleteFromEEPROM(unsigned long value) //Удаление метки из EEPROM
{	
		EEPROM.write(deleteAdress, 0);
		EEPROM.write(deleteAdress + 1, 0);
		EEPROM.write(deleteAdress + 2, 0);
		EEPROM.write(deleteAdress + 3, 0);

		EEPROM.write(RfidFreeAddress, EEPROM.read(RfidFreeAddress) - 4);
		EEPROM.commit();

		Serial.println("Удаление прошло успешно");
	
	
}

void RewriteEEPROMAfterDelete() //Перезапись ячеек на случай удаления
{
	for (int i = deleteAdress; i < maxAvailableAdress-4; i++)
	{
		EEPROM.write(i, EEPROM.read(i+4));
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
	//Serial.print("Card UID: ");
	//Serial.println(uidDec); // Выводим UID метки в консоль.
}

bool IsAdmin(unsigned long card) //Функция проверки является ли карта админской
{
	if(card == uidAdmin) 
	{
		ReadWriteMode = true;
		return true;
	}
	else return false;
}

void WriteStringEEPROM(int address, String str) //Записать строку в ЕЕПРОМ
{
	byte len = str.length();
	EEPROM.write(address, len);
	for (int i = 0; i < len; i++)
	{
		EEPROM.write(address + 1 + i, str[i]);
	}
	EEPROM.commit();
}

String ReadStringEEPROM(int address) //Прочитать строку из еепром
{
	int len = EEPROM.read(address);
	char data[len];
	
	for (int i = 0; i < len; i++)
	{
		data[i] = EEPROM.read(address + 1 + i);
	}
	data[len] = '\0'; 
	return String(data);
  
}

void WriteDeleteMode(unsigned long uidDec) //Для записи/удаления в мастер моде
{
	if (FindRfidEEPROM(uidDec)) //Если метка есть в базе
	{
		DeleteFromEEPROM(uidDec); //Удаляем
		RewriteEEPROMAfterDelete();
	}
	else //Если нет
	{
		WriteRfidEEPROM(uidDec); //Добавляем
	}
}

void Master() 
{
	while (timeMasterStart + masterTime > millis())
	{
		if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
		{
			ShowUID();
			if (millis() - TimerOnShowTags >= 1000)
			{	
				if(IsAdmin(uidDec))
				{
					return;
				}
				else
				{
					TimerOnShowTags = millis();
					WriteDeleteMode(uidDec);
					timeMasterStart = millis();
				}
			}
		}


		
	}
	
}


void setup() 
{
	Serial.begin(115200);   // Инициализация сериал порта
  	while (!Serial);      // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  	SPI.begin();          // Init SPI bus
  	mfrc522.PCD_Init();   // Init MFRC522

	EEPROM.begin(1000);

	char ssidBuf[EEPROM.read(SsidAdress)]; 
 	ReadStringEEPROM(SsidAdress).toCharArray(ssidBuf, EEPROM.read(SsidAdress)+1);
	char passwordBuf[EEPROM.read(PasswordAddress)];
	ReadStringEEPROM(PasswordAddress).toCharArray(passwordBuf, EEPROM.read(SsidAdress)+1);

	ssid = ssidBuf;
	password = passwordBuf;

	WiFi.begin(ssid, password);

	while (WiFi.status() != WL_CONNECTED) 
    {
		delay(500);
		Serial.println("...");
	}
 
  	Serial.print("WiFi connected with IP: ");
	
/*
	Serial.println(EEPROM.read(RfidFreeAddress));
	
	for (int  i = 0; i < 12; i++)
	{
		//EEPROM.write(i,0);
		Serial.println(EEPROM.read(i));
	}
	//EEPROM.write(RfidFreeAddress, 0);
	//EEPROM.commit();
*/
}

void loop() 
{
	if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) //Поиск новой метки
	{
		ShowUID();
		if (millis() - TimerOnShowTags >= 2000)
		{
			TimerOnShowTags = millis();
			if (IsAdmin(uidDec))
			{
				Serial.println("Режим записи включен");
				timeMasterStart = millis();
				Master(); 
				Serial.println("Выход из функции");
				TimerOnShowTags = millis();
			}
			else
			{
				FindRfidEEPROM(uidDec);
			}
		}
		
		
  	}

}