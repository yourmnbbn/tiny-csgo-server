#include "argparser.hpp"
#include "GCClient.hpp"
#include "server.hpp"

int main(int argc, char** argv)
{
	ArgParser parser;

	parser.AddOption("-version", "CSGO Server version", OptionAttr::RequiredWithValue, OptionValueType::STRING);
	parser.AddOption("-port", "Server listening port", OptionAttr::RequiredWithValue, OptionValueType::INT16U);
	parser.AddOption("-gslt", "Game server logon token", OptionAttr::OptionalWithValue, OptionValueType::STRING);
	parser.AddOption("-rdip", "Redirect IP address (e.g. 127.0.0.1:27015)", OptionAttr::OptionalWithValue, OptionValueType::STRING);
	parser.AddOption("-vac", "Enable VAC?", OptionAttr::OptionalWithoutValue, OptionValueType::NONE);
	parser.AddOption("-mirror", "Enable mirroring server info from redrecting server?", OptionAttr::OptionalWithoutValue, OptionValueType::NONE);


	try
	{
		parser.ParseArgument(argc, argv);
	}
	catch (const std::exception& e)
	{
		printf("%s\n", e.what());
		return -1;
	}

	if (parser.HasOption("-mirror") && !parser.HasOption("-rdip"))
	{
		printf("When -mirror is enabled, you have to provide a redirect server socket by option -rdip\n");
		return -1;
	}

	Server sv(parser);
	sv.InitializeServer();
	sv.RunServer();
	return 0;
}

