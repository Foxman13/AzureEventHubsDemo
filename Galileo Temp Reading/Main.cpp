// Main.cpp : Defines the entry point for the console application.
//
//#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include "stdafx.h"
#include "arduino.h"

#pragma comment (lib, "Ws2_32.lib")

int _tmain(int argc, _TCHAR* argv[])
{
    return RunArduinoSketch();
}

#define DEFAULT_SERVER "169.254.79.76"
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

// comment this out to leave out the socket connection
//#define CONNECT_SOCKET 

int led = 13;  // This is the pin the LED is attached to.

//TMP36 Pin Variables
int sensorPin = 0; //the analog pin the TMP36's Vout (sense) pin is connected to
//the resolution is 10 mV / degree centigrade with a
//500 mV offset to allow for negative temperatures

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;

char recvbuf[DEFAULT_BUFLEN];
int recvbuflen = DEFAULT_BUFLEN;

int iResult;

int getReading(int pin);
float getVoltage(int reading);
float convertVoltageToTempC(float voltage);
float convertToTempF(float tempC);
int setupWinsockServer();

void setup()
{
    pinMode(led, OUTPUT);       // Configure the pin for OUTPUT so you can turn on the LED.

#ifdef CONNECT_SOCKET
	setupWinsockServer();
#endif
}

// the loop routine runs over and over again forever:
void loop()
{
	int iSendResult;

	digitalWrite(led, HIGH);
	Log(L"LED ON\n");

	int reading = getReading(sensorPin);
	float voltage = getVoltage(reading);
	Log(L"raw voltage reading: %lf\n", voltage);

	float celcius = convertVoltageToTempC(voltage);
	Log(L"External Sensor Temperature: %lf Celcius\n", celcius);

	//log farenheit just for the fun of it
	Log(L"External Sensor Temperature: %lf Farenheit\n", convertToTempF(celcius));
	delay(4000);

	digitalWrite(led, LOW);
	Log(L"LED OFF\n");
	delay(1000);

#ifdef CONNECT_SOCKET
	boost::format fmter("%1%;%2%;%3%\n"); // this string contains - device id, raw voltage, converted temp
	fmter % 0 % voltage % celcius;
	std::string ds = fmter.str();
	strcpy(recvbuf, ds.c_str());
	int len = ds.length();

	iSendResult = send(ClientSocket, recvbuf, len, 0);
	if (iSendResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return;
	}
#endif
}
 
int getReading(int pin)
{
	return analogRead(pin);
}

// the sensor reading from the TMP36 has a 10mV offset, so we have to compensate for that.
float getVoltage(int reading)
{
	float voltage = reading * 5.0; // the voltage the board is operating at is 5V - change to 3.3 if running off 3.3 V
	return voltage / 1024.0;
}

// Returns degrees in Celcius.
float convertVoltageToTempC(float voltage)
{
	return (voltage - 0.5) * 100;
}

// pass in temp in Celcius
float convertToTempF(float tempC)
{
	return (tempC * 9.0 / 5.0) + 32.0;
}

int setupWinsockServer()
{
	WSADATA wsaData;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	Log(L"Waiting for a socket connection from sender...");

	// Accept a client socket
	ClientSocket = accept(ListenSocket, NULL, NULL);
	//setColor(ledDigitalOne, GREEN);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// No longer need server socket
	closesocket(ListenSocket);
}
