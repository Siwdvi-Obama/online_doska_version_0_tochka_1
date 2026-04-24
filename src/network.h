#ifndef NETWORK_H
#define NETWORK_H

#include <string>

// Initialize network client (call once before using other functions)
// Returns true if connection succeeded.
bool init_network(const std::string& server_ip, unsigned short port);

// Send a text message to the server (will be broadcast to all clients)
void send_message(const std::string& msg);

// Check if a new message has arrived from the server
bool has_message();

// Retrieve the oldest pending message (call only if has_message() returns true)
std::string get_message();

// Check if still connected to the server
bool is_connected();

// Close connection and clean up (called automatically on program exit)
void close_network();

#endif