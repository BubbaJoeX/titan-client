// ======================================================================
// COMPLETE IMPLEMENTATION - Add to Jawa.cpp after line 234
// Copy this entire block and paste after the fileExists() function
// ======================================================================

// ----------------------------------------------------------------------

std::string Jawa::normalizePath(const std::string &path)
{
	std::string result = path;
	
	// Replace backslashes with forward slashes
	for (size_t i = 0; i < result.length(); ++i)
	{
		if (result[i] == '\\')
			result[i] = '/';
	}
	
	// Remove trailing slash
	while (!result.empty() && result[result.length() - 1] == '/')
	{
		result = result.substr(0, result.length() - 1);
	}
	
	// Remove double slashes (but preserve leading // for network paths)
	size_t pos = 0;
	while ((pos = result.find("//", pos)) != std::string::npos)
	{
		if (pos == 0)
		{
			pos = 2; // Skip network path prefix
		}
		else
		{
			result.erase(pos, 1);
		}
	}
	
	return result;
}

// ----------------------------------------------------------------------

std::string Jawa::joinPath(const std::string &base, const std::string &sub)
{
	std::string b = normalizePath(base);
	std::string s = normalizePath(sub);
	
	// Remove leading slash from sub
	while (!s.empty() && s[0] == '/')
	{
		s = s.substr(1);
	}
	
	if (b.empty())
		return s;
	if (s.empty())
		return b;
	
	return b + "/" + s;
}

// ======================================================================
// Now find and replace these sections in buildAssetCustomizationManager:
// ======================================================================

// FIND (around line 582-583):
	const std::string acmMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.mif";
	const std::string cimMiff = config.basePath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.mif";

// REPLACE WITH:
	std::string acmMiff = joinPath(joinPath(config.basePath, "customization"), "asset_customization_manager.mif");
	std::string cimMiff = joinPath(joinPath(config.basePath, "customization"), "customization_id_manager.mif");

// ======================================================================

// FIND (around line 593-594):
	const std::string acmIff = config.clientPath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "asset_customization_manager.iff";
	const std::string cimIff = config.clientPath + PATH_SEPARATOR + "customization" + PATH_SEPARATOR + "customization_id_manager.iff";

// REPLACE WITH:
	std::string acmIff = joinPath(joinPath(config.clientPath, "customization"), "asset_customization_manager.iff");
	std::string cimIff = joinPath(joinPath(config.clientPath, "customization"), "customization_id_manager.iff");
