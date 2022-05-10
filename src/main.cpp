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
#define Zoomer 32

//////////////////////////////////////////// АДРЕСА В ЕЕПРОМ ПАМЯТИ /////////////////////////////////////////

#define RfidFreeAddress 200 //Адрес ячейки для хранения свободного адреса для записи 
#define SsidAdress 495 //Адрес ячеййки для хранения SSID роутера
#define PasswordAddress 480 //адрес для хранения пароля от роутера

////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Message {
	private:

	public:
		static void sendMessage(String Message)
		{
			
		}



};


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

void succes() //  функция открытия замка 
{                      
	digitalWrite(Zoomer, HIGH);
	delay(1000);
	digitalWrite(Zoomer, LOW);
}

void reject() //  функция отказа в открытии замка
{                  
	for (uint8_t n=0; n<=2; n++)
	{    
		digitalWrite(Zoomer, HIGH);
		delay(100);      
		digitalWrite(Zoomer, LOW);
		delay(100);
	}
}

void enterMasterMode() //  функция открытия замка 
{                      
	for (uint8_t n=0; n<=3; n++)
	{    
		digitalWrite(Zoomer, HIGH);
		delay(300);      
		digitalWrite(Zoomer, LOW);
		delay(300);
	}
}

void exitMasterMode() 
{
	for (uint8_t n=0; n<3; n++)
	{    
		digitalWrite(Zoomer, HIGH);
		delay(300);      
		digitalWrite(Zoomer, LOW);
		delay(300);
	}
}

void zoomerWrite() 
{
	digitalWrite(Zoomer, HIGH);
	delay(500);
	digitalWrite(Zoomer, LOW);
}

void zoomerDelete() 
{
	for (size_t i = 0; i < 2; i++)
	{
		digitalWrite(Zoomer, HIGH);
		delay(500);
		digitalWrite(Zoomer, LOW);
		delay(500);
	}
}

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
			return true;
		}
	}
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
}

void DeleteFromEEPROM(unsigned long value) //Удаление метки из EEPROM
{	
		EEPROM.write(deleteAdress, 0);
		EEPROM.write(deleteAdress + 1, 0);
		EEPROM.write(deleteAdress + 2, 0);
		EEPROM.write(deleteAdress + 3, 0);

		EEPROM.write(RfidFreeAddress, EEPROM.read(RfidFreeAddress) - 4);
		EEPROM.commit();
}

void RewriteEEPROMAfterDelete() //Перезапись ячеек на случай удаления
{
	for (int i = deleteAdress; i < maxAvailableAdress-4; i++)
	{
		EEPROM.write(i, EEPROM.read(i+4));
	}
	EEPROM.commit();
}

void getUid() //Выводим ID метки в десятичном формате
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

void showUid() 
{
	Serial.println("#00" + (String)uidDec + ";");
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
		showUid();
		zoomerDelete();
		RewriteEEPROMAfterDelete();
	}
	else //Если нет
	{
		WriteRfidEEPROM(uidDec); //Добавляем
		showUid();
		zoomerWrite();
	}
}

void Master() //Метод для записи/удаления меток 
{

	while (timeMasterStart + masterTime > millis())
	{
		if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
		{
			
			if (millis() - TimerOnShowTags >= 1000)
			{	
				getUid();
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

void CloseOpen(unsigned long uidDec) 
{
	if (FindRfidEEPROM(uidDec)) 
	{
		succes();
	}
	else 
	{
		reject();
	}
}

//////////////////////////////////////////////////ВЫПОЛНЕНИЕ КОМАНД////////////////////////////////////////////////////////////////


int Lenght = 20;
int amount;

void ChangeSSID(String expression)
{
    WriteStringEEPROM(SsidAdress, expression);
    EEPROM.commit();
}
void ChangePassword(String expression)
{
    WriteStringEEPROM(PasswordAddress, expression);
    EEPROM.commit();
}

void ChooseCommand(char data[]) //3 шаг: Проверяем какую именно команду нам отправили
{
  if(data[2] == '0')
  {
    String expression = "";
    for(int i = 3; i < amount; i++)
    {
		if(data[i] != ';')
		{
			expression += (char)data[i];
		}
		else
		{
			break;
		}
    }
    Serial.println("ВЫПОЛНЯЕТСЯ КОМАНДА");
    Serial.println(expression);
	ChangeSSID(expression);
    expression = "";
  }
  else if(data[2] == '1')
  {
	String expression = "";
    for(int i = 3; i < amount; i++)
    {
		if(data[i] != ';')
		{
			expression += (char)data[i];
		}
		else
		{
			break;
		}
    }
	ChangePassword(expression);
	Serial.println("ВЫПОЛНЯЕТСЯ КОМАНДА");
    Serial.println(expression);
	expression = "";
  }
  
}

void CheckForDistanation(char data[]) //2 шаг: Проверяем кому отправили данные
{
  if(data[1] == '1')
  {
    Serial.println("Это для ЕСП");
    ChooseCommand(data);
  }
  else
  {
    Serial.println("Это для компьютера");
    return;
  }
}

bool CheckForEnding(char data[])
{
  for(int i = 0; i < Lenght; i++)
  {
    if(data[i] == ';')
    {
      return true;
    }
  }
  return false;
}

void CheckForCommand(char data[]) //1 шаг: проверяем является ли командой 
{
  if(data[0] == '#' && CheckForEnding(data))
  {
    CheckForDistanation(data);
  }
  else
  {
    return;
  }
}

void setup() 
{
	pinMode(Zoomer, OUTPUT);
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

	Serial.println(ReadStringEEPROM(SsidAdress));

	WiFi.begin(ssid, password);

	bool connected = false;

	for (size_t i = 0; i < 10; i++)
	{
		if (WiFi.status() != WL_CONNECTED)
		{
			delay(500);
			Serial.println("...");
		}
		else
		{
			connected = true;
			break;
		}
	}
/*
	while (WiFi.status() != WL_CONNECTED) 
    {
		delay(500);
		Serial.println("...");
	}

*/
	if (connected == true)
	{
		Serial.println("WiFi connected with IP: ");
	}
	else
	{
		Serial.println("NO CONNECTION");
	}

	
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
	if(Serial.available() > 1)
  	{
		char data[Lenght];
		amount = Serial.readBytes(data, Lenght);
		data[amount]= NULL;
		CheckForCommand(data);
 	 }
	  
	if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) //Поиск новой метки
	{
		if (millis() - TimerOnShowTags >= 2000)
		{
			getUid();
			TimerOnShowTags = millis();
			if (IsAdmin(uidDec))
			{
				enterMasterMode();
				timeMasterStart = millis();
				Master(); 
				exitMasterMode();
				TimerOnShowTags = millis();
			}
			else
			{
				CloseOpen(uidDec);
				
			}
		}
		
		
  	}

}