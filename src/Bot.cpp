#include "Bot.hpp"
#include "Proto.hpp"
#include "State.hpp"
#include <iostream>

#ifdef FTIRC_WITH_CURL
	#include <curl/curl.h>

struct WriteCtx {
	std::string* buf;
	size_t total;
	WriteCtx(std::string* b) : buf(b), total(0) {}
};

static void append_utf8(std::string &out, unsigned int cp) {
	if (cp <= 0x7F) out.push_back((char)cp);
	else if (cp <= 0x7FF) {
		out.push_back((char)(0xC0 | ((cp >> 6) & 0x1F)));
		out.push_back((char)(0x80 | (cp & 0x3F)));
	} else if (cp <= 0xFFFF) {
		out.push_back((char)(0xE0 | ((cp >> 12) & 0x0F)));
		out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back((char)(0x80 | (cp & 0x3F)));
	} else {
		out.push_back((char)(0xF0 | ((cp >> 18) & 0x07)));
		out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back((char)(0x80 | (cp & 0x3F)));
	}
}

static unsigned int hex4(const std::string &s, size_t i) {
	unsigned int v=0;
	for (size_t k=0;k<4 && i+k<s.size();++k) {
		char c = s[i+k];
		v <<= 4;
		if (c>='0' && c<='9') v |= (c-'0');
		else if (c>='a' && c<='f') v |= (c-'a'+10);
		else if (c>='A' && c<='F') v |= (c-'A'+10);
	}
	return v;
}

static std::string unescape_json_string(const std::string& raw) {
	std::string out; out.reserve(raw.size());
	for (size_t i=0;i<raw.size();++i) {
		if (raw[i]=='\\' && i+1<raw.size()) {
			char n = raw[i+1];
			if (n=='n'){ out.push_back('\n'); ++i; continue; }
			if (n=='r'){ out.push_back('\r'); ++i; continue; }
			if (n=='t'){ out.push_back('\t'); ++i; continue; }
			if (n=='"'||n=='\\'){ out.push_back(n); ++i; continue; }
			if (n=='u' && i+5<raw.size()) {
				unsigned int code = hex4(raw, i+2);
				i += 5; // skip \uXXXX
				// surrogate pair?
				if (code >= 0xD800 && code <= 0xDBFF && i+6<raw.size() &&
					raw[i+1]=='\\' && raw[i+2]=='u') {
					unsigned int low = hex4(raw, i+3);
					if (low >= 0xDC00 && low <= 0xDFFF) {
						code = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
						i += 6; // skip second \uXXXX
					}
				}
				append_utf8(out, code);
				continue;
			}
		}
		out.push_back(raw[i]);
	}
	return out;
}

static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
	WriteCtx* ctx = static_cast<WriteCtx*>(userdata);
	size_t bytes = size * nmemb;
	ctx->buf->append(static_cast<const char*>(ptr), bytes);
	ctx->total += bytes;
	std::cerr << "[write_cb] chunk=" << bytes << "B, total=" << ctx->total << "B\n";
	return bytes;
}

static std::string extract_output_text_min(const std::string& resp) {
	std::cerr << "[extract] len=" << resp.size() << "\n";
	std::string collected;

	const std::string type_key = "\"type\": \"output_text\"";
	const std::string text_key = "\"text\": \"";

	size_t pos = resp.find(type_key);
	while (pos != std::string::npos) {
		std::cerr << "[extract] found output_text at pos=" << pos
				<< " head(50)=" << resp.substr(pos, 50) << "...\n";

		// look for the first "text":"
		size_t tx = resp.find(text_key, pos + type_key.size());
		if (tx != std::string::npos) {
			size_t t1 = resp.find('"', tx + text_key.size() - 1);
			if (t1 != std::string::npos) {
				size_t t2 = t1 + 1;
				while (t2 < resp.size() && !(resp[t2] == '"' && resp[t2 - 1] != '\\')) ++t2;
				if (t2 < resp.size()) {
					std::string raw = resp.substr(t1 + 1, t2 - (t1 + 1));
					std::string out = unescape_json_string(raw);

					if (!out.empty()) {
						if (!collected.empty()) collected.push_back('\n');
						collected += out;
					}
				}
			}
		}

		// continue searching for the next output_text
		pos = resp.find(type_key, pos + type_key.size());
	}

	return collected;
}
#endif // FTIRC_WITH_CURL


int Register_bot_ex(std::map<int, Client>& clients,
					const std::string& nick,
					const std::string& user,
					const std::string& realname)
{
	// If a client with this nick already exists (should not happen), return its fd
	std::map<int, Client>::const_iterator it = clients.begin();
	for (; it != clients.end(); ++it) {
		if (it->second.nick == nick) {
			return it->first; // already registered
		}
	}

	// Find an unused negative "fd" slot.
	int bot_fd = -1;
	while (clients.find(bot_fd) != clients.end()) {
		--bot_fd;
	}

	Client bot;
	bot.fd         = bot_fd;     // sentinel: never polled/recv
	bot.nick       = nick;
	bot.user       = user;
	bot.realname   = realname;
	bot.pass_ok    = true;       // as if PASS was accepted
	bot.registered = true;       // as if NICK/USER completed

	clients[bot_fd] = bot;
	return bot_fd;
}

