// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../../Utility/IteratorUtils.h"
#include <iosfwd>

namespace RenderCore { namespace Assets { namespace GeoProc
{
	//
	//	We sometimes need to analyze streams of bits and try to extract meaningful data
	//	out of them. Some mathematical types, such as Quaternions, can be stored in many
	//	different compressed forms. This can be interpreting bits a little tricky, so these
	//	are just a few simple utilities that can make that a little easier.
	//

	struct Dequant
	{
		unsigned _bitCountA, _bitCountB, _bitCountC, _bitCountD;
		unsigned _stride;
		bool _doSignExtension;
		const char* _name;
	};

	void DrawDequantAnalysis(
		std::ostream& str,
		IteratorRange<const unsigned*> keyframes, IteratorRange<const void*> range, IteratorRange<const Dequant*> optionsToTest);

	void PrintChart(
		std::ostream& str,
		IteratorRange<const unsigned*> keyframes,
		IteratorRange<const float*> values);
		
	void PrintChart(
		std::ostream& str,
		IteratorRange<const unsigned*> keyframes,
		IteratorRange<const signed*> values,
		unsigned bitCount = 0);

	signed ExtractBits(const void* p, unsigned bStart, unsigned bEnd, bool doSignExtension);
	float DequantUnsigned(unsigned v, unsigned bitWidth, float variation, float lowest);

	static Dequant s_3channel10Bit { 10, 10, 10, 2, 32, false, "10/10/10/2" };
	static Dequant s_3channel10BitSigned { 10, 10, 10, 2, 32, true,  "10/10/10/2 (signed)" };
	static Dequant s_3channel20Bit { 20, 20, 20, 4, 64, false, "20/20/20/4" };
	static Dequant s_4channel12Bit { 12, 12, 12, 12, 64, false, "12/12/12/12" };
	static Dequant s_3channel21Bit { 21, 21, 21, 1, 64, false, "21/21/21/1" };

	void PrintBytes(std::ostream& str, IteratorRange<const uint8_t*> bytes);

}}}

