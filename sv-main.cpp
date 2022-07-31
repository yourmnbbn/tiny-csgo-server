#include "argparser.hpp"
#include "GCClient.hpp"
#include "server.hpp"

int main(int argc, char** argv)
{
	ArgParser parser;

	parser.AddOption("-version", "CSGO Server version", OptionAttr::RequiredWithValue, OptionValueType::STRING);
	parser.AddOption("-port", "Server listening port", OptionAttr::RequiredWithValue, OptionValueType::INT16U);
	parser.AddOption("-gslt", "Game server logon token", OptionAttr::RequiredWithValue, OptionValueType::STRING);
	parser.AddOption("-rdip", "Redirect IP address (e.g. 127.0.0.1:27015)", OptionAttr::OptionalWithValue, OptionValueType::STRING);

	try
	{
		parser.ParseArgument(argc, argv);
	}
	catch (const std::exception& e)
	{
		printf("%s\n", e.what());
		return -1;
	}

	Server sv(parser);
	sv.InitializeServer();
	sv.RunServer();
}

