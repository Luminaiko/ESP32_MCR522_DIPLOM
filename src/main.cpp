/*
-----------------
 MFRC522 | ESP32
-----------------
     SDA | d5
     SCK | d18
    MOSI | d23
    MISO | d19
     IRQ | N/A
     GND | GND
     RST | d27
    3.3V | 3.3V

	FINGER
	rx- tx2
	tx - rx2
*/
#include <SPI.h>
#include <Arduino.h>
#include <MFRC522.h>  	
#include <EEPROM.h>	
#include <WiFi.h>		
#include <Adafruit_Fingerprint.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h> 

#define RST_PIN 27	//22
#define SS_PIN 5 //21
#define SS_PIN2 4  //21
#define SS_PIN3 2
#define Zoomer 32
#define ButtonOut 15
#define RXD2 16
#define TXD2 17

void StringToIp(String message);
void WriteIntEEPROM(int address, int number);
int readIntEEPROM(int address);
int getFingerprintIDez();
uint8_t getFingerprintEnroll();
int findEmptyID();
uint8_t deleteFingerprint(uint8_t id);
void succes();
void reject();
void zoomerDelete();
void zoomerWrite();
void ChangeSSID(String expression);
String ReadStringEEPROM(int address);
void WriteStringEEPROM(int address, String str);
void SendMessage(String message);
unsigned long CharArrayToLong(char data[]);
void StringToCharArray(String message);
void ChangeWiFiSSID(String expression);
void WifiConnect();

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

//////////////////////////////////////////// АДРЕСА В ЕЕПРОМ ПАМЯТИ /////////////////////////////////////////

#define RfidFreeAddress 200 //Адрес ячейки для хранения свободного адреса для записи 
#define SsidAdress 495 //Адрес ячеййки для хранения SSID роутера
#define PasswordAddress 479 //адрес для хранения пароля от роутера
#define LocalIPAdressFirst 463 //Адрес для хранения первого октета
#define LocalIpAdressSecond 461  //Адрес для хранения второго октета
#define LocalIpAdressThird 459	//Адрес для хранения третьего октета
#define LocalIpAdressFourth 457	//Адрес для хранения четвертого октета
#define GatewayFirst 455 //Адрес для хранения шлюза первого октета
#define GatewaySecond 453  //Адрес для хранения шлюза второго октета
#define GatewayThird 451	//Адрес для хранения шлюза третьего октета
#define GatewayFourth 449	//Адрес для хранения шлюза четвертого октета
#define SubnetFirst 447 //Адрес для хранения шлюза первого октета
#define SubnetSecond 445  //Адрес для хранения шлюза второго октета
#define SubnetThird 443	//Адрес для хранения шлюза третьего октета
#define SubnetFourth 441	//Адрес для хранения шлюза четвертого октета
#define HostAdress 425	//Адрес для хранения адреса хоста

////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int Lenght = 20;
int amount;

int port = 8000;
const char* ssid;
const char* password;
const char* gateway;
const char* subnet;
const char* dns;
const char* host; 
uint32_t TimerOnShowTags;
uint32_t TimerButton;

int MaxRFIDTags = 50;	//Максимальное количество RFID меток
int maxAvailableAdress = MaxRFIDTags * 4;	//Максимальный адрес занимаемый метками
int deleteAdress; //Начальный адрес для удаления метки если такая есть
unsigned long uidDec, uidDecTemp; // для хранения номера метки в десятичном формате
unsigned long uidAdmin = 3544981781; // Номер метки админа

long timeStart;                 //  время, в которое сработала карта
long timeMasterStart;           //  время, в которое запустился цикл добавления новой метки
const int openTime = 200;     //  время, мс, на которое открывается замок
const int masterTime = 7000;  //  время, на которое активируется режим добавления новой метки

char DataRecieved[Lenght];

int IdFinger = 0;
int FingerToDelete;
String expression;
String message;
uint32_t Timer;

int firstOctet = 0;
int secondOctet = 0;
int thirdOctet = 0;
int fourthOctet = 0;

bool ReadWriteMode = false; //Флаг для режима записи

MFRC522 mfrc522(SS_PIN, RST_PIN); // Создание обьекта RFID
MFRC522 mfrc522_2(SS_PIN2, RST_PIN);
MFRC522 mfrc522_3(SS_PIN3, RST_PIN);
WiFiServer wifiServer(port);
WiFiClient client;
LiquidCrystal_I2C lcd(0x27,20,4);

