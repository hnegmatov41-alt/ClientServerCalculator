// client.cpp
#include <iostream>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

SOCKET connectToServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5555);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    int result = connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cout << "Ошибка подключения к серверу!\n";
        return INVALID_SOCKET;
    }
    
    return clientSocket;
}

void showMainMenu() {
    std::cout << "\n=== КЛИЕНТ-СЕРВЕРНЫЙ КАЛЬКУЛЯТОР ===\n";
    std::cout << "1. Войти в систему\n";
    std::cout << "2. Регистрация\n";
    std::cout << "3. Выход\n";
    std::cout << "Выберите действие (1-3): ";
}

void showCalcMenu() {
    std::cout << "\n=== РЕЖИМ КАЛЬКУЛЯТОРА ===\n";
    std::cout << "Введите выражение (например: 2+2, 5*3, 10/2)\n";
    std::cout << "Или команду:\n";
    std::cout << "  stats - показать статистику\n";
    std::cout << "  exit - выйти из аккаунта\n";
    std::cout << "> ";
}

int main() {
    setlocale(LC_ALL, "Russian");
    
    SOCKET sock = connectToServer();
    if (sock == INVALID_SOCKET) {
        std::cout << "Не удалось подключиться к серверу. Убедись, что сервер запущен.\n";
        system("pause");
        return 1;
    }
    
    std::cout << "Подключено к серверу!\n";
    
    char buffer[4096];
    bool loggedIn = false;
    
    while (true) {
        if (!loggedIn) {
            showMainMenu();
            
            int choice;
            std::cin >> choice;
            std::cin.ignore();
            
            if (choice == 3) {
                std::cout << "Выход из программы.\n";
                break;
            }
            
            std::string login, password;
            std::cout << "Логин: ";
            std::getline(std::cin, login);
            
            std::cout << "Пароль: ";
            char ch;
            while ((ch = _getch()) != '\r') {
                if (ch == '\b') {
                    if (!password.empty()) {
                        password.pop_back();
                        std::cout << "\b \b";
                    }
                } else {
                    password += ch;
                    std::cout << '*';
                }
            }
            std::cout << "\n";
            
            if (choice == 1) {
                // Вход
                std::string msg = "LOGIN:" + login + ":" + password;
                send(sock, msg.c_str(), msg.length(), 0);
                
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer) - 1, 0);
                
                if (std::string(buffer) == "OK") {
                    std::cout << "✅ Вход выполнен успешно!\n";
                    loggedIn = true;
                } else {
                    std::cout << "❌ Ошибка входа! Неверный логин или пароль.\n";
                }
            }
            else if (choice == 2) {
                // Регистрация
                std::string msg = "REGISTER:" + login + ":" + password;
                send(sock, msg.c_str(), msg.length(), 0);
                
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer) - 1, 0);
                
                if (std::string(buffer) == "OK") {
                    std::cout << "✅ Регистрация успешна! Теперь можете войти.\n";
                } else {
                    std::cout << "❌ Ошибка регистрации! Логин уже занят.\n";
                }
            }
        }
        else {
            // Режим калькулятора
            showCalcMenu();
            
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "exit") {
                loggedIn = false;
                std::cout << "Выход из аккаунта...\n";
            }
            else if (input == "stats") {
                send(sock, "STATS", 5, 0);
                
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer) - 1, 0);
                
                std::cout << "\n" << buffer << "\n";
            }
            else if (!input.empty()) {
                // Отправляем выражение на сервер
                std::string msg = "CALC:" + input;
                send(sock, msg.c_str(), msg.length(), 0);
                
                memset(buffer, 0, sizeof(buffer));
                recv(sock, buffer, sizeof(buffer) - 1, 0);
                
                std::cout << "Результат: " << buffer << "\n";
            }
        }
    }
    
    closesocket(sock);
    WSACleanup();
    system("pause");
    return 0;
}