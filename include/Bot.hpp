#pragma once
#ifndef BOT_HPP
#define BOT_HPP
#include <string>
#include <map>
#include "Client.hpp"

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>



int Register_bot_ex(std::map<int, Client>& clients,
					const std::string& nick,
					const std::string& user,
					const std::string& realname);
int Register_bot(std::map<int, Client>& clients);
void Bot_handle_message(std::map<int, Client>& clients,
						int bot_fd,
						Client& from_client,
						int from_fd,
						const std::string& text, const std::string& cmd);

class Bot {
public:
	// ---- Singleton access ----
	static Bot& Instance();   // constructs on first call, then uses same instance
	static void Destroy();

	// ---- Bot API ----
	void SetNick(const std::string& n) { nick_ = n; }
	const std::string& Nick() const { return nick_; }

	void SetModel(const std::string& m) { model_ = m; }
	void SetEndpoint(const std::string& url) { endpoint_ = url; }

	void OnPrivmsg(std::map<int, Client>& clients, int bot_fd, int from_fd, const std::string& text, const std::string& cmd);
	std::string Complete(const std::string& prompt) const;

private:
	Bot();              // default ctor (reads .env)
	~Bot();             // dtor cleans curl
	Bot(const Bot&);            // no copy
	Bot& operator=(const Bot&); // no assign

	void ReplyTo(std::map<int, Client>& clients,int bot_fd, int to_fd, const std::string& text, const std::string& cmd) const;
	std::string read_api_key_from_envfile();

private:
	std::string model_;      // e.g. "gpt-4o-mini"
	std::string endpoint_;   // e.g. "https://api.openai.com/v1/responses"
	std::string api_key_;    // prefixed "Bearer ..."
	std::string nick_;
	bool curl_ready_;

	static Bot* s_instance_;
};

#endif