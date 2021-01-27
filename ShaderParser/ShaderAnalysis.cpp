// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "ShaderAnalysis.h"
#include "../Assets/IFileSystem.h"
#include "../Assets/IntermediatesStore.h"		// (for GetDependentFileState)
#include "../OSServices/Log.h"
#include "../Utility/Streams/StreamFormatter.h"
#include "../Utility/Streams/PreprocessorInterpreter.h"
#include "../Utility/StringFormat.h"
#include <tiny-process-library/process.hpp>

#if COMPILER_ACTIVE == COMPILER_TYPE_MSVC
	// Awkward work-around for a strange problem inside of nlohmann/json.hpp
	// it's use the "deprecated" attribute on an inline friend function definition
	// (ie, it's defining a free function, but doing it inside of a class by using
	// the friend keyword). MSVC doesn't seem to like the depreciated attribute
	// begin used like this. We can adjust the __cplusplus keyword to try to trick
	// the library into not doing it. Since MSVC doesn't set the correct value on 
	// __cplusplus by default, this may be why it's not a more well known issue?
	#include <nlohmann/thirdparty/hedley/hedley.hpp>
	#undef JSON_HEDLEY_DEPRECATED
	#define JSON_HEDLEY_DEPRECATED(X)
	#include <nlohmann/json.hpp>
#else
	#include <nlohmann/json.hpp>
#endif

#include <sstream>

namespace ShaderSourceParser
{

	template<typename CharType> 
        static bool WhitespaceChar(CharType c)  // (excluding new line)
    {
        return c==' ' || c=='\t' || c==0x0B || c==0x0C || c==0x85 || c==0xA0 || c==0x0;
    }

	static StringSection<> ParseIncludeDirective(StringSection<> src)
	{
		auto i = src.begin();
		while (i != src.end() && WhitespaceChar(*i)) ++i;
		if (i != src.end() && (*i == '"' || *i == '<')) {
			char terminator = (*i == '"') ? '"' : '>';
			++i;
			auto resultStart = i;
			while (i != src.end() && *i != terminator) ++i;
			if (i == src.end()) return {};		// no terminator

			return MakeStringSection(resultStart, i);
		}

		return {};		// didn't understand
	}

	struct LineDetails
	{
		enum class LineType 
		{
			Normal,
			EndsInBlockComment, 
			EndsInTrailingLineComment
		};

		StringSection<> _line;
		LineType _type;

		const char* _nextLineBegin = nullptr;
		const char* _firstNonCommentNonWhitespaceChar = nullptr;
	};

	static LineDetails ParseLineDetails(StringSection<> src, LineDetails::LineType prevLineType)
	{
		// Note -- there's a bit of overlap between this behaviour and what we do in the "ConditionalProcessingTokenizer" class
		// in PredefinedCBLayout.cpp. It would be nice if we could combine the parsing together somehow
		bool inBlockComment = prevLineType == LineDetails::LineType::EndsInBlockComment;
		bool inLineComment = prevLineType == LineDetails::LineType::EndsInTrailingLineComment;
		auto i = src.begin();
		auto firstNonCommentNonWhitespaceChar = src.end();
		for (;;) {
			if (i == src.end() || *i == '\n' || *i == '\r') {
				// Hit the end of the line; return our result
				// Note that even when we're inside of a block comment we will return here
				LineDetails result;
				result._line = MakeStringSection(src.begin(), i);
				if (inBlockComment) {
					result._type = LineDetails::LineType::EndsInBlockComment;
				} else if (inLineComment) {
					if ((i - 1) >= src.begin() && *(i-1) == '\\') {
						result._type = LineDetails::LineType::EndsInTrailingLineComment;
					} else
						result._type = LineDetails::LineType::Normal;	// line comment ends here, so we don't need to care about it for future lines
				} else {
					result._type = LineDetails::LineType::Normal;
				}
				result._firstNonCommentNonWhitespaceChar = std::min(i, firstNonCommentNonWhitespaceChar);

				// i currently points to the end of this line; we should
				// advance to the start of the next line
				if (i!=src.end()) {
					if (*i == '\n') {
						++i;
					} else if (*i == '\r') {
						++i;
						if (i < src.end() && *i == '\n')
							++i;
					}
				}
				result._nextLineBegin = i;
				return result;
			}

			if (inBlockComment) {
				if (*i == '*') {
					if (i+1 < src.end() && *(i+1) == '/') {
						inBlockComment = false;
						i+=2;
						continue;
					}
				}
			} else if (!inLineComment) {
				// We cannot start a block comment inside of a line comment, so only
				// do this check when where not in a line comment
				if (*i == '/') {
					if (i+1 < src.end() && *(i+1) == '*') {
						inBlockComment = true;
						i+=2;
						continue;
					} else if (i+1 < src.end() && *(i+1) == '/') {
						inLineComment = true;
						i+=2;
						continue;
					}
				}

				// otherwise, this is real text, let's record it
				if (!WhitespaceChar(*i) && firstNonCommentNonWhitespaceChar == src.end()) {
					firstNonCommentNonWhitespaceChar = i;
				}
			}

			++i;
		}
	}

