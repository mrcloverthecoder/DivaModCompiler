// MIT License
//
// Copyright(c) 2022 samyuu
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "file_format_spr_set.h"

namespace Comfy
{
	StreamResult Tex::Read(IO::Reader& reader)
	{
		reader.PushBaseOffset();

		const auto texSignature = static_cast<TxpSig>(reader.ReadUInt32());
		const auto mipMapCount = reader.ReadUInt32();

		const auto mipLevels = reader.ReadUInt8();
		const auto arraySize = reader.ReadUInt8();
		const auto depth = reader.ReadUInt8();
		const auto dimensions = reader.ReadUInt8();

		if (texSignature != TxpSig::Texture2D && texSignature != TxpSig::CubeMap)
			return StreamResult::BadFormat;

		const auto adjustedMipLevels = (texSignature == TxpSig::CubeMap) ? (mipMapCount / arraySize) : mipMapCount;

		MipMapsArray.reserve(arraySize);
		for (size_t i = 0; i < arraySize; i++)
		{
			auto& mipMaps = MipMapsArray.emplace_back();
			mipMaps.reserve(adjustedMipLevels);

			for (size_t j = 0; j < adjustedMipLevels; j++)
			{
				const auto mipMapOffset = reader.ReadInt32();
				if (mipMapOffset <= 0)
					return StreamResult::BadPointer;

				auto streamResult = StreamResult::Success;
				reader.ReadAtOffset(mipMapOffset, [&](IO::Reader& reader)
				{
					const auto mipSignature = static_cast<TxpSig>(reader.ReadUInt32());
					if (mipSignature != TxpSig::MipMap)
					{
						streamResult = StreamResult::BadFormat;
						return;
					}

					auto& mipMap = mipMaps.emplace_back();
					mipMap.Size.x = reader.ReadUInt32();
					mipMap.Size.y = reader.ReadUInt32();
					mipMap.Format = static_cast<TextureFormat>(reader.ReadUInt32());

					const auto mipIndex = reader.ReadUInt8();
					const auto arrayIndex = reader.ReadUInt8();
					const auto padding = reader.ReadUInt16();

					mipMap.DataSize = reader.ReadUInt32();
					if (mipMap.DataSize > reader.GetRemaining())
					{
						streamResult = StreamResult::BadCount;
						return;
					}

					mipMap.Data = std::make_unique<u8[]>(mipMap.DataSize);
					reader.Read(mipMap.Data.get(), mipMap.DataSize);
				}, true);
				if (streamResult != StreamResult::Success)
					return streamResult;
			}
		}

		reader.PopBaseOffset();
		return StreamResult::Success;
	}

	StreamResult TexSet::Read(IO::Reader& reader)
	{
		reader.PushBaseOffset();

		const auto setSignature = static_cast<TxpSig>(reader.ReadUInt32());
		const auto textureCount = reader.ReadUInt32();
		const auto packedInfo = reader.ReadUInt32();

		if (setSignature != TxpSig::TexSet)
			return StreamResult::BadFormat;

		Textures.reserve(textureCount);
		for (size_t i = 0; i < textureCount; i++)
		{
			const auto textureOffset = reader.ReadInt32();
			if (textureOffset <= 0)
				return StreamResult::BadPointer;

			auto streamResult = StreamResult::Success;
			reader.ReadAtOffset(textureOffset, [&](IO::Reader& reader)
			{
				streamResult = Textures.emplace_back(std::make_shared<Tex>())->Read(reader);
			}, true);

			if (streamResult != StreamResult::Success)
				return streamResult;
		}

		reader.PopBaseOffset();
		return StreamResult::Success;
	}

