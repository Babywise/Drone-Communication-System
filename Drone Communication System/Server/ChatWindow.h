#pragma once
#include "ChatWindowCommunication.h"
#include "conio.h"
#include "Server.h"

#include <stdlib.h>
#include <vector>
#include <mutex>

#define LINE_COUNT 15

bool sendChatMessage(Server& server, SOCKET& clientSocket, std::string message) {

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

bool recieveChatMessage(Server& server, SOCKET& clientSocket) {

	char RxBuffer[maxPacketSize] = {};

	int bytesReceived = recv(clientSocket, RxBuffer, maxPacketSize, 0);

	if ( bytesReceived > 0 ) {

		PacketManager pM(RxBuffer);
		if ( pM.getPacketType() == PacketType::packetMessage ) {
			MessagePacket* msgPacket = new MessagePacket(RxBuffer);
			server.setCurrMessage(msgPacket->getMessage());
			//std::cout << server.getDroneID() << "|(Client): " << message << "\n";
			return true;
		}
		return false;
	} else {
		return false;
	}
	return true;
}

void printToCoordinates(int y, int x, char* text)
{
	printf("\033[%d;%dH%s\n", y, x, text);
}

class ChatWindow {
private:
	std::vector<ChatWindowCommunication> chats;
	std::mutex lock;
	bool hasUpdate = false;
	bool termination_pending = false;
public:
	std::string message = "";
	ChatWindow() {

	}
	void addChat(char* date, std::string message) {
		lock.lock();
		ChatWindowCommunication newChat;
		newChat.setMessage(message);
		chats.push_back(newChat);
		hasUpdate = true;
		lock.unlock();
	}
	void updateWindow() {
		// CLEAR Screen
		printf("\033[2J\033[1;1H");
		lock.lock();
		// load last LINE_COUNT messages into a vector
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
};

void UpdateWindow(ChatWindow& window) {
	while ( !window.isTerminating() || window.HasUpdate() ) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if ( window.HasUpdate() ) {
			window.updateWindow();
		}
	}
}

void listener(ChatWindow& window, Server& chatClient, SOCKET& clientSocket) {

	while ( !window.isTerminating() || window.HasUpdate() ) {
		// if message received
		if ( recieveChatMessage(chatClient, clientSocket) ) {
			window.addChat((char*)"2022-12-3", chatClient.getCurrMessage());
		}
		chatClient.clearCurrMessage();
	}
}

int runChatWindow(Server& chatClient, SOCKET& clientSocket) {
	ChatWindow CHAT;
	std::thread t1 = std::thread([&]() { UpdateWindow(CHAT); });
	std::thread t2 = std::thread([&]() { listener(CHAT, chatClient, clientSocket); });
	std::string message = "";
	while ( true )
	{
		// get message from user
		std::string output_message = "Enter message: " + message;
		printToCoordinates(LINE_COUNT + 1, 0, (char*)output_message.c_str());
		char user_character = _getch();
		CHAT.updated();
		message += user_character;
		if ( user_character == '\r' ) {
			if ( message == "exit\r" ) {
				CHAT.terminate();
				break;
			} else {
				//send message to server
				sendChatMessage(chatClient, clientSocket, CHAT.message);
				std::string add_to_chat = CHAT.message;
				CHAT.addChat((char*)"2022-12-3", message);
			}
			message = "";
		} else if ( user_character == '\b' ) {
			if ( message.size() > 1 ) {
				message.pop_back();
				message.pop_back();
			}
		}
		CHAT.message = message;
	}
	CHAT.addChat((char*)"2022-12-3", (char*)"Goodbye!");
	t1.join();
	t2.join();
	return 0;
}