class RFID
{
private:
	static void WriteRfidEEPROM(unsigned long value) //запись карты в EEPROM
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
	static unsigned long ReadRfidEEPROM(byte address) //Чтение карты из EEPROM
	{
		long four = EEPROM.read(address);
		long three = EEPROM.read(address + 1);
		long two = EEPROM.read(address + 2);
		long one = EEPROM.read(address + 3);
		
		return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
	}
	static void zoomerDelete() 
	{
		for (size_t i = 0; i < 2; i++)
		{
			digitalWrite(Zoomer, HIGH);
			delay(500);
			digitalWrite(Zoomer, LOW);
			delay(500);
		}
	}
	static void zoomerWrite() 
	{
		digitalWrite(Zoomer, HIGH);
		delay(500);
		digitalWrite(Zoomer, LOW);
	}
	///Отправка команд
	static void SendNewEvent(unsigned long uidDec)
	{
		SendMessage("#05" + (String)uidDec + ";");
		Serial.println("#05" + (String)uidDec + ";");
	}
	static void SendDeletedRFID(unsigned long uidDec)
	{
		SendMessage("#02" + (String)uidDec + ";");
		Serial.println("#02" + (String)uidDec + ";");
	}
	static void SendAddedRFID(unsigned long uidDec)
	{
		SendMessage("#01" + (String)uidDec + ";");
		Serial.println("#01" + (String)uidDec + ";");
	}
	
public:
	static void DeleteFromEEPROM(unsigned long value) //Удаление метки из EEPROM
	{	
			EEPROM.write(deleteAdress, 0);
			EEPROM.write(deleteAdress + 1, 0);
			EEPROM.write(deleteAdress + 2, 0);
			EEPROM.write(deleteAdress + 3, 0);

			EEPROM.write(RfidFreeAddress, EEPROM.read(RfidFreeAddress) - 4);
			EEPROM.commit();
	}
	static void RewriteEEPROMAfterDelete() //Перезапись ячеек на случай удаления
	{
		for (int i = deleteAdress; i < maxAvailableAdress-4; i++)
		{
			EEPROM.write(i, EEPROM.read(i+4));
		}
		EEPROM.commit();
	}
	static bool FindRfidEEPROM(unsigned long uidDec) //Нахождение метки в массиве EEPROM
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

	static void GetRFIDId() //Выводим ID метки в десятичном формате
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
	static void GetRFIDId2() //Выводим ID метки в десятичном формате
	{  	
		uidDec = 0;		
		for (byte i = 0; i < mfrc522_2.uid.size; i++) //Функция вывода ID метки
		{
			uidDecTemp = mfrc522_2.uid.uidByte[i]; // Выдача серийного номера метки.
			uidDec = uidDec * 256 + uidDecTemp;
		}
		Serial.print("Card UID: ");
		Serial.println(uidDec); // Выводим UID метки в консоль.
	}
	static bool IsAdmin(unsigned long card) //Функция проверки является ли карта админской
	{
		if(card == uidAdmin) 
		{
			ReadWriteMode = true;
			return true;
		}
		else return false;
	}
	static void CloseOpen(unsigned long uidDec) 
	{
		if (FindRfidEEPROM(uidDec)) 
		{
			succes();
			SendNewEvent(uidDec);
		}
		else 
		{
			reject();
		}
	}
	static void WriteDeleteMode(unsigned long uidDec) //Для записи/удаления в мастер моде
	{
		if (FindRfidEEPROM(uidDec)) //Если метка есть в базе
		{
			DeleteFromEEPROM(uidDec); //Удаляем
			SendDeletedRFID(uidDec);
			zoomerDelete();
			RewriteEEPROMAfterDelete();
		}
		else //Если нет
		{	

			WriteRfidEEPROM(uidDec); //Добавляем
			SendAddedRFID(uidDec);
			zoomerWrite();
		}
	}
	static void Master() //Метод для записи/удаления меток 
	{
		while (timeMasterStart + masterTime > millis())
		{
			if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
			{
				
				if (millis() - TimerOnShowTags >= 1000)
				{	
					GetRFIDId();
					if(IsAdmin(uidDec))
					{
						return;
					}
					else
					{
						TimerOnShowTags = millis();
						RFID::WriteDeleteMode(uidDec);
						timeMasterStart = millis();
					}
				}
			}
		}
	}
	static void enterMasterMode() //  функция открытия замка 
	{                      
		for (uint8_t n=0; n<=3; n++)
		{    
			digitalWrite(Zoomer, HIGH);
			delay(300);      
			digitalWrite(Zoomer, LOW);
			delay(300);
		}
	}
	static void exitMasterMode() 
	{
		for (uint8_t n=0; n<3; n++)
		{    
			digitalWrite(Zoomer, HIGH);
			delay(300);      
			digitalWrite(Zoomer, LOW);
			delay(300);
		}
	}

};

