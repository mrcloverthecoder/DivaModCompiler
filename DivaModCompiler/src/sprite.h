#pragma once

#include <stdint.h>
#include <string>
#include <vector>

namespace Sprite
{
	struct SpriteInfo
	{
		std::string Name;
		std::string File;
		int32_t InternalId = -1;
	};

	struct SpriteSetInfo
	{
		std::string Name;
		std::vector<SpriteInfo> Sprites;
	};

	using SpriteSetList = std::vector<Sprite::SpriteSetInfo>;

	void CompileSpriteSetsWithDB(std::string& outputPath, SpriteSetList& info);
	bool CompileSpriteData(std::string& rootPath, std::string& outputPath, SpriteSetList& cumulativeSetsInfo);
}