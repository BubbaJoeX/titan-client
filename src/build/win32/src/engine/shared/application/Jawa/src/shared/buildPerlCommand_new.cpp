std::string Jawa::buildPerlCommand(const JawaConfig &config, const std::string &script, const std::string &args)
{
	std::string command;
	
	// Set environment variable for datatables path
#ifdef _WIN32
	// Windows: use 'set VAR=value && command' syntax
	if (!config.basePath.empty())
	{
		std::string datatablesPath = config.basePath + "sys.shared\\compiled\\game\\datatables\\";
		command = "set SWG_DATATABLES_PATH=" + datatablesPath + " && ";
	}
#else
	// Linux: use 'VAR=value command' syntax
	if (!config.basePath.empty())
	{
		std::string datatablesPath = config.basePath + "sys.shared/compiled/game/datatables/";
		command = "SWG_DATATABLES_PATH=" + datatablesPath + " ";
	}
#endif
	
	command += config.perlInterpreter;
	
	// Add -I flag for Perl library path
	if (!config.perlLibPath.empty())
	{
		command += " -I\"" + config.perlLibPath + "\"";
	}
	
	command += " \"" + script + "\" " + args;
	return command;
}
