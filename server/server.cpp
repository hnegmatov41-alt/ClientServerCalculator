// server.cpp
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <libpq-fe.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libpq.lib")

// Данные для подключения к PostgreSQL - ЗДЕСЬ ЗАМЕНИ ПАРОЛЬ
const char* conninfo = "host=localhost port=5432 dbname=calculator_db user=postgres password=hftillo190407";

// Проверка подключения к БД
bool checkDB() {
    PGconn* conn = PQconnectdb(conninfo);
    bool status = (PQstatus(conn) == CONNECTION_OK);
    PQfinish(conn);
    return status;
}

// Создание таблиц при запуске
void initDB() {
    const char* sql_users = "CREATE TABLE IF NOT EXISTS users ("
        "id SERIAL PRIMARY KEY, "
        "login VARCHAR(50) UNIQUE NOT NULL, "
        "password VARCHAR(100) NOT NULL, "
        "role VARCHAR(20) DEFAULT 'user');";

    const char* sql_history = "CREATE TABLE IF NOT EXISTS history ("
        "id SERIAL PRIMARY KEY, "
        "user_id INTEGER REFERENCES users(id), "
        "expression TEXT, "
        "result TEXT, "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";

    PGconn* conn = PQconnectdb(conninfo);
    PQexec(conn, sql_users);
    PQexec(conn, sql_history);
    PQfinish(conn);

    std::cout << "База данных инициализирована\n";
}

// Регистрация пользователя
bool registerUser(const std::string& login, const std::string& password) {
    std::string sql = "INSERT INTO users (login, password) VALUES ('" +
        login + "', '" + password + "');";
    PGconn* conn = PQconnectdb(conninfo);
    PGresult* res = PQexec(conn, sql.c_str());
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    PQfinish(conn);
    return success;
}

// Авторизация пользователя
int loginUser(const std::string& login, const std::string& password) {
    std::string sql = "SELECT id FROM users WHERE login='" + login +
        "' AND password='" + password + "';";
    PGconn* conn = PQconnectdb(conninfo);
    PGresult* res = PQexec(conn, sql.c_str());

    int user_id = -1;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        user_id = std::stoi(PQgetvalue(res, 0, 0));
    }

    PQclear(res);
    PQfinish(conn);
    return user_id;
}

// Добавление в историю
void addHistory(int user_id, const std::string& expr, const std::string& result) {
    std::string sql = "INSERT INTO history (user_id, expression, result) VALUES (" +
        std::to_string(user_id) + ", '" + expr + "', '" + result + "');";
    PGconn* conn = PQconnectdb(conninfo);
    PQexec(conn, sql.c_str());
    PQfinish(conn);
}

// Получение статистики
std::string getStats() {
    std::string result = "Статистика использования:\n";
    std::string sql = "SELECT u.login, COUNT(h.id) as ops "
        "FROM users u LEFT JOIN history h ON u.id = h.user_id "
        "GROUP BY u.login;";

    PGconn* conn = PQconnectdb(conninfo);
    PGresult* res = PQexec(conn, sql.c_str());

    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        result += std::string(PQgetvalue(res, i, 0)) + ": " +
            std::string(PQgetvalue(res, i, 1)) + " операций\n";
    }

    PQclear(res);
    PQfinish(conn);
    return result;
}

// Обработка одного клиента
void handleClient(SOCKET clientSocket) {
    char buffer[4096];
    int user_id = -1;

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;

        std::string msg(buffer);
        std::cout << "Получено: " << msg << std::endl;

        if (msg.rfind("REGISTER:", 0) == 0) {
            size_t pos1 = msg.find(':');
            size_t pos2 = msg.find(':', pos1 + 1);
            std::string login = msg.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string password = msg.substr(pos2 + 1);

            if (registerUser(login, password)) {
                send(clientSocket, "OK", 2, 0);
            }
            else {
                send(clientSocket, "ERROR", 5, 0);
            }
        }
        else if (msg.rfind("LOGIN:", 0) == 0) {
            size_t pos1 = msg.find(':');
            size_t pos2 = msg.find(':', pos1 + 1);
            std::string login = msg.substr(pos1 + 1, pos2 - pos1 - 1);
            std::string password = msg.substr(pos2 + 1);

            user_id = loginUser(login, password);
            if (user_id != -1) {
                send(clientSocket, "OK", 2, 0);
            }
            else {
                send(clientSocket, "ERROR", 5, 0);
            }
        }
        else if (msg.rfind("CALC:", 0) == 0 && user_id != -1) {
            std::string expr = msg.substr(5);
            std::string result;

            try {
                if (expr.find('+') != std::string::npos) {
                    size_t pos = expr.find('+');
                    double a = std::stod(expr.substr(0, pos));
                    double b = std::stod(expr.substr(pos + 1));
                    result = std::to_string(a + b);
                }
                else if (expr.find('-') != std::string::npos) {
                    size_t pos = expr.find('-');
                    double a = std::stod(expr.substr(0, pos));
                    double b = std::stod(expr.substr(pos + 1));
                    result = std::to_string(a - b);
                }
                else if (expr.find('*') != std::string::npos) {
                    size_t pos = expr.find('*');
                    double a = std::stod(expr.substr(0, pos));
                    double b = std::stod(expr.substr(pos + 1));
                    result = std::to_string(a * b);
                }
                else if (expr.find('/') != std::string::npos) {
                    size_t pos = expr.find('/');
                    double a = std::stod(expr.substr(0, pos));
                    double b = std::stod(expr.substr(pos + 1));
                    if (b != 0) result = std::to_string(a / b);
                    else result = "Error: divide by 0";
                }
                else {
                    result = "Error: unknown operation";
                }

                addHistory(user_id, expr, result);
                send(clientSocket, result.c_str(), result.length(), 0);
            }
            catch (...) {
                send(clientSocket, "ERROR", 5, 0);
            }
        }
        else if (msg.rfind("STATS", 0) == 0) {
            std::string stats = getStats();
            send(clientSocket, stats.c_str(), stats.length(), 0);
        }
    }

    closesocket(clientSocket);
}

int main() {
    std::cout << "Запуск сервера...\n";

    if (!checkDB()) {
        std::cout << "Ошибка подключения к PostgreSQL!\n";
        return 1;
    }

    initDB();

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5555);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 10);

    std::cout << "Сервер запущен на порту 5555\n";

    while (true) {
        SOCKET clientSocket = accept(serverSocket, NULL, NULL);
        std::cout << "Клиент подключился!\n";
        std::thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}