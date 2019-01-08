/**
* Perform an async scanning for BLE advertised Beacons and Sends the Beacon MAC with 20 RSSI Values
to the Server
*/
#include <Blynk.h>
#include <dummy.h>
#include "BLEUtils.h"
#include "BLEScan.h"
#include <string>
#include <WiFi.h>
#include "BLEDevice.h"
#include "BLEAdvertisedDevice.h"
#include "sdkconfig.h"
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
int rssi;

char ssid[] = "RPI";		         //  your network SSID (name)
char pass[] = "helphelp";		     // your network password
int status = WL_IDLE_STATUS;

IPAddress local_IP(192, 168, 137, 20); // Set your Static IP address
IPAddress gateway(192, 168, 137, 1);   // Set your Gateway IP address
IPAddress subnet(255, 255, 255, 0);    // Set your Subnet mask address

IPAddress server(192, 168, 137, 1);  // The Server IP

char localIPString[16];
int port = 1234;                     // Port to send the data
WiFiClient client;                   // initialize the WiFi Client to connect and send to the Server

char rssiString[3];                  // Char array to store the Curren RSSI 
char beaconAddress[20];			     // Char array to store the Curren MAC	
char beaconsPackets[20][100];        // 2D Char array to store the Packet with the MAC and RSSI Values
int AddressesCount = 0;			     // Counter to Track the Founded Beacons
bool deviceFound = false;		     // Flag to detect if the Device is found 
char theReaderName[4] = ",R1";
bool serverConnectedForFirstTime = false;
//flags for the Wifi and Server Connection
bool wifiConnected = false;
bool serverConnected = false;
hw_timer_t *periodicReboot = NULL;
long loopTime = 0;

// This Function to check the WiFi and Server connection and send the Packet or set the flags false
void checkAndSend(char sendPacket[])
{
	if (WiFi.status() == WL_CONNECTED)
	{
		if (client.connected())
		{
			client.write(sendPacket);
			client.flush();
		}
		else
		{
			serverConnected = false;
		}
	}
	else
	{
		wifiConnected = false;
	}
}

//Function to connect to WiFi
void connectWifi()
{
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(" \nAttempting to connect to WPA SSID: ");
		Serial.println(ssid);
		//Connect to WPA/WPA2 network:
		status = !WiFi.config(local_IP, gateway, subnet);
		//wait 3 seconds for connection:
		//delay(3000);
		FreeRTOS::sleep(3000);
		if ((millis() - loopTime > 30000) && (WiFi.status() != WL_CONNECTED))
		{
			loopTime = millis();
			interruptReboot();
		}
	}
	strcpy(localIPString, WiFi.localIP().toString().c_str());
	Serial.println(WiFi.localIP());
	wifiConnected = true;
}

//Function to connect to the Server for the first time
//if the server disconnected the ESP would restart to connect again //know issue//
//if you tried to just reconnect to the server it won't work
void connectServer()
{
	while (!client.connected())
	{
		if (WiFi.status() == WL_CONNECTED)
		{
			if (!serverConnectedForFirstTime)
			{
				printf("Tring to connect to the Server . .  \n");
				client.connect(server, 1234);
				FreeRTOS::sleep(1000);
				if (client.connected())
				{
					serverConnectedForFirstTime = true;
				}
				if ((millis() - loopTime > 30000) && !serverConnectedForFirstTime)
				{
					loopTime = millis();
					interruptReboot();
				}
			}
			else
			{
				ESP.restart();
			}
		}
		else
		{
			connectWifi();
		}
	}
	serverConnected = true;
	client.write(strcat(localIPString, theReaderName));
	printf(strcat(localIPString, theReaderName));
}
//check connection function must be executed in the main thread not in the scan callback
//to avoid Core panic and reboot 
bool checkConnection()
{
	if (!wifiConnected)
	{
		connectWifi();
		return false;
	}
	else
	{
		if (!serverConnected || !client.connected())
		{
			connectServer();
			return false;
		}
		else {
			return true;
		}
	}
}

