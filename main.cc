#include <iostream>
#include <fstream>

#include <boost/asio.hpp>

#include <discordpp/bot.hh>
#include <discordpp/plugin-overload.hh>
#include <discordpp/rest-beast.hh>
#include <discordpp/websocket-beast.hh>
#include <discordpp/plugin-responder.hh>
#include <discordpp/plugin-pretty.hh>

namespace asio = boost::asio;
using json = nlohmann::json;
namespace dpp = discordpp;

using DppBot =
dpp::PluginPretty<
		dpp::PluginResponder<
				dpp::PluginOverload<
						dpp::WebsocketBeast<
								dpp::RestBeast<
										dpp::Bot
								>
						>
				>
		>
>;

std::istream &safeGetline(std::istream &is, std::string &t);

void filter(std::string &target, const std::string &pattern);


int main(){
	std::cout << "Howdy, and thanks for trying out Discord++!\n"
	          << "Feel free to drop into the official server at https://discord.gg/0usP6xmT4sQ4kIDh if you have any questions.\n\n"
	          << std::flush;

	std::cout << "Starting bot...\n\n";

	/*/
	 * Read token from token file.
	 * Tokens are required to communicate with Discord, and hardcoding tokens is a bad idea.
	 * If your bot is open source, make sure it's ignore by git in your .gitignore file.
	/*/
	std::string token;
	{
		std::ifstream tokenFile("token.dat");
		if(!tokenFile){
			std::cerr << "CRITICAL: "
			          << "There is no valid way for Echo to obtain a token! "
			          << "Copy the example `token.eg.dat` as `token.dat` to make one.\n";
			exit(1);
		}
		safeGetline(tokenFile, token);
		tokenFile.close();
	}

	// Create Bot object
	auto bot = std::make_shared<DppBot>();
	// Don't complain about unhandled events
	bot->debugUnhandled = false;

	/*/
	 * Create handler for the READY payload, this may be handled by the bot in the future.
	 * The `self` object contains all information about the 'bot' user.
	/*/
	json self;
	bot->handlers.insert(
			{
					"READY",
					[&bot, &self](json data){
						self = data["user"];
					}
			}
	);

	bot->prefix = "~";

	bot->respond("help", "Mention me and I'll echo your message back!");

	bot->respond(
			"about", [&bot](json msg){
				std::ostringstream content;
				content << "Sure thing, "
				        << (msg["member"]["nick"].is_null() ? msg["author"]["username"].get<std::string>() : msg["member"]["nick"].get<std::string>())
				        << "!\n"
				        << "I'm a simple bot meant to demonstrate the Discord++ library.\n"
				        << "You can learn more about Discord++ at https://discord.gg/0usP6xmT4sQ4kIDh";
				bot->sendMessage(msg["channel_id"].get<std::string>(), content.str());
			}
	);

	// Create handler for the MESSAGE_CREATE payload, this receives all messages sent that the bot can see.
	bot->handlers.insert(
			{
					"MESSAGE_CREATE",
					[&bot, &self](json msg){
						// Scan through mentions in the message for self
						bool mentioned = false;
						for(const json &mention : msg["mentions"]){
							mentioned = mentioned or mention["id"] == self["id"];
						}
						if(mentioned){
							// Identify and remove mentions of self from the message
							std::string content = msg["content"];
							std::string mentioncode = "<@" + self["id"].get<std::string>() + ">";
							std::string nickedcode = "<@!" + self["id"].get<std::string>() + ">";
							filter(content, mentioncode + ' ');
							filter(content, nickedcode + ' ');
							filter(content, mentioncode);
							filter(content, nickedcode);

							while(content.find(mentioncode) != std::string::npos){
								content = content.substr(0, content.find(mentioncode)) +
								          content.substr(content.find(mentioncode) + mentioncode.size());
							}

							// Echo the created message
							bot->sendMessage(msg["channel_id"].get<std::string>(), content);

							// Set status to Playing "with [author]"
							dpp::Activity activity;
							{
								activity.name =  (
										msg["member"]["nick"].is_null()
										? msg["author"]["username"].get<std::string>()
										: msg["member"]["nick"].get<std::string>()
								);
								activity.type = dpp::Activity::Listening;
								activity.created_at = std::time(nullptr);
							}
							dpp::Status status;
							{
								status.since = 0;
								status.game = activity;
								status.status = dpp::Status::online;
								status.afk = false;
							}
							bot->setStatus(status);
						}
					}
			}
	);

	// Create Asio context, this handles async stuff.
	auto aioc = std::make_shared<asio::io_context>();

	// Set the bot up
	bot->initBot(6, token, aioc);

	// Run the bot!
	bot->run();

	return 0;
}

/*/
 * Source: https://stackoverflow.com/a/6089413/1526048
/*/
std::istream &safeGetline(std::istream &is, std::string &t){
	t.clear();

	// The characters in the stream are read one-by-one using a std::streambuf.
	// That is faster than reading them one-by-one using the std::istream.
	// Code that uses streambuf this way must be guarded by a sentry object.
	// The sentry object performs various tasks,
	// such as thread synchronization and updating the stream state.

	std::istream::sentry se(is, true);
	std::streambuf *sb = is.rdbuf();

	for(;;){
		int c = sb->sbumpc();
		switch(c){
			case '\n':
				return is;
			case '\r':
				if(sb->sgetc() == '\n'){
					sb->sbumpc();
				}
				return is;
			case std::streambuf::traits_type::eof():
				// Also handle the case when the last line has no line ending
				if(t.empty()){
					is.setstate(std::ios::eofbit);
				}
				return is;
			default:
				t += (char)c;
		}
	}
}

void filter(std::string &target, const std::string &pattern){
	while(target.find(pattern) != std::string::npos){
		target = target.substr(0, target.find(pattern)) +
		         target.substr(target.find(pattern) + (pattern).size());
	}
}