// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "PredefinedCBLayout.h"
#include "../RenderUtils.h"
#include "../ShaderLangUtil.h"
#include "../Format.h"
#include "../../Assets/ConfigFileContainer.h"
#include "../../Assets/IFileSystem.h"
#include "../../Assets/DepVal.h"
#include "../../ConsoleRig/Log.h"
#include "../../Utility/BitUtils.h"
#include "../../Utility/Streams/FileUtils.h"
#include "../../Utility/StringUtils.h"
#include "../../Utility/MemoryUtils.h"
#include "../../Utility/Conversion.h"
#include <regex>

namespace RenderCore { namespace Assets
{
    PredefinedCBLayout::PredefinedCBLayout(StringSection<::Assets::ResChar> initializer)
    {
		_cbSize = 0;
		_validationCallback = std::make_shared<::Assets::DependencyValidation>();
		::Assets::RegisterFileDependency(_validationCallback, initializer);

		TRY {
			// Here, we will read a simple configuration file that will define the layout
			// of a constant buffer. Sometimes we need to get the layout of a constant 
			// buffer without compiling any shader code, or really touching the HLSL at all.
			size_t size;
			auto file = ::Assets::TryLoadFileAsMemoryBlock(initializer, &size);
			StringSection<char> configSection((const char*)file.get(), (const char*)PtrAdd(file.get(), size));

			// if it's a compound document, we're only going to extra the cb layout part
			auto compoundDoc = ::Assets::ReadCompoundTextDocument(configSection);
			if (!compoundDoc.empty()) {
				auto i = std::find_if(
					compoundDoc.cbegin(), compoundDoc.cend(),
					[](const ::Assets::TextChunk<char>& chunk)
					{ return XlEqString(chunk._type, "CBLayout"); });
				if (i != compoundDoc.cend())
					configSection = i->_content;
			}

			Parse(configSection);
		} CATCH(const ::Assets::Exceptions::ConstructionError& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH (const std::exception& e) {
			Throw(::Assets::Exceptions::ConstructionError(e, _validationCallback));
		} CATCH_END
	}

    PredefinedCBLayout::PredefinedCBLayout(StringSection<char> source, bool)
    {
		_cbSize = 0;
        Parse(source);
        _validationCallback = std::make_shared<::Assets::DependencyValidation>();
    }

	PredefinedCBLayout::PredefinedCBLayout(IteratorRange<const NameAndType*> elements)
	{
		_cbSize = 0;
		_validationCallback = std::make_shared<::Assets::DependencyValidation>();
		AppendElements(elements);
	}

    void PredefinedCBLayout::Parse(StringSection<char> source)
    {
        std::regex parseStatement(R"--((\w*)\s+(\w*)\s*(?:\[(\d*)\])?\s*(?:=\s*([^;]*))?;.*)--");

		std::vector<NameAndType> nameAndTypes;

		const char* iterator = source.begin();
        const char* end = source.end();
        for (;;) {
            while (iterator < end && (*iterator == '\n' || *iterator == '\r' || *iterator == ' '|| *iterator == '\t')) ++iterator;
            const char* lineStart = iterator;
            if (lineStart >= end) break;

            while (iterator < end && *iterator != '\n' && *iterator != '\r') ++iterator;

                // double slash at the start of a line means ignore the reset of the line
            if (*lineStart == '/' && (lineStart+1) < iterator && *(lineStart+1) == '/')
                continue;

            std::match_results<const char*> match;
            bool a = std::regex_match(lineStart, iterator, match, parseStatement);
            if (a && match.size() >= 4) {
				auto name = match[2].str();
				auto type = ShaderLangTypeNameAsTypeDesc(MakeStringSection(match[1].str()));

				unsigned arrayElementCount = 1;
				if (match.size() > 3 && match[3].matched) {
					arrayElementCount = Conversion::Convert<unsigned>(match[3].str());
				}

				auto size = type.GetSize();
				if (!size) {
					Log(Warning) << "Problem parsing type in PredefinedCBLayout. Type size is 0: " << std::string(lineStart, iterator) << std::endl;
					continue;
				}

				nameAndTypes.push_back(PredefinedCBLayout::NameAndType { name, type, arrayElementCount });

                if (match.size() > 4 && match[4].matched) {

                    if (arrayElementCount > 1)
                        Log(Warning) << "Attempting to provide an default for an array type in PredefinedCBLayout (this isn't supported): " << std::string(lineStart, iterator) << std::endl;

                    uint8 buffer0[256], buffer1[256];
                    auto defaultType = ImpliedTyping::Parse(
                        MakeStringSection(match[4].first, match[4].second),
                        buffer0, dimof(buffer0));

                    if (!(defaultType == type)) {
                            //  The initialiser isn't exactly the same type as the
                            //  defined variable. Let's try a casting operation.
                            //  Sometimes we can get int defaults for floats variables, etc.
                        bool castSuccess = ImpliedTyping::Cast(
                            MakeIteratorRange(buffer1), type,
                            MakeIteratorRange(buffer0), defaultType);
                        if (castSuccess) {
							_defaults.SetParameter(MakeStringSection(name).Cast<utf8>(), {buffer1, PtrAdd(buffer1, std::min(sizeof(buffer1), (size_t)type.GetSize()))}, type);
                        } else {
                            Log(Warning) << "Default initialiser can't be cast to same type as variable in PredefinedCBLayout: " << std::string(lineStart, iterator) << std::endl;
                        }
                    } else {
                        _defaults.SetParameter(MakeStringSection(name).Cast<utf8>(), {buffer0, PtrAdd(buffer0, std::min(sizeof(buffer0), (size_t)defaultType.GetSize()))}, defaultType);
                    }
                }
            } else {
                Log(Warning) << "Failed to parse line in PredefinedCBLayout: " << std::string(lineStart, iterator) << std::endl;
            }
        }

		AppendElements(MakeIteratorRange(nameAndTypes));
    }

	void PredefinedCBLayout::AppendElements(IteratorRange<const NameAndType*> elements)
	{
		unsigned cbIterator = _cbSize;

		for (const auto& nameAndType : elements) {
			Element e;
			e._hash = ParameterBox::MakeParameterNameHash(nameAndType._name);
			e._hash64 = Hash64(AsPointer(nameAndType._name.begin()), AsPointer(nameAndType._name.end()));
			e._type = nameAndType._type;

			// HLSL adds padding so that vectors don't straddle 16 byte boundaries!
			// let's detect that case, and add padding as necessary
			if (FloorToMultiplePow2(cbIterator, 16) != FloorToMultiplePow2(cbIterator + std::min(16u, e._type.GetSize()) - 1, 16)) {
				cbIterator = CeilToMultiplePow2(cbIterator, 16);
			}

			auto size = e._type.GetSize();

			e._offset = cbIterator;
			e._arrayElementCount = nameAndType._arrayElementCount;
			e._arrayElementStride = (e._arrayElementCount > 1) ? CeilToMultiplePow2(size, 16) : size;
			if (e._arrayElementCount != 0)
				cbIterator += (e._arrayElementCount - 1) * e._arrayElementStride + size;
			_elements.push_back(e);
			_elementNames.push_back(nameAndType._name);
		}

		_cbSize = cbIterator;
		_cbSize = CeilToMultiplePow2(_cbSize, 16);
	}

	static PredefinedCBLayout::NameAndType* FindAlignmentGap(IteratorRange<PredefinedCBLayout::NameAndType*> elements, size_t requestSize)
	{
		unsigned cbIterator = 0;

		auto i = elements.begin();
		for(;i!=elements.end(); ++i) {
			auto newCBIterator = cbIterator;
			if (FloorToMultiplePow2(newCBIterator, 16) != FloorToMultiplePow2(newCBIterator + std::min(16u, i->_type.GetSize()) - 1, 16)) {
				newCBIterator = CeilToMultiplePow2(newCBIterator, 16);

				auto paddingSpace = newCBIterator - cbIterator;
				// If the paddingSpace equals or exceeds the space we're looking for, then let's use this space
				// We return the current iterator, which means the space can be found immediately before this element
				if (paddingSpace >= requestSize)
					return i;
			}

			auto eleSize = i->_type.GetSize();
			auto arrayElementStride = (i->_arrayElementCount > 1) ? CeilToMultiplePow2(eleSize, 16) : eleSize;
			if (i->_arrayElementCount != 0)
				cbIterator += (i->_arrayElementCount - 1) * arrayElementStride + eleSize;
		}

		return i;
	}

	void PredefinedCBLayout::OptimizeElementOrder(IteratorRange<NameAndType*> elements)
	{
		// Optimize ordering in 2 steps:
		//  1) order by type size (largest first) -- using a stable sort to maintain the original ordering as much as possible
		//	2) move any elements that can be squeezed into gaps in earlier parts of the ordering
		std::stable_sort(
			elements.begin(), elements.end(),
			[](const NameAndType& lhs, const NameAndType& rhs) {
				if (lhs._arrayElementCount > rhs._arrayElementCount) return true;
				if (lhs._arrayElementCount < rhs._arrayElementCount) return false;
				return lhs._type.GetSize() > rhs._type.GetSize();
			});

		for (auto i=elements.begin(); i!=elements.end(); ++i) {
			if (i->_arrayElementCount != 1 || i->_type.GetSize() >= 16) continue;

			// Find the best gap to squeeze this into
			// (note that since "elements" is ordered from largest to smallest, we will always
			// find and occupy the large alignment gaps first)
			auto insertionPoint = FindAlignmentGap(MakeIteratorRange(elements.begin(), i), i->_type.GetSize());
			if (insertionPoint == i) continue;	// no better location found

			// we can insert this object immediate before 'insertionPoint'. To do that, we should
			// move all elements from i-1 up to (and including) insertionPoint forward on element
			auto elementToInsert = *i;
			for (auto i2=i; (i2-1)>=insertionPoint; i2--)
				*i2 = *(i2-1);
			*insertionPoint = elementToInsert;
		}
	}

    void PredefinedCBLayout::WriteBuffer(void* dst, const ParameterBox& parameters) const
    {
        for (auto c=_elements.cbegin(); c!=_elements.cend(); ++c) {
            for (auto e=0u; e<c->_arrayElementCount; e++) {
                bool gotValue = parameters.GetParameter(
                    c->_hash + e, PtrAdd(dst, c->_offset + e * c->_arrayElementStride),
                    c->_type);

                if (!gotValue)
                    _defaults.GetParameter(c->_hash + e, PtrAdd(dst, c->_offset), c->_type);
            }
        }
    }

    std::vector<uint8> PredefinedCBLayout::BuildCBDataAsVector(const ParameterBox& parameters) const
    {
        std::vector<uint8> cbData(_cbSize, uint8(0));
        WriteBuffer(AsPointer(cbData.begin()), parameters);
        return std::move(cbData);
    }

    SharedPkt PredefinedCBLayout::BuildCBDataAsPkt(const ParameterBox& parameters) const
    {
        SharedPkt result = MakeSharedPktSize(_cbSize);
        std::memset(result.begin(), 0, _cbSize);
        WriteBuffer(result.begin(), parameters);
        return std::move(result);
    }
    
    uint64_t PredefinedCBLayout::CalculateHash() const
    {
        return HashCombine(Hash64(AsPointer(_elements.begin()), AsPointer(_elements.end())), _defaults.GetHash());
    }
    
    auto PredefinedCBLayout::MakeConstantBufferElements() const -> std::vector<ConstantBufferElementDesc>
    {
        std::vector<ConstantBufferElementDesc> result;
        result.reserve(_elements.size());
        for (auto i=_elements.begin(); i!=_elements.end(); ++i) {
            result.push_back(ConstantBufferElementDesc {
                i->_hash64, AsFormat(i->_type),
                i->_offset, i->_arrayElementCount });
        }
        return result;
    }

    PredefinedCBLayout::PredefinedCBLayout() 
	: _cbSize(0)
	{}    
    PredefinedCBLayout::~PredefinedCBLayout() {}
}}
