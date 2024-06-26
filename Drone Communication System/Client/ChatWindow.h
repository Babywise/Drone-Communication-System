/*
* Project: Next Level Drone Systems
* Module: Client
* Language: C++
*
* File: ChatWindow.h
*
* Description: Contains the chat window class and functions for the client
* To display the chat window and send and recieve messages gui style
*
* Authors : Danny Smith
*/
#pragma once
#include "../DCS Class Library/ChatWindowCommunication.h"
#include "conio.h"
#include "Client.h"
#include "ClientListeningServer.h"

#include <stdlib.h>
#include <vector>
#include <mutex>

#define LINE_COUNT 15
#define DEFAULT_DATE "2022-12-3"
#define BACKSPACE '\b'
#define ENTER '\r'
#define CHECK_INTERVAL 100
#define EXIT_COMMAND "exit\r"

// Function to send a message from the client
bool sendChatMessage(Client& client, std::string message) {
	/*
	* Send message to server
	*/
	MessagePacket msgPacket;
	char messageToSend[MAX_MESSAGE_SIZE] = {};
	strcpy_s(messageToSend, message.c_str());
	msgPacket.setMessage(messageToSend);
	msgPacket.setCurrDate();
	client.setCurrDate(msgPacket.getCurrDate());
	PacketManager pM(msgPacket.serialize());
	Packet* packet = pM.getPacket();

	if ( client.sendPacket(*packet) > 0 ) {
		return true;
	} else {
		return false;
	}
}

// Function to recieve a message from the server
bool recieveChatMessage(Client& client) {

	char RxBuffer[maxPacketSize] = {};

	int bytesReceived = recv(client.getClientSocket(), RxBuffer, maxPacketSize, 0);

	if ( bytesReceived > 0 ) {

		PacketManager pM(RxBuffer);
		if ( pM.getPacketType() == PacketType::packetMessage ) {
			MessagePacket* msgPacket = new MessagePacket(RxBuffer);
			client.setCurrMessage(msgPacket->getMessage());
			return true;
		}
		return false;
	} else {
		return false;
	}
	return true;

}

// Function to send a message from the server
bool sendServerMessage(Server& server, SOCKET& clientSocket, std::string message) {

	MessagePacket msgPacket;
	char messageToSend[MAX_MESSAGE_SIZE] = {};
	strcpy_s(messageToSend, message.c_str());
	msgPacket.setMessage(messageToSend);

	PacketManager pM(msgPacket.serialize());
	Packet* packet = pM.getPacket();

	if ( server.sendPacket(*packet, clientSocket) > 0 ) {
		return true;
	} else {
		return false;
	}
}

// Function to recieve a message from the client
bool recieveServerMessage(Server& server, SOCKET& clientSocket) {

	char RxBuffer[maxPacketSize] = {};

	int bytesReceived = recv(clientSocket, RxBuffer, maxPacketSize, 0);

	if ( bytesReceived > 0 ) {

		PacketManager pM(RxBuffer);
		if ( pM.getPacketType() == PacketType::packetMessage ) {
			MessagePacket* msgPacket = new MessagePacket(RxBuffer);
			server.setCurrMessage(msgPacket->getMessage());
			return true;
		}
		return false;
	} else {
		return false;
	}
	return true;
}

// Function to print to a specific coordinate on the screen
void printToCoordinates(int y, int x, char* text)
{
	printf("\033[%d;%dH%s", y, x, text);
}

class ChatWindow {
private:
	std::vector<ChatWindowCommunication> chats;
	std::mutex lock;
	bool hasUpdate = false;
	bool termination_pending = false;
	bool connected = false;
public:
	std::string message = "";
	ChatWindow() {

	}
	void addChat(char* date, std::string message) {
		lock.lock();
		ChatWindowCommunication newChat;
		newChat.setMessage(date + message);
		chats.push_back(newChat);
		hasUpdate = true;
		lock.unlock();
	}
	void updateWindow() {
		// CLEAR Screen
		std::system("cls");
		lock.lock();
		// load last LINE_COUNT messages into a std::vector
		std::vector<ChatWindowCommunication> lastChats;
		int counter = 0;
		for ( int i = this->chats.size() - 1; i >= 0; i-- ) {
			lastChats.push_back(chats[i]);
			counter++;
			if ( counter == LINE_COUNT ) {
				break;
			}
		}
		hasUpdate = false;
		lock.unlock();
		int index = 1;

		for ( int i = lastChats.size() - 1; i >= 0; i-- ) {
			printToCoordinates(index, 0, (char*)lastChats[i].getMessage().c_str());
			index++;
		}
		std::string output_message = "Enter message: " + this->message;
		printToCoordinates(LINE_COUNT + 1, 0, (char*)output_message.c_str());
	}
	bool HasUpdate() {
		return hasUpdate;
	}
	void updated() {
		hasUpdate = true;
	}
	std::vector<ChatWindowCommunication> getChats() {
		return chats;
	}
	void terminate() {
		termination_pending = true;
	}
	bool isTerminating() {
		return termination_pending;
	}
	void connect() {
		connected = true;
	}
	void disconnect() {
		connected = false;
	}
	bool isConnected() {
		return connected;
	}
};

void UpdateWindow(ChatWindow& window) {
	while ( !window.isTerminating() || window.HasUpdate() ) {
		std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL));
		if ( window.HasUpdate() ) {
			window.updateWindow();
		}
	}
}

void listener(ChatWindow& window, Client& chatClient, std::string& message) {

	while ( (!window.isTerminating() || window.HasUpdate()) && message != EXIT_COMMAND ) {
		// if message received
		chatClient.setTimeout(1);
		// wait for message
		if ( recieveChatMessage(chatClient) ) {
			window.addChat((char*)"", chatClient.getCurrMessage());
		}
		chatClient.clearCurrMessage();
		// if message is exit command and server is disconnected
		if ( message == EXIT_COMMAND && window.isConnected()) {
			sendChatMessage(chatClient, "[" + chatClient.getDroneID() + "] " + "Drone has disconnected");
		}
		chatClient.setTimeout(5000);
	}
	Sleep(1000);
}

int runChatWindow(Client& chatClient) {
	system("cls");
	ChatWindow CHAT;
	CHAT.connect();
	std::thread t1 = std::thread([&]() { UpdateWindow(CHAT); });
	std::thread t2 = std::thread([&]() { listener(CHAT, chatClient, CHAT.message); });
	std::string message = "";
	while ( true )
	{
		// get message from user
		std::string output_message = "Enter message: " + message;
		printToCoordinates(LINE_COUNT + 1, 0, (char*)output_message.c_str());
		char user_character = _getch();
		CHAT.updated();
		message += user_character;
		if ( user_character == ENTER) {
			if ( message == EXIT_COMMAND) {
				CHAT.message = message;
				CHAT.disconnect();
				CHAT.terminate();
				break;
			} else {
				//send message to server
				std::string add_to_chat = "[" + chatClient.getDroneID() + "] " + CHAT.message;
				sendChatMessage(chatClient, add_to_chat);
				CHAT.addChat((char*)chatClient.getCurrDate().c_str()," - "+ add_to_chat);
			}
			message = "";
		} else if ( user_character == BACKSPACE) {
			if ( message.size() > 1 ) {
				message.pop_back();
				message.pop_back();
			}
		}
		CHAT.message = message;
	}
	CHAT.addChat((char*)chatClient.getCurrDate().c_str(), (char*)"Goodbye!");
	t1.join();
	t2.join();
	return 0;
}


