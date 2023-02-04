#pragma once

#include <memory>
#include <core_io.h>
#include "core_types.h"

namespace Comfy
{
	enum class StreamResult : u8
	{
		Success,
		BadFormat,
		BadCount,
		BadPointer,
		InsufficientSpace,
		UnknownError,
		Count
	};

	class IStreamReadable
	{
	public:
		virtual StreamResult Read(IO::Reader& reader) = 0;
	};

	class IStreamWritable
	{
	public:
		virtual StreamResult Write(IO::Writer& writer) = 0;
	};

	template <typename Readable>
	std::unique_ptr<Readable> LoadFile(std::string_view path)
	{
		static_assert(std::is_base_of_v<IStreamReadable, Readable>);

		auto data = std::make_unique<Readable>();
		IO::Reader reader = { };
		reader.FromFile(path);
		data->Read(reader);
		return data;
	}
}
