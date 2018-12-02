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

bool scanEnded = false;
int rssi;

char ssid[] = "RPI";		         //  your network SSID (name)
char pass[] = "helphelp";		     // your network password
int status = WL_IDLE_STATUS;
IPAddress server(192, 168, 137, 1);  // The Server IP
char localIPString[16];
int port = 1234;                     // Port to send the data
WiFiClient client;                   // initialize the WiFi Client to connect and send to the Server

char rssiString[3];                  // Char array to store the Curren RSSI 
char beaconAddress[20];			     // Char array to store the Curren MAC	
char beaconsPackets[20][100];        // 2D Char array to store the Packet with the MAC and RSSI Values
int AddressesCount = 0;			     // Counter to Track the Founded Beacons
bool deviceFound = false;		     // Flag to detect if the Device is found 
char theReaderName[4] = ",R2";
bool serverConnectedForFirstTime = false;
//flags for the Wifi and Server Connection
bool wifiConnected = false;     
bool serverConnected = false; 

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
			printf("The Server Disconnected, Waiting for the Server to go online . . !  \n");
			serverConnected = false;
		}
	}
	else
	{
		printf("Not Connected to a WiFi Network . . . Tring To connect again . . !  \n");
		wifiConnected = false;
	}
}

//Function to connect to WiFi
void connectWifi()
{
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print("Attempting to connect to WPA SSID: ");
		Serial.println(ssid);
		// Connect to WPA/WPA2 network:
		status = WiFi.begin(ssid, pass);
		// wait 3 seconds for connection:
		delay(3000);
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
			}
			else 
			{
				printf("The server disconnected  . . the ESP will restart to connect again .!  \n");
				ESP.restart();
			}
		}
		else
		{
			connectWifi();
		}
	}

	Serial.println("Connected to the server : Success  \n");
	serverConnected = true;
	client.write(strcat(localIPString, theReaderName));
	printf(strcat(localIPString, theReaderName));
}
//check connection function must be executed in the main thread not in the scan callback
//to avoid Core panic and reboot 
void checkConnection()
{
	if (!wifiConnected)
	{
		connectWifi();
	}
	else
	{
		if (!serverConnected)
		{
			connectServer();
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
						printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Packet Sent \n");
						printf(beaconsPackets[i]);
						printf("\n");
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
				printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Device Added \n");
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
	scanEnded = true;
	printf("Scan complete!\n");
	AddressesCount = scanResults.getCount(); //Correction for the AddessesCount number //known Error//
	printf("We found %d devices\n", AddressesCount);
	scanResults.dump();
} // scanCompleteCB

void setup()
{
	Serial.begin(115200);
	status = WiFi.begin(ssid, pass);
	// attempt to connect to Wifi network:
	connectWifi();
	connectServer();
}

/**
* Run the sample.
*/
static void run()
{
	printf("Async Scanning sample starting\n");
	BLEDevice::init("");
	BLEScan *pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
	pBLEScan->setActiveScan(true);
StartHere:
	printf("About to start scanning for 60 seconds\n");
	scanEnded = false;
	pBLEScan->start(60, scanCompleteCB);
	//
	// you can consider this loop as the main Thread
	//
	while (1)
	{
		if (!scanEnded)
		{
			checkConnection();
			printf("Tick! - still alive\n");
			//delay(1000);
		}
		else
		{
			goto StartHere;
		}
		FreeRTOS::sleep(1000);
	}
	printf("Scanning sample ended\n");
}

void loop()
{
	run();
} // app_main
