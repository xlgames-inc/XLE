// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "DequantAnalysisTools.h"
#include "../../OSServices/Log.h"
#include "../../Utility/IteratorUtils.h"
#include <iomanip>
#include <utility>

#pragma warning(disable:4505) // unreferenced local function has been removed

namespace RenderCore { namespace Assets { namespace GeoProc
{

	namespace
	{
		std::ostream& operator<<(std::ostream& str, IteratorRange<const uint8_t*> bytes)
		{
			str << std::hex;
			for (auto b:bytes)
				str << (unsigned)b << " ";
			str << std::dec;
			return str;
		}
	}

	void PrintBytes(std::ostream& str, IteratorRange<const uint8_t*> bytes)
	{
		str << bytes;
	}

	void PrintChart(
		std::ostream& str,
		IteratorRange<const unsigned*> keyframes,
		IteratorRange<const float*> values)
	{
		float minV = FLT_MAX, maxV = -FLT_MAX;
		for (auto f:values) {
			minV = std::min(minV, f);
			maxV = std::max(maxV, f);
		}
		if (maxV <= minV) {
			str << "  No variation: " << values[0] << std::endl;
			return;
		}
		const unsigned chartHeight = 10;
		unsigned keyframeMax = *(keyframes.end()-1);
		for (signed y=chartHeight-1; y>=0; --y) {
			std::string buffer;
			buffer.resize(keyframeMax, ' ');
			auto v = values.begin();
			for (auto f=keyframes.begin(); (f+1)<keyframes.end(); ++f, ++v) {
				unsigned startFrame = *f, nextFrame = *(f+1);
				signed height = (signed)std::floor((*v - minV) / (maxV - minV) * chartHeight + 0.5f);
				if (y <= height) {
					for (auto q=startFrame; q<nextFrame; ++q) buffer[q] = '*';
				}
			}
			str << "  ";
			if (y == (chartHeight-1)) {
				str << std::setprecision(2) << std::fixed << std::setw(8) << maxV << " | ";
			} else if (y == 0) {
				str << std::setprecision(2) << std::fixed << std::setw(8) << minV << " | ";
			} else {
				str << "         | ";
			}
			str << buffer << std::endl;
		}
	}

	void PrintChart(
		std::ostream& str,
		IteratorRange<const unsigned*> keyframes,
		IteratorRange<const signed*> values,
		unsigned bitCount)
	{
		signed minV = INT_MAX, maxV = INT_MIN;
		for (auto f:values) {
			minV = std::min(minV, f);
			maxV = std::max(maxV, f);
		}
		if (maxV <= minV) {
			str << "  No variation: " << values[0] << std::endl;
			return;
		}
		const unsigned chartHeight = 10;
		unsigned keyframeMax = *(keyframes.end()-1);
		for (signed y=chartHeight-1; y>=0; --y) {
			std::string buffer;
			buffer.resize(keyframeMax, ' ');
			auto v = values.begin();
			for (auto f=keyframes.begin(); (f+1)<keyframes.end(); ++f, ++v) {
				unsigned startFrame = *f, nextFrame = *(f+1);
				signed height = (signed)std::floor((*v - minV) / float(maxV - minV) * chartHeight + 0.5f);
				if (y <= height) {
					for (auto q=startFrame; q<nextFrame; ++q) buffer[q] = '*';
				}
			}
			str << "  ";
			if (y == (chartHeight-1)) {
				str << std::setprecision(2) << std::fixed << std::setw(8) << maxV << " | ";
			} else if (y == 0) {
				str << std::setprecision(2) << std::fixed << std::setw(8) << minV << " | ";
			} else {
				str << "         | ";
			}
			str << buffer << std::endl;
		}

		if (bitCount != 0) {
			for (signed y=0; y<(signed)bitCount; ++y) {
				std::string buffer;
				buffer.resize(keyframeMax, ' ');

				auto v = values.begin();
				for (auto f=keyframes.begin(); f<keyframes.end(); ++f, ++v) {
					buffer[*f] = ((*v) & (1<<y)) ? '1' : '0';
				}

				str << "  ";
				if (y == 0) {
					str << "LSB    " << y << " | ";
				} else if (y == 0) {
					str << "       " << y << " | ";
				} else {
					str << "         | ";
				}
				str << buffer << std::endl;
			}
		}
	}

