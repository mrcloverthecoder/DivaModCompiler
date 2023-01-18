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

#include "core_string.h"
#include <charconv>
#include <Windows.h>

static std::string Win32NarrowStdStringWithCodePage(std::wstring_view input, UINT win32CodePage)
{
	std::string output;
	const int outputLength = ::WideCharToMultiByte(win32CodePage, 0, input.data(), static_cast<int>(input.size() + 1), nullptr, 0, nullptr, nullptr) - 1;

	if (outputLength > 0)
	{
		output.resize(outputLength);
		::WideCharToMultiByte(win32CodePage, 0, input.data(), static_cast<int>(input.size()), output.data(), outputLength, nullptr, nullptr);
	}

	return output;
}

static std::wstring Win32WidenStdStringWithCodePage(std::string_view input, UINT win32CodePage)
{
	std::wstring utf16Output;
	const int utf16Length = ::MultiByteToWideChar(win32CodePage, 0, input.data(), static_cast<int>(input.size() + 1), nullptr, 0) - 1;

	if (utf16Length > 0)
	{
		utf16Output.resize(utf16Length);
		::MultiByteToWideChar(win32CodePage, 0, input.data(), static_cast<int>(input.size()), utf16Output.data(), utf16Length);
	}

	return utf16Output;
}

namespace UTF8
{
	std::string Narrow(std::wstring_view utf16Input)
	{
		return Win32NarrowStdStringWithCodePage(utf16Input, CP_UTF8);
	}

	std::wstring Widen(std::string_view utf8Input)
	{
		return Win32WidenStdStringWithCodePage(utf8Input, CP_UTF8);
	}

	WideArg::WideArg(std::string_view utf8Input)
	{
		// NOTE: Length **without** null terminator
		convertedLength = ::MultiByteToWideChar(CP_UTF8, 0, utf8Input.data(), static_cast<int>(utf8Input.size() + 1), nullptr, 0) - 1;
		if (convertedLength <= 0)
		{
			stackBuffer[0] = L'\0';
			return;
		}

		if (convertedLength < ArrayCount(stackBuffer))
		{
			::MultiByteToWideChar(CP_UTF8, 0, utf8Input.data(), static_cast<int>(utf8Input.size()), stackBuffer, convertedLength);
			stackBuffer[convertedLength] = L'\0';
		}
		else
		{
			// heapBuffer = std::make_unique<wchar_t[]>(convertedLength + 1);
			heapBuffer = std::unique_ptr<wchar_t[]>(new wchar_t[convertedLength + 1]);
			::MultiByteToWideChar(CP_UTF8, 0, utf8Input.data(), static_cast<int>(utf8Input.size()), heapBuffer.get(), convertedLength);
			heapBuffer[convertedLength] = L'\0';
		}
	}

	const wchar_t* WideArg::c_str() const
	{
		return (convertedLength < ArrayCount(stackBuffer)) ? stackBuffer : heapBuffer.get();
	}
}

namespace ASCII
{
	template <typename T>
	static inline b8 TryParsePrimitiveTypeT(std::string_view string, T& out)
	{
		const std::from_chars_result result = std::from_chars(string.data(), string.data() + string.size(), out);
		const b8 hasNoError = (result.ec == std::errc {});
		const b8 parsedFully = (result.ptr == string.data() + string.size());

		return hasNoError && parsedFully;
	}

	b8 TryParseU32(std::string_view string, u32& out) { return TryParsePrimitiveTypeT(string, out); }
	b8 TryParseI32(std::string_view string, i32& out) { return TryParsePrimitiveTypeT(string, out); }
	b8 TryParseU64(std::string_view string, u64& out) { return TryParsePrimitiveTypeT(string, out); }
	b8 TryParseI64(std::string_view string, i64& out) { return TryParsePrimitiveTypeT(string, out); }
	b8 TryParseF32(std::string_view string, f32& out) { return TryParsePrimitiveTypeT(string, out); }
	b8 TryParseF64(std::string_view string, f64& out) { return TryParsePrimitiveTypeT(string, out); }
}