	static bool HasNonWhiteSpace(StringSection<> input)
	{
		for (auto i:input) if (!WhitespaceChar(i) && i != '\n' && i != '\r') return true;
		return false;
	}

	RenderCore::Assets::ISourceCodePreprocessor::SourceCodeWithRemapping ExpandIncludes(
		StringSection<> src,
		const std::string& srcName,
		const ::Assets::DirectorySearchRules& searchRules)
	{
		// Find the #include statements in the input and expand them out
		// Note; some limitations:
		//		1) not handling #pragma once -- every include gets included every time
		//		2) expanding out includes even when they are within a failed "#if" preprocessor statement
		//		3) some combinations of comments and preprocessor statements will be handled incorrectly. Comments are mostly
		//			handled, but there are some less common cases (such as a block/line comment immediately after the "#include" that
		//			can throw it off

		unsigned sourceLineIndex = 0;		// (note zero based index for lines -- ie, line at the top of the file is line number 0)

		unsigned workingBlockStartLineIndex = 0;
		unsigned workingBlockLineCount = 0;
		StringSection<> workingBlock;

		std::stringstream outputStream;
		unsigned outputStreamLineCount = 0;
		RenderCore::Assets::ISourceCodePreprocessor::SourceCodeWithRemapping result;

		LineDetails::LineType prevLineType = LineDetails::LineType::Normal;
		auto i = src.begin();
		while (i != src.end()) {
			auto lineDetails = ParseLineDetails(MakeStringSection(i, src.end()), prevLineType);

			// Check if the first non-whitespace, non-commented character on the line
			// is a hash -- if so, there's a preprocessor directive to consider
			bool appendLineToWorkingBlock = true;
			{
				auto i2 = lineDetails._firstNonCommentNonWhitespaceChar;
				if (i2 != lineDetails._line.end() && *i2 == '#') {
					++i2;
					// Note -- comments not supporting in these upcoming scans
					while (i2 != lineDetails._line.end() && WhitespaceChar(*i2)) ++i2;
					auto i3 = i2;
					while (i3 != lineDetails._line.end() && !WhitespaceChar(*i3) && *i3 != '"' && *i3 != '<') ++i3;

					// It is a valid preprocessor statement; check if it's an include
					if (XlEqStringI(MakeStringSection(i2, i3), "include")) {
						auto includeFile = ParseIncludeDirective(MakeStringSection(i3, lineDetails._line.end()));
						if (includeFile.IsEmpty())
							Throw(::Utility::FormatException("Could not interpret #include directive", StreamLocation{unsigned(i3-lineDetails._line.begin()), sourceLineIndex}));

						// Commit the working block to our output stream
						// We must also include the text that becomes before the #include on this line -- because it
						// could close a block comment that began on a previous line
						bool requiresPrefix = HasNonWhiteSpace(MakeStringSection(lineDetails._line._start, lineDetails._firstNonCommentNonWhitespaceChar));
						if (!workingBlock.IsEmpty() || requiresPrefix) {
							result._lineMarkers.push_back(
								RenderCore::ILowLevelCompiler::SourceLineMarker {
									srcName,
									(!workingBlock.IsEmpty()) ? workingBlockStartLineIndex : sourceLineIndex,		// (second case is for when there is only the prefix, and no working block
									outputStreamLineCount
								});

							outputStream << workingBlock;
							outputStreamLineCount += workingBlockLineCount;

							if (requiresPrefix) {
								if (!workingBlock.IsEmpty()) {
									outputStream << MakeStringSection(workingBlock.end(), lineDetails._firstNonCommentNonWhitespaceChar) << std::endl;
								} else {
									outputStream << MakeStringSection(lineDetails._line.begin(), lineDetails._firstNonCommentNonWhitespaceChar) << std::endl;
								}
								++outputStreamLineCount;
							}
						}

						workingBlockLineCount = 0;
						workingBlock = {};
						appendLineToWorkingBlock = false;		// (don't append this line, because we've just used it as an include */

						// We must expand out this include...
						char resolvedFN[MaxPath];
						searchRules.ResolveFile(resolvedFN, includeFile);
						auto subFile = ::Assets::MainFileSystem::OpenFileInterface(resolvedFN, "rb");
						subFile->Seek(0, OSServices::FileSeekAnchor::End);
						auto subFileLength = subFile->TellP();
						subFile->Seek(0);

						std::string buffer;
						buffer.resize(subFileLength);
						subFile->Read(AsPointer(buffer.begin()), subFileLength);

						auto expanded = ExpandIncludes(
							MakeStringSection(buffer),
							resolvedFN,
							::Assets::DefaultDirectorySearchRules(resolvedFN));

						if (!expanded._processedSource.empty()) {
							for (const auto&l:expanded._lineMarkers)
								result._lineMarkers.push_back(
									RenderCore::ILowLevelCompiler::SourceLineMarker {
										l._sourceName,
										l._sourceLine,
										outputStreamLineCount + l._processedSourceLine
									});

							outputStream << expanded._processedSource;
							outputStreamLineCount += expanded._processedSourceLineCount;

							// Some expanded files might not end in a new line; so append one if needed
							if (!expanded._processedSource.empty() && *(expanded._processedSource.end()-1) != '\n' && *(expanded._processedSource.end()-1) != '\r') {
								outputStream << std::endl;
								outputStreamLineCount ++;
							}
						}

						// todo -- search for duplicates in the dependencies
						result._dependencies.insert(
							result._dependencies.end(),
							expanded._dependencies.begin(), expanded._dependencies.end());
						result._dependencies.push_back(::Assets::IntermediatesStore::GetDependentFileState(resolvedFN));
					}
				}
			}

			// either append to, or construct the working block -- 
			if (appendLineToWorkingBlock) {
				if (!workingBlock.IsEmpty()) {
					workingBlock._end = lineDetails._nextLineBegin;
				} else {
					workingBlockStartLineIndex = sourceLineIndex;
					workingBlock = MakeStringSection(lineDetails._line.begin(), lineDetails._nextLineBegin);
				}
				// This condition is to avoid an issue counting lines if there is no newline at the end of the
				// file. The line count is actually the number of newlines in the text -- so if the final
				// line doesn't end in a new line, we don't count it. In the extreme case, for a file with
				// no new lines at all, we actually return a line count of 0
				if (lineDetails._line.end() != lineDetails._nextLineBegin)
					++workingBlockLineCount;
			}
			
			++sourceLineIndex;
			prevLineType = lineDetails._type;
			i = lineDetails._nextLineBegin;
		}

		// Append the last fragment from the working block -- 
		if (!workingBlock.IsEmpty()) {
			result._lineMarkers.push_back(
				RenderCore::ILowLevelCompiler::SourceLineMarker {
					srcName,
					workingBlockStartLineIndex,
					outputStreamLineCount
				});
			outputStream << workingBlock;
			outputStreamLineCount += workingBlockLineCount;
		}

		result._processedSource = outputStream.str();
		result._processedSourceLineCount = outputStreamLineCount;

		return result;
	}