class Fingerprint //Класс с методами для работы с отпечатком пальца
{
private:
	static uint8_t deleteFingerprint(uint8_t id) 
	{
		uint8_t p = -1;

		p = finger.deleteModel(id);

		if (p == FINGERPRINT_OK) 
		{
			Serial.println("Finger with ID: " + (String)id + "Deleted!");
		} else if (p == FINGERPRINT_PACKETRECIEVEERR) {
			Serial.println("Communication error");
		} else if (p == FINGERPRINT_BADLOCATION) {
			Serial.println("Could not delete in that location");
		} else if (p == FINGERPRINT_FLASHERR) {
			Serial.println("Error writing to flash");
		} else {
			Serial.print("Unknown error: 0x"); Serial.println(p, HEX);
		}
		return p;
	}
	static uint8_t getFingerprintEnroll() 
	{
		IdFinger = findEmptyID();
		if (IdFinger < 0)
		{
			Serial.println("Лимит меток");
		}
		int p = -1;
		Serial.println(IdFinger);
		Serial.println("Place finger to enroll"); //First step
		while (p != FINGERPRINT_OK) 
		{
			p = finger.getImage();
		}
		// OK success!
		p = finger.image2Tz(1);
		switch (p) 
		{
			case FINGERPRINT_OK:
					break;
			case FINGERPRINT_IMAGEMESS:
				return p;
			case FINGERPRINT_PACKETRECIEVEERR:
				return p;
			case FINGERPRINT_FEATUREFAIL:
				return p;
			case FINGERPRINT_INVALIDIMAGE:
				return p;
			default:
				return p;
		}

		FingerToDelete = findFIngerID(p);

		if (FingerToDelete > 0 && FingerToDelete != 1)
		{
			deleteFingerprint(FingerToDelete);
			return p;
		}

		Serial.print("Remove finger to enroll"); //After getting the first template successfully
		delay(2000);
		p = 0;
		while (p != FINGERPRINT_NOFINGER) 
		{
			p = finger.getImage();
		}

		p = -1;

		Serial.println("Place same finger please"); //We launch the same thing another time to get a second template of the same finger
		while (p != FINGERPRINT_OK) 
		{
			p = finger.getImage();
		}

		// OK success!

		p = finger.image2Tz(2);
		switch (p) 
		{
			case FINGERPRINT_OK:
				break;
			case FINGERPRINT_IMAGEMESS:
				return p;
			case FINGERPRINT_PACKETRECIEVEERR:
				return p;
			case FINGERPRINT_FEATUREFAIL:
				return p;
			case FINGERPRINT_INVALIDIMAGE:
				return p;
			default:
				return p;
		}
		
		p = finger.createModel();
		if (p == FINGERPRINT_OK) 
		{
		} 
		else if (p == FINGERPRINT_PACKETRECIEVEERR) 
		{
				return p;
		} 
		else if (p == FINGERPRINT_ENROLLMISMATCH) 
		{
				return p;
		} 
		else 
		{
				return p;
		}   
		
		p = finger.storeModel(IdFinger);
		if (p == FINGERPRINT_OK) 
		{
			Serial.print("Stored in ID: ");    //Print a message after storing and showing the ID where it's stored
			Serial.println(IdFinger);
			//IdFinger++;
			delay(3000);
		} 
		else if (p == FINGERPRINT_PACKETRECIEVEERR) 
		{
			return p;
		} 
		else if (p == FINGERPRINT_BADLOCATION) 
		{
			return p;
		} 
		else if (p == FINGERPRINT_FLASHERR) 
		{
			return p;
		} 
		else 
		{
			return p;
		}   
	}
	static int findFIngerID(uint8_t p)
	{
		p = finger.fingerFastSearch();
		if (p == FINGERPRINT_OK)  //if the searching fails it means that the template isn't registered
		{         
			return finger.fingerID;
		}
		return 0;
	}
	static int findEmptyID()
	{
		for (size_t i = 1; i < 128; i++)
		{
			if(!(finger.loadModel(i) == FINGERPRINT_OK))
			{
				return i;
			}
		}
		return -1;
	}
public:
	static int getFingerprintIDez() 
	{
		uint8_t p = finger.getImage();        //Image scanning
		if (p != FINGERPRINT_OK)  
			return -1;  

		p = finger.image2Tz();               //Converting
		if (p != FINGERPRINT_OK)  
			return -1;
		
		p = finger.fingerFastSearch();     //Looking for matches in the internal memory
		if (p != FINGERPRINT_OK)  //if the searching fails it means that the template isn't registered
		{         
			lcd.clear();
			lcd.setCursor(7,1);
			lcd.print("Access");
			lcd.setCursor(7,2);
			lcd.print("denied");
			reject();
			delay(2000);
			lcd.clear();
			return -1;
		}
		//If we found a match we proceed in the function
		if (finger.fingerID == 1)
		{
			getFingerprintEnroll();
			return -1;
		}
		
		lcd.clear();
		lcd.setCursor(7,1);
		lcd.print("Welcome");
		lcd.setCursor(8,2);
		lcd.print("ID: ");
		lcd.setCursor(11,2);
		lcd.print(finger.fingerID);
		succes();
		delay(1000);
		lcd.clear();

		Serial.println(finger.fingerID); //And the ID of the finger template
		return finger.fingerID; 
	}

};