// Convenience wrapper with the usual defaults
int Register_bot(std::map<int, Client>& clients)
{
	int botfd = Register_bot_ex(clients, "bot", "service", "ft_irc helper bot");
	g_state.nick2fd["bot"] = botfd;
	return botfd;
}

Bot* Bot::s_instance_ = 0;

std::string Bot::read_api_key_from_envfile() {
	std::ifstream f(".env");
	if (!f) return std::string();
	std::string line;
	while (std::getline(f, line)) {
		if (line.size() > 8 && line.compare(0, 8, "API_KEY=") == 0) {
			std::string val = line.substr(8);
			while (!val.empty() &&
				(val[val.size()-1] == '\r' || val[val.size()-1] == '\n' || val[val.size()-1] == ' '))
				val.erase(val.size()-1);
			return val;
		}
	}
	return std::string();
}

Bot::Bot()
: model_("gpt-5-nano")
, endpoint_("https://api.openai.com/v1/responses")
, api_key_()
, nick_("bot")
, curl_ready_(false)
{
	std::string raw = read_api_key_from_envfile();
	if (!raw.empty())
		api_key_ = "Bearer " + raw;
#ifdef FTIRC_WITH_CURL
	if (curl_global_init(CURL_GLOBAL_DEFAULT) == 0)
		curl_ready_ = true;
#endif
}

Bot::~Bot() {
	if (curl_ready_) {
#ifdef FTIRC_WITH_CURL
		curl_global_cleanup();
#endif
		curl_ready_ = false;
	}
}

Bot& Bot::Instance() {
	if (!s_instance_) s_instance_ = new Bot();
	return *s_instance_;
}

void Bot::Destroy() {
	if (s_instance_) {
		delete s_instance_;
		s_instance_ = 0;
	}
}

void Bot::ReplyTo(std::map<int, Client>& clients, int bot_fd, int to_fd, const std::string& text, const std::string& cmd) const {
	const std::string to_nick = clients[to_fd].nick;
	// Forge ":bot PRIVMSG <nick> :<text>"
	const std::string line = user_prefix(clients.at(bot_fd)) + " " + cmd + " " + to_nick + " :" + text;
	enqueue_line(clients, to_fd, line);
}

void Bot::OnPrivmsg(std::map<int, Client>& clients, int bot_fd, int from_fd, const std::string& msg, const std::string& cmd) {
	if (msg == "help") {
		ReplyTo(clients, bot_fd, from_fd, "commands: help, ping, gpt <prompt>", cmd);
	} else if (msg == "ping") {
		ReplyTo(clients, bot_fd, from_fd, "pong", cmd);
	} else if (msg.size() > 4 && msg.substr(0,4) == "gpt ") {
		std::string prompt = msg.substr(4);
		std::string out = Complete(prompt);
		if (out.empty()) out = "Sorry, The Void is silent. But the time is....\n\n  ..now.";
		ReplyTo(clients, bot_fd, from_fd, out, cmd);
	} else {
		ReplyTo(clients, bot_fd, from_fd, "unknown command. try: help", cmd);
	}
}

// simple wrapper
void Bot_handle_message(std::map<int, Client>& clients,
						int bot_fd,
						Client& from_client,
						int from_fd,
						const std::string& text, const std::string& cmd)
{
	(void)from_client;
	std::cout << "[Bot] got message from fd=" << from_fd << ": " << text << std::endl;
	Bot::Instance().OnPrivmsg(clients, bot_fd, from_fd, text, cmd);
}


std::string Bot::Complete(const std::string& prompt) const {
#ifndef FTIRC_WITH_CURL
	(void)prompt;
	return "sorry, no curl";
#else
	// ---- sanity checks ----
	if (!curl_ready_) {
		std::cerr << "[Bot::Complete] curl not initialized\n";
		return std::string();
	}
	if (api_key_.empty()) {
		std::cerr << "[Bot::Complete] API key is empty (did .env load?)\n";
		return std::string();
	}
	if (model_.empty()) {
		std::cerr << "[Bot::Complete] model_ empty\n";
		return std::string();
	}
	if (endpoint_.empty()) {
		std::cerr << "[Bot::Complete] endpoint_ empty\n";
		return std::string();
	}

	CURL* curl = curl_easy_init();
	if (!curl) {
		std::cerr << "[Bot::Complete] curl_easy_init() failed\n";
		return std::string();
	}

	std::string escaped;
	escaped.reserve(prompt.size()+16);
	for (size_t i=0;i<prompt.size();++i) {
		char c = prompt[i];
		if (c=='\\' || c=='"') { escaped.push_back('\\'); escaped.push_back(c); }
		else if (c=='\n') escaped += "\\n";
		else if (c=='\r') escaped += "\\r";
		else escaped.push_back(c);
	}

	std::ostringstream body;
	body << "{"
		<< "\"model\":\"" << model_ << "\","
		<< "\"input\":\"" << escaped << "\""
		<< "}";

	struct curl_slist* headers = 0;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, ("Authorization: " + api_key_).c_str());

	std::string response;
	curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	const std::string body_str = body.str();
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
	WriteCtx wc(&response);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wc);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 25L);

	CURLcode rc = curl_easy_perform(curl);
	long code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK || code/100 != 2) {
		std::cerr << "[Bot::Complete] curl_easy_perform() failed: "
				<< curl_easy_strerror(rc)
				<< " HTTP code=" << code << "\n";
		return std::string(); // failure
	}

	return extract_output_text_min(response);
#endif
}