	ShaderSelectorAnalysis AnalyzeSelectors(const std::string& sourceCode)
	{
		std::stringstream str;
		// Send the given text to the shader analyzer, and interpret the
		// results
		// HACK -- shader analyzer not public yet
		TinyProcessLib::Process analyzerProcess(
			"node build/analyze.js", "c:/code/ShaderPreprocessorAnalyzer/",
			[&str](const char *bytes, size_t n) {
				str << MakeStringSection(bytes, bytes+n);
			},
			nullptr, true);
		analyzerProcess.write(sourceCode);
		analyzerProcess.close_stdin();
		int exitStatus = analyzerProcess.get_exit_status();

		if (exitStatus == 0) {
			auto json = nlohmann::json::parse(str.str());
			ShaderSelectorAnalysis result;
			for (auto i=json.begin(); i!=json.end(); ++i) {
				result._selectorRelevance.insert(std::make_pair(i.key(), i.value().get<std::string>()));
			}
			return result;
		} else {
			return {};
		}
	}

	ParameterBox FilterSelectors(
		const ParameterBox& unfiltered,
		const std::unordered_map<std::string, std::string>& relevance)
	{
		const ParameterBox* unfilteredAsArray[] = { &unfiltered };

		// Filter selectors based on the given relevance table
		ParameterBox result;
		for (const auto&p:unfiltered) {
			auto i = relevance.find(p.Name().AsString());
			if (i == relevance.end())
				continue;

			auto relevant = EvaluatePreprocessorExpression(MakeStringSection(i->second), MakeIteratorRange(unfilteredAsArray));
			if (!relevant)
				continue;

			result.SetParameter(p.Name(), p.RawValue(), p.Type());
		}
		return result;
	}

