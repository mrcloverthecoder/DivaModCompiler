#include <json.hpp>
#include <core_io.h>
#include <diva_archive.h>
#include <diva_db.h>
#include <util_string.h>
#include "comfy/texture_util.h"
#include "sprite.h"

using namespace Sprite;
using json = nlohmann::json;

const std::string BaseDataPath = ".";
const std::vector<const char*> CumulativeSetNames = { "SPR_SEL_PVTMB" };
uint32_t CurrentSpriteId = 85000;

static bool ParseSpriteInfo(std::string& rootPath, SpriteSetList& data, SpriteSetList& cumulativeData)
{
	// NOTE: Try to open and read all the data from `spr_info.json`
	std::string sprInfoPath = rootPath + "/spr_info.json";
	IO::FileBuffer buffer = IO::File::ReadAllData(sprInfoPath, true);
	if (buffer.Content == nullptr)
		return false;

	// NOTE: If it didn't fail reading it, parse the json
	json sprInfo = json::parse(buffer.Content.get());

	for (auto& srcSet : sprInfo["Sets"])
	{
		Sprite::SpriteSetInfo* setInfo = nullptr;
		std::string setName = srcSet["Name"];

		for (const char* name : CumulativeSetNames)
			if (strncmp(setName.c_str(), name, setName.size()) == 0)
				setInfo = &cumulativeData.emplace_back();

		if (setInfo == nullptr)
			setInfo = &data.emplace_back();

		setInfo->Name = srcSet["Name"];
		for (auto& srcSpr : srcSet["Sprites"])
		{
			auto& sprInfo = setInfo->Sprites.emplace_back();
			sprInfo.Name = srcSpr["Name"];
			sprInfo.File = rootPath + "/" + std::string(srcSpr["File"]);
			sprInfo.InternalId = srcSpr.value("InternalId", -1);
		}
	}

	return true;
}

static bool CheckSetInfoEligibleForPacking(Sprite::SpriteSetInfo& setInfo)
{
	for (auto& sprInfo : setInfo.Sprites)
	{
		if (sprInfo.File.empty())
			return false;

		if (!IO::File::Exists(sprInfo.File))
			return false;
	}
	return true;
}

static std::unique_ptr<Comfy::SprSet> PackSpriteSet(const Sprite::SpriteSetInfo& setInfo)
{
	Comfy::SprPacker packer;
	std::vector<std::unique_ptr<u8[]>> imgPixelData;
	std::vector<Comfy::SprMarkup> markups;

	// NOTE: Disable YCbCr texture encoding
	packer.Settings.AllowYCbCrTextures = false;

	for (auto& sprInfo : setInfo.Sprites)
	{
		std::unique_ptr<u8[]> imgData;
		ivec2 imgSize = { };

		Comfy::ReadImageFile(sprInfo.File, imgSize, imgData);
		imgPixelData.push_back(std::move(imgData));

		auto& markup = markups.emplace_back();
		markup.Name = sprInfo.Name;
		markup.RGBAPixels = imgPixelData.back().get();
		markup.Size = imgSize;
		markup.ScreenMode = Comfy::ScreenMode::HDTV1080;
		markup.Flags = Comfy::SprMarkupFlags_Compress;
	}

	return packer.Create(markups);
}

static int32_t GetSpriteIndex(const Comfy::SprSet& sprSet, std::string_view name)
{
	int32_t idx = 0;
	for (const auto& spr : sprSet.Sprites)
	{
		if (strncmp(name.data(), spr.Name.c_str(), name.size()) == 0)
			return idx;
		idx++;
	}

	return -1;
}

static std::string GetTextureNameWithNewIndex(std::string_view orgName, size_t newIndex)
{
	char newBuffer[0x100] = { '\0' };
	int32_t lastUndIndex = Util::String::GetLastIndex(orgName, '_');
	std::string strippedName = std::string(orgName.data(), lastUndIndex);
	sprintf_s(newBuffer, 0x100, "%s_%03zu", strippedName.c_str(), newIndex);
	return std::string(newBuffer);
}

