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

#include "core_types.h"
#include <Windows.h>

static_assert(BitsPerByte == 8);
static_assert((sizeof(u8) * BitsPerByte) == 8 && (sizeof(i8) * BitsPerByte) == 8);
static_assert((sizeof(u16) * BitsPerByte) == 16 && (sizeof(i16) * BitsPerByte) == 16);
static_assert((sizeof(u32) * BitsPerByte) == 32 && (sizeof(i32) * BitsPerByte) == 32);
static_assert((sizeof(u64) * BitsPerByte) == 64 && (sizeof(i64) * BitsPerByte) == 64);
static_assert((sizeof(f32) * BitsPerByte) == 32 && (sizeof(f64) * BitsPerByte) == 64);
static_assert((sizeof(b8) * BitsPerByte) == 8);

static i64 Win32GetPerformanceCounterTicksPerSecond()
{
	::LARGE_INTEGER frequency = {};
	::QueryPerformanceFrequency(&frequency);
	return frequency.QuadPart;
}

static i64 Win32GetPerformanceCounterTicksNow()
{
	::LARGE_INTEGER timeNow = {};
	::QueryPerformanceCounter(&timeNow);
	return timeNow.QuadPart;
}

static struct Win32PerformanceCounterData
{
	i64 TicksPerSecond = Win32GetPerformanceCounterTicksPerSecond();
	i64 TicksOnProgramStartup = Win32GetPerformanceCounterTicksNow();
} Win32GlobalPerformanceCounter = {};

CPUTime CPUTime::GetNow()
{
	return CPUTime { Win32GetPerformanceCounterTicksNow() - Win32GlobalPerformanceCounter.TicksOnProgramStartup };
}

CPUTime CPUTime::GetNowAbsolute()
{
	return CPUTime { Win32GetPerformanceCounterTicksNow() };
}

Time CPUTime::DeltaTime(const CPUTime& startTime, const CPUTime& endTime)
{
	const i64 deltaTicks = (endTime.Ticks - startTime.Ticks);
	return Time::FromSeconds(static_cast<f64>(deltaTicks) / static_cast<f64>(Win32GlobalPerformanceCounter.TicksPerSecond));
}