	static const std::string s_alwaysRelevant { "1" };

	namespace Utility
	{
		void MergeRelevance(
			std::unordered_map<std::string, std::string>& result,
			const std::unordered_map<std::string, std::string>& src)
		{
			for (const auto&s:src) {
				auto i = result.find(s.first);
				if (i != result.end()) {
					if (i->second == s_alwaysRelevant) {
						// already always relevant; just continue
					} else if (s.second == s_alwaysRelevant) {
						// becoming always relevance, no merging necessary
						result.insert(s);
					} else if (i->second == s.second) {
						// the conditions are just the same; just continue
					} else {
						std::string merged = std::string("(") + i->second + ") || (" + s.second + ")";
						result.insert(std::make_pair(s.first, merged));
					}
				} else {
					result.insert(s);
				}
			}
		}

		::Assets::DepValPtr MergeRelevanceFromShaderFiles(
			std::unordered_map<std::string, std::string>& result,
			const std::set<std::string>& shaderFileSet)
		{
			std::vector<::Assets::DependentFileState> dependencies;
			for (const auto& rawShader:shaderFileSet) {
				size_t fileSize = 0;
				auto blob = ::Assets::TryLoadFileAsMemoryBlock_TolerateSharingErrors(rawShader, &fileSize);
				auto expanded = ShaderSourceParser::ExpandIncludes(
					MakeStringSection((char*)blob.get(), (char*)PtrAdd(blob.get(), fileSize)),
					rawShader, 
					::Assets::DefaultDirectorySearchRules(rawShader));
				auto selectors = ShaderSourceParser::AnalyzeSelectors(expanded._processedSource);
				MergeRelevance(result, selectors._selectorRelevance);

				dependencies.insert(dependencies.end(), expanded._dependencies.begin(), expanded._dependencies.end());
			}

			return ::Assets::AsDepVal(MakeIteratorRange(dependencies));
		}
	}
}
