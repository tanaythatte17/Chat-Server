#ifndef DATABASE_H
#define DATABASE_H

#define WIN32_LEAN_AND_MEAN  // Prevents conflicts with Winsock
#include <winsock2.h>        // Ensure this loads before MySQL
#include <ws2tcpip.h>        // Needed for networking

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <string>
#include <vector>

using namespace std;

class Database {
private:
    sql::Driver* driver;
    sql::Connection* con;
    string db_name;

public:
    Database(const string& server, const string& username, const string& password, const string& database);
    ~Database();

    void insertMessage(int clientID, const string& message);
	bool loginUser(const string& username, const string& password);
    bool registerUser(const string& username, const string& password);
    vector<string> getMessages();

    void closeConnection();
};

#endif