class Command
{
private:
	static void CheckForDistanation(char data[]) //2 шаг: Проверяем кому отправили данные
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
	static void ChooseCommand(char data[]) //3 шаг: Проверяем какую именно команду нам отправили
	{
		if(data[2] == '0')
		{
			Serial.println("ВЫПОЛНЯЕТСЯ КОМАНДА");
			ChangeSSID(FindingEnding(data));
			expression = "";
		}
		else if(data[2] == '1')
		{
			Serial.println("ВЫПОЛНЯЕТСЯ КОМАНДА");
			ChangePassword(FindingEnding(data));
			expression = "";
		}
		else if(data[2] == '2')
		{
			//PrintSSIDPasswordInfo();
		}
		else if(data[2] == '3') 
		{
			DeleteRFID(FindingEnding(data));
			expression = "";
		}
		else if(data[2] == '4')
		{
			ChangeWiFiSSID(data);
		}
		else if(data[2] == '5')
		{
			ChangeWiFiPassword(data);
		}
		else if(data[2] == '6') //Установить адрес контроллера
		{
			ChangeLocalIP(FindingEnding(data));
		}
		else if(data[2] == '7') //Установить адрес хоста
		{
			ChangeHostAdress(FindingEnding(data));
		}
		else if(data[2] == '8') //Установить шлюз по умолчанию
		{
			ChangeGateway(FindingEnding(data));
		}
		else if(data[2] == '9') //Установить маску
		{
			ChangeSubnet(FindingEnding(data));
		}
	}
	static bool CheckForEnding(char data[])
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
	static String FindingEnding(char data[]) 
	{
		for(int i = 3; i < Lenght; i++)
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
		return expression;
	}

	static void ChangeSSID(String expression)
	{
		WriteStringEEPROM(SsidAdress, expression);
		EEPROM.commit();
	}
	static void ChangePassword(String expression)
	{
		WriteStringEEPROM(PasswordAddress, expression);
		EEPROM.commit();
	}
	static void DeleteRFID(String expression) 
	{
		Serial.println("EXPRESSION = " + expression);
		//StringToCharArray(expression);
		char RFIDDELETE[Lenght];
		for (int i = 0; i < Lenght; i++)
		{
			RFIDDELETE[i] = 0;
		}
				
		for (int i = 0; i < Lenght; i++)
		{
			RFIDDELETE[i] += expression[i];
		}

		if(RFID::FindRfidEEPROM(CharArrayToLong(RFIDDELETE)))
		{
			RFID::DeleteFromEEPROM(CharArrayToLong(RFIDDELETE));
		}
		else 
		{
			Serial.println("RFID NETU ");
		}
	}
	static void ChangeWiFiSSID(char data[])
	{
		WriteStringEEPROM(SsidAdress, FindingEnding(data));
		WiFi.disconnect();
		WifiConnect();
	}
	static void ChangeWiFiPassword(char data[])
	{
		WriteStringEEPROM(PasswordAddress, FindingEnding(data));
		WiFi.disconnect();
		WifiConnect();
	}
	static void ChangeLocalIP(String expression)
	{
		StringToIp(expression);
		WriteIntEEPROM(LocalIPAdressFirst, firstOctet);
		WriteIntEEPROM(LocalIpAdressSecond, secondOctet);
		WriteIntEEPROM(LocalIpAdressThird, thirdOctet);
		WriteIntEEPROM(LocalIpAdressFourth, fourthOctet);
	}
	static void ChangeGateway(String expression)
	{
		StringToIp(expression);
		WriteIntEEPROM(GatewayFirst, firstOctet);
		WriteIntEEPROM(GatewaySecond, secondOctet);
		WriteIntEEPROM(GatewayThird, thirdOctet);
		WriteIntEEPROM(GatewayFourth, fourthOctet);
	}
	static void ChangeHostAdress(String expression)
	{
		WriteStringEEPROM(HostAdress, expression);
	}
	static void ChangeSubnet(String expression)
	{
		StringToIp(expression);
		WriteIntEEPROM(SubnetFirst, firstOctet);
		WriteIntEEPROM(SubnetSecond, secondOctet);
		WriteIntEEPROM(SubnetThird, thirdOctet);
		WriteIntEEPROM(SubnetFourth, fourthOctet);
	}
public:
	static void CheckForCommand(char data[]) //1 шаг: проверяем является ли командой 
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
};