static bool MergeBaseSpriteData(Comfy::SprSet& sprSet, Database::SpriteSetInfo& setInfo)
{
	std::string baseSprSetPath = BaseDataPath + "/base_" + Util::String::ToLower(setInfo.Name) + ".bin";
	std::string baseSprDbPath = BaseDataPath + "/base_spr_db.bin";

	if (!IO::File::Exists(baseSprSetPath) || !IO::File::Exists(baseSprDbPath))
		return false;

	auto baseSprSet = Comfy::LoadFile<Comfy::SprSet>(baseSprSetPath);
	IO::Reader r;
	r.FromFile(baseSprDbPath);
	Database::SpriteDatabase baseSprDb = { };
	baseSprDb.Parse(r);
	Database::SpriteSetInfo* baseSetInfo = baseSprDb.FindSpriteSetByName(setInfo.Name);
	setInfo.Id = baseSetInfo->Id;

	// NOTE: Merge sprite data and entries
	size_t texNum = sprSet.TexSet.Textures.size();
	size_t idx = 0;
	for (const auto& tex : baseSprSet->TexSet.Textures)
	{
		std::string a = tex->Name;
		// NOTE: Push textures from the base sprset into the merged one
		auto texNew = std::make_shared<Comfy::Tex>();
		texNew->Name = GetTextureNameWithNewIndex(tex->Name, texNum + idx);
		texNew->MipMapsArray = std::move(tex->MipMapsArray);
		sprSet.TexSet.Textures.push_back(std::move(texNew));

		// NOTE: Merge sprite database texture entry
		const auto& baseTexInfo = baseSetInfo->Textures[idx];
		auto& texInfo = setInfo.Textures.emplace_back();
		texInfo.Name = GetTextureNameWithNewIndex(baseTexInfo.Name, texNum + idx);
		texInfo.DataIndex = static_cast<int32_t>(texNum + idx);
		texInfo.Id = CurrentSpriteId++;

		idx++;
	}

	// NOTE: Merge sprite data and entries
	size_t sprNum = sprSet.Sprites.size();
	idx = 0;
	for (const auto& spr : baseSprSet->Sprites)
	{
		// NOTE: Add new sprite to the merged sprite set and copy the base one
		auto& newSpr = sprSet.Sprites.emplace_back();
		newSpr = spr;
		// NOTE: Fix texture index
		newSpr.TextureIndex += static_cast<int32_t>(texNum);

		// NOTE: Merge sprite database entry
		const auto& baseSprInfo = baseSetInfo->Sprites[idx];
		auto& sprInfo = setInfo.Sprites.emplace_back();
		sprInfo.Name = baseSprInfo.Name;
		sprInfo.DataIndex = static_cast<int32_t>(sprNum + idx);
		sprInfo.Id = CurrentSpriteId++;

		idx++;
	}

	return true;
}

void Sprite::CompileSpriteSetsWithDB(std::string& outputPath, SpriteSetList& info)
{
	Database::SpriteDatabase sprDatabase = { };

	for (auto& srcSetInfo : info)
	{
		// NOTE: Check if SpriteSetInfo is eligible for sprite packing.
		//       If not, skip packing and setting up database.
		if (!CheckSetInfoEligibleForPacking(srcSetInfo))
			continue;

		// NOTE: Create SpriteSet file
		auto sprSet = PackSpriteSet(srcSetInfo);
		
		// NOTE: Add this SpriteSet to our mod's SpriteDatabase
		Database::SpriteSetInfo& sprSetInfo = sprDatabase.SpriteSets.emplace_back();
		sprSetInfo.Name = srcSetInfo.Name;
		sprSetInfo.Filename = Util::String::ToLower(srcSetInfo.Name) + ".bin";
		sprSetInfo.Id = CurrentSpriteId++;

		// NOTE: Add the sprites' information to the SpriteSet entry
		for (auto& srcSprInfo : srcSetInfo.Sprites)
		{
			Database::SpriteDataInfo& sprInfo = sprSetInfo.Sprites.emplace_back();
			sprInfo.Name = srcSetInfo.Name + "_" + srcSprInfo.Name;
			sprInfo.DataIndex = GetSpriteIndex(*sprSet, srcSprInfo.Name);
			sprInfo.Id = CurrentSpriteId++;
		}

		int32_t texIndex = 0;
		for (auto& tex : sprSet->TexSet.Textures)
		{
			std::string texName = (tex->Name.size() > 0) ? tex->Name : "MERGE_NOCOMP_0";
			std::string texPrefix = "SPRTEX_" + std::string(&srcSetInfo.Name[4], srcSetInfo.Name.size() - 4);

			Database::SpriteDataInfo& texInfo = sprSetInfo.Textures.emplace_back();
			texInfo.Name = texPrefix + "_" + texName;
			texInfo.DataIndex = texIndex++;
			texInfo.Id = CurrentSpriteId++;
		}

		// NOTE: Try to merge base-game entries, if applicable
		MergeBaseSpriteData(*sprSet, sprSetInfo);

		// NOTE: Write SpriteSet
		IO::Writer writer = { };
		sprSet->Write(writer);

		// NOTE: Pack the SpriteSet into its farc
		Archive::FArcPacker farcPacker = { };
		Archive::FArcPacker::FArcFile file = { };
		file.Filename = Util::String::ToLower(srcSetInfo.Name) + ".bin";
		file.Data = writer.GetData();
		file.Size = writer.GetSize();

		farcPacker.AddFile(file);
		farcPacker.Flush(outputPath + "/" + Util::String::ToLower(srcSetInfo.Name) + ".farc", false);
	}

	// NOTE: Write SpriteDatabase file
	IO::Writer w;
	sprDatabase.Write(w);
	w.Flush(outputPath + "/mod_spr_db.bin");
}

bool Sprite::CompileSpriteData(std::string& rootPath, std::string& outputPath, SpriteSetList& cumulativeSetsInfo)
{
	std::vector<SpriteSetInfo> setsInfo;
	if (!ParseSpriteInfo(rootPath, setsInfo, cumulativeSetsInfo))
		return false;

	CompileSpriteSetsWithDB(outputPath, setsInfo);
	return true;
}