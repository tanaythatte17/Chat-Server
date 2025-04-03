#include "database.h"
#include <iostream>

using namespace std;

Database::Database(const string& server, const string& username, const string& password, const string& database) {
    try {
        driver = get_driver_instance();
        con = driver->connect(server, username, password);
        con->setSchema(database);
        db_name = database;
        cout << "Connected to database: " << database << endl;
    }
    catch (sql::SQLException& e) {
        cerr << "Database connection failed: " << e.what() << endl;
        exit(1);
    }
}

Database::~Database() {
    closeConnection();
}

void Database::insertMessage(int clientID, const string& message) {
    try {
        sql::PreparedStatement* pstmt = con->prepareStatement("INSERT INTO messages(client_id, message) VALUES (?, ?)");
        pstmt->setInt(1, clientID);
        pstmt->setString(2, message);
        pstmt->execute();
        delete pstmt;
    }
    catch (sql::SQLException& e) {
        cerr << "Insert error: " << e.what() << endl;
    }
}
bool Database::loginUser(const string& username, const string& password) {
	try {
		sql::PreparedStatement* pstmt = con->prepareStatement("SELECT * FROM users WHERE username = ? and password = ?");
		pstmt->setString(1, username);
		pstmt->setString(2, password);
		pstmt->execute();
        sql::ResultSet* res = pstmt->executeQuery();
        if (res->next()) {
            delete pstmt;
            delete res;
            return true;
        }
        else {
            delete pstmt;
            delete res;
            return false;
        }
	}
	catch (sql::SQLException& e) {
		cerr << "Insert error: " << e.what() << endl;
	}
}
bool Database::registerUser(const string& username, const string& password) {
    try {
        sql::PreparedStatement* pstmt = con->prepareStatement("SELECT * FROM users WHERE username = ?");
        pstmt->setString(1, username);
        pstmt->execute();
        sql::ResultSet* res = pstmt->executeQuery();
        if (res->next()) {
            delete pstmt;
            delete res;
            return false;
        }
        else {
            delete pstmt;
            delete res;
            pstmt = con->prepareStatement("INSERT INTO users(username, password) VALUES (?, ?)");
            pstmt->setString(1, username);
            pstmt->setString(2, password);
            pstmt->execute();
            delete pstmt;
            return true;
        }
    }
    catch (sql::SQLException& e) {
        cerr << "Insert error: " << e.what() << endl;
    }
}
vector<string> Database::getMessages() {
    vector<string> messages;
    try {
        sql::Statement* stmt = con->createStatement();
        sql::ResultSet* res = stmt->executeQuery("SELECT message FROM messages");

        while (res->next()) {
            messages.push_back(res->getString("message"));
        }

        delete res;
        delete stmt;
    }
    catch (sql::SQLException& e) {
        cerr << "Retrieve error: " << e.what() << endl;
    }
    return messages;
}

void Database::closeConnection() {
    if (con) {
        delete con;
        con = nullptr;
    }
}