unsigned long CharArrayToLong(char data[])
{
	//Serial.print("Я ВЕРНУЛ ЭТО ЗНАЧЕНИЕ: ");
	unsigned long x = strtoul(data, NULL, 10);
	//Serial.println(x);
	return x;
}

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

void WifiConnect() 
{
	char ssidBuf[EEPROM.read(SsidAdress)]; 
 	ReadStringEEPROM(SsidAdress).toCharArray(ssidBuf, EEPROM.read(SsidAdress)+1);
	char passwordBuf[EEPROM.read(PasswordAddress)];
	ReadStringEEPROM(PasswordAddress).toCharArray(passwordBuf, EEPROM.read(SsidAdress)+1);
	
	IPAddress local_ip(readIntEEPROM(LocalIPAdressFirst), readIntEEPROM(LocalIpAdressSecond), readIntEEPROM(LocalIpAdressThird), readIntEEPROM(LocalIpAdressFourth));
	IPAddress gateway(readIntEEPROM(GatewayFirst), readIntEEPROM(GatewaySecond), readIntEEPROM(GatewayThird), readIntEEPROM(GatewayFourth));
	IPAddress subnet(readIntEEPROM(SubnetFirst),readIntEEPROM(SubnetSecond),readIntEEPROM(SubnetThird),readIntEEPROM(SubnetFourth));
	ssid = ssidBuf;
	password = passwordBuf;

	Serial.print("Локальный IP = ");
	Serial.println((String)readIntEEPROM(LocalIPAdressFirst) + (String)readIntEEPROM(LocalIpAdressSecond) + (String)readIntEEPROM(LocalIpAdressThird) + (String)readIntEEPROM(LocalIpAdressFourth));

	Serial.print("Шлюз по умолчанию = ");
	Serial.println(gateway);
	Serial.print("Маска подсети = ");
	Serial.println(subnet);

	Serial.println(ReadStringEEPROM(SsidAdress));
	Serial.println(ReadStringEEPROM(PasswordAddress));

	WiFi.config(local_ip, gateway, subnet);
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

	if (connected == true)
	{
		Serial.println("WiFi connected with IP: ");
	}
	else
	{
		Serial.println("NO CONNECTION");
	}
	
}
void SendMessage(String message) 
{
	char hostBuf[EEPROM.read(HostAdress)];
	ReadStringEEPROM(HostAdress).toCharArray(hostBuf, EEPROM.read(HostAdress)+1);
	host = hostBuf;
	Serial.println("Отправляем данные на " + (String)host);
	client.connect(host, port, 1000);
	client.print(message);
	client.stop();
}

void StringToCharArray(String message) 
{
	for (int i = 0; i < Lenght; i++)
	{
		DataRecieved[i] = 0;
	}
			
	for (int i = 0; i < Lenght; i++)
	{
		DataRecieved[i] += message[i];
	}
	
}

void WriteIntEEPROM(int address, int number)
{
  EEPROM.write(address, (number >> 8) & 0xFF);
  EEPROM.write(address+1, number & 0xFF);

  EEPROM.commit();
}

int readIntEEPROM(int address)
{
  return (EEPROM.read(address) << 8) + 
          EEPROM.read(address+1);
}