	float DequantUnsigned(unsigned v, unsigned bitWidth, float variation, float lowest)
	{
		// Take a quantized value, and return a value between variation and variation+lowest
		// based on the unsigned interpretation of that quantized value
		return lowest + variation * v / float((1<<(bitWidth))-1);
	}

	static uint8_t ReverseBits8(uint8_t b) 
	{
		b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
		b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
		b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
		return b;
	}

	static signed SignExtend(unsigned input, unsigned inputBitWidth)
	{
		int const m = 1U << (inputBitWidth - 1); // mask can be pre-computed if b is fixed
		auto x = input & ((1U << inputBitWidth) - 1);  // (Skip this if bits in x above position b are already zero.)
		return (x ^ m) - m;
	}

	signed ExtractBits(const void* p, unsigned bStart, unsigned bEnd, bool doSignExtension)
	{
		unsigned remainingPrefix = bStart;
		unsigned remainingCount = bEnd-bStart;
		assert(remainingCount <= 32);
		auto i = (const uint8_t*)p;
		while (remainingPrefix >= 8) {
			remainingPrefix -= 8;
			i++;
		}
		unsigned result = 0;
		unsigned shiftIntoResult = 0;
		while (remainingCount) {
			auto raw = (*i) >> remainingPrefix;
			raw &= (1<<(remainingCount)) - 1;
			result |= raw << shiftIntoResult;

			auto bitsGrabbed = std::min(remainingCount, 8u) - remainingPrefix;
			remainingCount -= bitsGrabbed;
			remainingPrefix = 0;
			shiftIntoResult += bitsGrabbed;
			i++;
		}
		if (doSignExtension) {
			return SignExtend(result, bEnd-bStart);
		} else
			return result;
	}

	void DrawDequantAnalysis(
		std::ostream& str,
		IteratorRange<const unsigned*> keyframes, IteratorRange<const void*> range, IteratorRange<const Dequant*> optionsToTest)
	{
		// Look at different wants to dequantize the given input, and try to find the way that looks the most sensible
		for (auto&d:optionsToTest) {
			std::vector<signed> as, bs, cs, ds;
			for (const void*p=range.begin(); PtrAdd(p, d._stride/8)<=range.end(); p=PtrAdd(p, d._stride/8)) {
				unsigned bitIterator = 0;
				as.push_back(ExtractBits(p, bitIterator, bitIterator+d._bitCountA, d._doSignExtension)); bitIterator += d._bitCountA;
				bs.push_back(ExtractBits(p, bitIterator, bitIterator+d._bitCountB, d._doSignExtension)); bitIterator += d._bitCountB;
				cs.push_back(ExtractBits(p, bitIterator, bitIterator+d._bitCountC, d._doSignExtension)); bitIterator += d._bitCountC;
				ds.push_back(ExtractBits(p, bitIterator, bitIterator+d._bitCountD, d._doSignExtension)); bitIterator += d._bitCountD;
				assert(bitIterator<=d._stride);
			}

			str << "  ---------------- [" << d._name << " - As ] ----------------" << std::endl;
			PrintChart(str, keyframes, MakeIteratorRange(as), d._bitCountA);
			str << "  ---------------- [" << d._name << " - Bs ] ----------------" << std::endl;
			PrintChart(str, keyframes, MakeIteratorRange(bs), d._bitCountB);
			str << "  ---------------- [" << d._name << " - Cs ] ----------------" << std::endl;
			PrintChart(str, keyframes, MakeIteratorRange(cs), d._bitCountC);
			str << "  ---------------- [" << d._name << " - Ds ] ----------------" << std::endl;
			PrintChart(str, keyframes, MakeIteratorRange(ds), d._bitCountD);
		}
	}

	

}}}

