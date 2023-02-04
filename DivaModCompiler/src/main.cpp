#include <filesystem>
#include <string>
#include <diva_db.h>
#include <util_string.h>
#include "sprite.h"

// TODO: Load from config.toml
std::string ModsFolder = "./mods_102";
std::string SourceFolder = "rom_src";

int main()
{
	std::vector<std::string> modDirectories;
	for (auto& modDirectory : std::filesystem::directory_iterator(ModsFolder))
	{
		if (!modDirectory.is_directory())
			continue;

		modDirectories.push_back(modDirectory.path().string());
	}

	std::vector<Sprite::SpriteSetInfo> cumulativeSetsInfo;
	for (std::string& modRootDir : modDirectories)
	{
		// NOTE: Compile sprite data
		std::string modSrcSprFolder = modRootDir + "/" + SourceFolder + "/2d";
		std::string modSprFolder = modRootDir + "/rom/2d";
		if (!IO::Directory::Exists(modSprFolder))
			IO::Directory::Create(modSprFolder);

		Sprite::CompileSpriteData(modSrcSprFolder, modSprFolder, cumulativeSetsInfo);
	}

	std::string priorityFolder = ModsFolder + "/AAA - MERGER PRIORITY";
	std::string priority2dFolder = priorityFolder + "/rom/2d";
	if (!IO::Directory::Exists(priority2dFolder))
		IO::Directory::Create(priority2dFolder);
	Sprite::CompileSpriteSetsWithDB(priority2dFolder, cumulativeSetsInfo);

	return 0;
}