	StreamResult TexSet::Write(IO::Writer& writer)
	{
		const size_t texSetOffset = writer.GetPosition();
		const u32 textureCount = static_cast<u32>(Textures.size());
		constexpr u32 packedMask = 0x01010100;

		writer.WriteUInt32(static_cast<u32>(TxpSig::TexSet));
		writer.WriteUInt32(textureCount);
		writer.WriteUInt32(textureCount | packedMask);

		for (const auto& texture : Textures)
		{
			writer.ScheduleWriteOffset([&](IO::Writer& writer)
			{
				const size_t texOffset = writer.GetPosition();
				const u8 arraySize = static_cast<u8>(texture->MipMapsArray.size());
				const u8 mipLevels = (arraySize > 0) ? static_cast<u8>(texture->MipMapsArray.front().size()) : 0;

				writer.WriteUInt32(static_cast<u32>(texture->GetSignature()));
				writer.WriteUInt32(arraySize * mipLevels);
				writer.WriteUInt8(mipLevels);
				writer.WriteUInt8(arraySize);
				writer.WriteUInt8(0x01);
				writer.WriteUInt8(0x01);

				for (u8 arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
				{
					for (u8 mipIndex = 0; mipIndex < mipLevels; mipIndex++)
					{
						writer.ScheduleWriteOffset([arrayIndex, mipIndex, &texture](IO::Writer& writer)
						{
							const auto& mipMap = texture->MipMapsArray[arrayIndex][mipIndex];
							writer.WriteUInt32(static_cast<u32>(TxpSig::MipMap));
							writer.WriteInt32(mipMap.Size.x);
							writer.WriteInt32(mipMap.Size.y);
							writer.WriteUInt32(static_cast<u32>(mipMap.Format));
							writer.WriteUInt8(mipIndex);
							writer.WriteUInt8(arrayIndex);
							writer.WriteUInt8(0x00);
							writer.WriteUInt8(0x00);
							writer.WriteUInt32(mipMap.DataSize);
							writer.Write(mipMap.Data.get(), mipMap.DataSize);
						}, texOffset);
					}
				}
			}, texSetOffset);
		}
		
		writer.FlushScheduledWrites();
		writer.Pad(0x10);

		return StreamResult::Success;
	}

	StreamResult SprSet::Read(IO::Reader& reader)
	{
		Flags = reader.ReadUInt32();
		const auto texSetOffset = reader.ReadInt32();
		const auto textureCount = reader.ReadUInt32();
		const auto spriteCount = reader.ReadUInt32();
		const auto spritesOffset = reader.ReadInt32();
		const auto textureNamesOffset = reader.ReadInt32();
		const auto spriteNamesOffset = reader.ReadInt32();
		const auto spriteExtraDataOffset = reader.ReadInt32();

		if (textureCount > 0)
		{
			auto streamResult = StreamResult::Success;
			if (texSetOffset <= 0)
				return StreamResult::BadPointer;

			reader.ReadAtOffset(static_cast<size_t>(texSetOffset), [&](IO::Reader& reader)
			{
				this->TexSet.Read(reader);
			});

			if (streamResult != StreamResult::Success)
				return streamResult;

			if (textureNamesOffset > 0 && TexSet.Textures.size() == textureCount)
			{
				reader.ReadAtOffset(textureNamesOffset, [&](IO::Reader& reader)
				{
					for (auto& texture : this->TexSet.Textures)
						texture->Name = reader.ReadStringOffset();
				});
			}
		}

		if (spriteCount > 0)
		{
			if (spritesOffset <= 0 || spriteExtraDataOffset <= 0)
				return StreamResult::BadPointer;

			auto streamResult = StreamResult::Success;
			reader.ReadAtOffset(spritesOffset, [&](IO::Reader& reader)
			{
				Sprites.reserve(spriteCount);
				for (size_t i = 0; i < spriteCount; i++)
				{
					auto& sprite = Sprites.emplace_back();
					sprite.TextureIndex = reader.ReadUInt32();
					sprite.Rotate = reader.ReadUInt32();
					reader.Read(&sprite.TexelRegion, sizeof(vec4));
					reader.Read(&sprite.PixelRegion, sizeof(vec4));
				}
			});
			if (streamResult != StreamResult::Success)
				return streamResult;

			if (spriteNamesOffset > 0 && Sprites.size() == spriteCount)
			{
				reader.ReadAtOffset(spriteNamesOffset, [&](IO::Reader& reader)
				{
					for (auto& sprite : Sprites)
						sprite.Name = reader.ReadStringOffset();
				});
			}

			reader.ReadAtOffset(spriteExtraDataOffset, [&](IO::Reader& reader)
			{
				for (auto& sprite : Sprites)
				{
					sprite.Extra.Flags = reader.ReadUInt32();
					sprite.Extra.ScreenMode = static_cast<ScreenMode>(reader.ReadUInt32());
				}
			});
		}

		return StreamResult::Success;
	}

	StreamResult SprSet::Write(IO::Writer& writer)
	{
		writer.WriteUInt32(Flags);

		const auto texSetPtrAddress = writer.GetPosition();
		writer.WriteUInt32(0x00000000);
		writer.WriteUInt32(static_cast<u32>(TexSet.Textures.size()));

		writer.WriteUInt32(static_cast<u32>(Sprites.size()));
		writer.ScheduleWriteOffset([this](IO::Writer& writer)
		{
			for (const auto& sprite : Sprites)
			{
				writer.WriteInt32(sprite.TextureIndex);
				writer.WriteInt32(sprite.Rotate);
				writer.WriteFloat32(sprite.TexelRegion.x);
				writer.WriteFloat32(sprite.TexelRegion.y);
				writer.WriteFloat32(sprite.TexelRegion.z);
				writer.WriteFloat32(sprite.TexelRegion.w);
				writer.WriteFloat32(sprite.PixelRegion.x);
				writer.WriteFloat32(sprite.PixelRegion.y);
				writer.WriteFloat32(sprite.PixelRegion.z);
				writer.WriteFloat32(sprite.PixelRegion.w);
			}
		});

		
		writer.ScheduleWriteOffset([this](IO::Writer& writer)
		{
			for (const auto& texture : TexSet.Textures)
			{
				if (texture->Name.size() > 0)
					writer.ScheduleWriteStringOffset(texture->Name);
				else
					writer.WriteInt32(0);
			}
		});
		

		writer.ScheduleWriteOffset([this](IO::Writer& writer)
		{
			for (const auto& sprite : Sprites)
				writer.ScheduleWriteStringOffset(sprite.Name);
		});

		writer.ScheduleWriteOffset([this](IO::Writer& writer)
		{
			for (const auto& sprite : Sprites)
			{
				writer.WriteUInt32(sprite.Extra.Flags);
				writer.WriteUInt32(static_cast<u32>(sprite.Extra.ScreenMode));
			}
		});
		
		writer.FlushScheduledWrites();
		writer.Pad(0x10);
		writer.FlushScheduledStrings();
		writer.Pad(0x10);

		const auto texSetPtr = writer.GetPosition();
		TexSet.Write(writer);

		writer.Seek(texSetPtrAddress);
		writer.WriteInt32(static_cast<int32_t>(texSetPtr));

		return StreamResult::Success;
	}
}
