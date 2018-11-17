/**
* Perform an async scanning for BLE advertised Beacons and Sends the Beacon MAC with 20 RSSI Values
to the Server
*/
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

char ssid[] = "RPI";      //  your network SSID (name)
char pass[] = "helphelp"; // your network password
int status = WL_IDLE_STATUS;
IPAddress server(192, 168, 137, 1); // The Server IP
char localIPString[16];
int port = 1234;                      // Port to send the data
WiFiClient client;                  // initialize the WiFi Client to connect and send to the Server

char rssiString[3];                // Char array to store the Curren RSSI 
char beaconAddress[20];			   // Char array to store the Curren MAC	
char beaconsPackets[20][100];    // 2D Char array to store the Packet with the MAC and RSSI Values
int AddressesCount = 0;			   // Counter to Track the Founded Beacons
bool deviceFound = false;		   // Flag to detect if the Device is found 
char theReaderName[4] = ",R1";

/**
* Callback for each detected advertised device.
*/
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
	void onResult(BLEAdvertisedDevice advertisedDevice)
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
				// printf(beaconAddress);
				// printf(beaconsPackets[i]);
				//printf("Device Address: %s\n Device RSSI: %d\n", beaconAddress, rssi);
				itoa(rssi, rssiString, 10);
				strcat(beaconsPackets[i], ",");
				strcat(beaconsPackets[i], rssiString);
				if (strlen(beaconsPackets[i]) > 93)
				{
					printf("Founded Beacons : %d\n", AddressesCount);
					printf("Current Counter in the For Loop : %d\n", i);
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
			printf("############################################# Device Added \n");
			strcpy(beaconsPackets[AddressesCount], beaconAddress);
			AddressesCount++;
		}
	}
};

// This Function to check the WiFi and Server connection and send the Packet or try to connect
void checkAndSend(char sendPacket[])
{
	if (WiFi.status() == WL_CONNECTED)
	{
		if (client.connected())
		{
			client.write(sendPacket);
		}
		else
		{
			printf("The Server Disconnected, Waiting for the Server to go online . . !");
			connectServer();
		}
	}
	else
	{
		printf("Not Connected to a WiFi Network . . . Tring To connect again . . !");
		connectWifi();
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
		// wait 10 seconds for connection:
		delay(3000);
	}
	strcpy(localIPString, WiFi.localIP().toString().c_str());
	Serial.println(WiFi.localIP());
}

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

  //Function to connect to the Server
void connectServer()
{
	while (!client.connect(server, port))
	{
		Serial.println("Tring to connect to the Server . . ");
		FreeRTOS::sleep(1000);
	}
	Serial.println("Connected to the server : Success");
	client.write(strcat(localIPString, theReaderName));
	printf(strcat(localIPString, theReaderName));

}

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
	// Now going into a loop logging that we are still alive.
	//
	while (1)
	{
		if (!scanEnded)
		{
			printf("Tick! - still alive\n");
			FreeRTOS::sleep(1000);
		}
		else
		{
			goto StartHere;
		}
	}
	printf("Scanning sample ended\n");
}

void loop()
{
	run();
} // app_main