/**
* Callback for each detected advertised device.
*/
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
	void onResult(BLEAdvertisedDevice advertisedDevice)
	{
		if (wifiConnected && serverConnected)
		{
			rssi = advertisedDevice.getRSSI(); 		//get the RSSI Value
			strcpy(beaconAddress, advertisedDevice.getAddress().toString().c_str()); // get the Beacon MAC
			//Loop with the AddressesCount as the counter and check if the Device is detected before
			 //If so append its RSSI Value to the Related Row in the beaconsPackets
			for (int i = 0; i <= AddressesCount; i++)
			{
				if (strncmp(beaconAddress, beaconsPackets[i], 15) == 0)
				{
					deviceFound = true;
					itoa(rssi, rssiString, 10);
					strcat(beaconsPackets[i], ",");
					strcat(beaconsPackets[i], rssiString);
					if (strlen(beaconsPackets[i]) > 93)
					{
						//printf("Founded Beacons : %d\n", AddressesCount);
						strcat(beaconsPackets[i], theReaderName);
						checkAndSend(beaconsPackets[i]);
						printf("\n>>>>>>>>>>>>>>>>>>>>>> Packet Sent \n");
						printf(beaconsPackets[i]);
						strcpy(beaconsPackets[i], beaconAddress);
					}
					break;
				}
				else
				{
					deviceFound = false;
				}
			}
			if (!deviceFound)
			{
				printf("\n>>>>>>>>>>>>>>>>>>>>>>> Device Added \n");
				strcpy(beaconsPackets[AddressesCount], beaconAddress);
				AddressesCount++;
			}
		}
	}
};


/**
* Callback invoked when scanning has completed.
*/
static void scanCompleteCB(BLEScanResults scanResults)
{
	printf("Scan complete!\n");
	AddressesCount = scanResults.getCount(); //Correction for the AddessesCount number //known Error//
	printf("We found %d devices\n", AddressesCount);
	scanResults.dump();
} // scanCompleteCB

//the watchdog timer interrupt function 
TaskHandle_t Core0WdtHandle;
hw_timer_t *core0Wdt = NULL;
void Core0WDT(void * parameter)
{
	core0Wdt = timerBegin(0, 80, true);
	timerAlarmWrite(core0Wdt, 3000000, false);
	timerAttachInterrupt(core0Wdt, &interruptReboot, true);
	timerAlarmEnable(core0Wdt);

	while (true)
	{
		timerWrite(core0Wdt, 0); // reset the Watchdog timer
		vTaskDelay(1000);
	}
}

void setup()
{
	periodicReboot = timerBegin(0, 80, true);
	timerAlarmWrite(periodicReboot, 300000000, false);
	timerAttachInterrupt(periodicReboot, &interruptReboot, true);
	timerAlarmEnable(periodicReboot);
	loopTime = millis();

	//fire a Watchdog timer on Core0 with a interrupt function
	//to fix the issue of hanging the code when the default wdt triggered
	xTaskCreatePinnedToCore(
		Core0WDT, /* Function to implement the task */
		"Core0WatchdogTimer", /* Name of the task */
		10000,  /* Stack size in words */
		NULL,  /* Task input parameter */
		0,  /* Priority of the task */
		&Core0WdtHandle,  /* Task handle. */
		0); /* Core where the task should run */

	Serial.begin(115200);
	status = WiFi.begin(ssid, pass);
	// attempt to connect to Wifi network:
	connectWifi();
	connectServer();
}

void loop()
{
	printf("\nAsync Scanning sample starting\n");
	BLEDevice::init("");
	BLEScan *pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
	pBLEScan->setActiveScan(true);
	printf("\nAbout to start scanning forever seconds\n");
	pBLEScan->start(0, scanCompleteCB);
	//
	// you can consider this loop as the main Thread
	//
	while (1)
	{
		checkConnection();
		printf("\nTick! - still alive");
		FreeRTOS::sleep(1000);
	}
} // app_main
void interruptReboot()
{
	esp_task_wdt_init(1, true);
	esp_task_wdt_add(NULL);
	while (true);
}