void StringToIp(String message)
{
	String first = "";
	String second = "";
	String third = "";
	String fourth = "";
	int counter = 0;
	
	int len = message.length();
	for (int i = 0; i < 4; i++)
	{
		if (message[i] != '.')
		{
			first += message[i];
			//counter++;
		}
		else 
		{
			counter += i;
			break;
		}
	}
	for (int i = counter+1; i < counter+5; i++)
	{
		if (message[i] != '.')
		{
			second += message[i];
			//counter++;
		}
		else
		{
			counter = 0;
			counter += i;
			break;
		}
	}
	for (int i = counter+1; i < counter + 5; i++)
	{
		if (message[i] != '.')
		{
			third += message[i];
			//counter++;
		}
		else
		{
			counter = 0;
			counter += i;
			break;
		}
	}
	for (int i = counter+1; i < len; i++)
	{
		fourth += message[i];
	}
	
	firstOctet = first.toInt();
	secondOctet = second.toInt();
	thirdOctet = third.toInt();
	fourthOctet = fourth.toInt();
}

void setup() 
{
	expression = "";
	pinMode(ButtonOut, INPUT);
	pinMode(Zoomer, OUTPUT);
	Serial.begin(115200);   // Инициализация сериал порта
	Serial2.begin(5700, SERIAL_8N1, RXD2, TXD2);
  	while (!Serial);      // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  	SPI.begin();          // Init SPI bus
	finger.begin(57600);
  	mfrc522.PCD_Init();   // Init MFRC522
	
	if (finger.verifyPassword()) 
	{
    	Serial.println("Found fingerprint sensor!");
  	}
	EEPROM.begin(1000);
	WifiConnect();
	wifiServer.begin();
	mfrc522_2.PCD_Init();
	mfrc522_3.PCD_Init();
	Serial.println(WiFi.localIP());

	lcd.init();                      // initialize the lcd 
  	lcd.backlight();

}

void serialFlush()
{
  while(Serial.available() > 0) {
    char t = Serial.read();
  }
}

void loop() 
{	
	WiFiClient client = wifiServer.available();
	if (client) 
	{
		while (client.connected()) 
		{
			while (client.available() > 0) 
			{
				char c = client.read();
				//Serial.write(c);
				message += (String)c;
			}
			StringToCharArray(message);

			Serial.println(DataRecieved);
			Command::CheckForCommand(DataRecieved);
			message = "";
			client.stop();
			break;
		}
		Serial.println("Client disconnected");
	}

	if(Serial.available() > 1)
  	{
		char data[Lenght];
		amount = Serial.readBytes(data, Lenght);
		data[amount]= NULL;
		Command::CheckForCommand(data);
		
 	}
	lcd.setCursor(7, 0);
	lcd.print("Place"); 
	lcd.setCursor(8, 1);
	lcd.print("New");
	lcd.setCursor(7, 2);
	lcd.print("Finger");
	
	Fingerprint::getFingerprintIDez();
	if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) //Поиск новой метки
	{
		if (millis() - TimerOnShowTags >= 2000)
		{
			RFID::GetRFIDId();
			TimerOnShowTags = millis();
			if (RFID::IsAdmin(uidDec))
			{
				RFID::enterMasterMode();
				timeMasterStart = millis();
				RFID::Master(); 
				RFID::exitMasterMode();
				TimerOnShowTags = millis();
			}
			else
			{
				RFID::CloseOpen(uidDec);
			}
		}
  	}
	if (mfrc522_2.PICC_IsNewCardPresent() && mfrc522_2.PICC_ReadCardSerial())
	{
		if (millis() - TimerOnShowTags >= 2000)
		{
			RFID::GetRFIDId2();
			TimerOnShowTags = millis();
			if (RFID::IsAdmin(uidDec))
			{
				RFID::enterMasterMode();
				timeMasterStart = millis();
				RFID::Master(); 
				RFID::exitMasterMode();
				TimerOnShowTags = millis();
			}
			else
			{
				RFID::CloseOpen(uidDec);
			}
		}
	}

	if (millis() - TimerButton >= 2000 && digitalRead(ButtonOut) == HIGH) 
	{ 
		TimerButton = millis();
		succes();
	}

	if (mfrc522_3.PICC_IsNewCardPresent() && mfrc522_3.PICC_ReadCardSerial())
	{
		succes();
		Serial.println("KARTA PODNESENA");
	}

}







