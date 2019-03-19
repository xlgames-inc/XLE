file(GLOB Src "*.cpp")
file(GLOB Headers "*.h")

set(GeneratedGrammarFiles
	Grammar/GraphSyntaxEval.c Grammar/GraphSyntaxEval.h
 	Grammar/GraphSyntaxLexer.c Grammar/GraphSyntaxLexer.h
	Grammar/GraphSyntaxParser.c Grammar/GraphSyntaxParser.h
	Grammar/ShaderLexer.c Grammar/ShaderLexer.h
	Grammar/ShaderParser.c Grammar/ShaderParser.h
	Grammar/ShaderTreeWalk.c Grammar/ShaderTreeWalk.h)

set(GrammarDir ${CMAKE_CURRENT_LIST_DIR}/Grammar)

add_custom_command(
	OUTPUT ${GeneratedGrammarFiles}
	COMMAND java -jar "${FOREIGN_DIR}/Antlr-3.4/antlr-3.4-complete.jar" -lib ${CMAKE_CURRENT_BINARY_DIR}/Grammar/ -o ${CMAKE_CURRENT_BINARY_DIR}/Grammar/ ${GrammarDir}/Shader.g ${GrammarDir}/GraphSyntax.g ${GrammarDir}/GraphSyntaxEval.g ${GrammarDir}/ShaderTreeWalk.g
	DEPENDS Grammar/Shader.g Grammar/GraphSyntax.g Grammar/GraphSyntaxEval.g Grammar/ShaderTreeWalk.g
	BYPRODUCTS Grammar/GraphSyntax.tokens Grammar/GraphSyntaxEval.tokens Grammar/ShaderTreeWalk.tokens)

BasicLibrary(ShaderParser "${Src};${Headers};${GeneratedGrammarFiles}")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_compile_options(ShaderParser PRIVATE -Wno-parentheses-equality -Wno-incompatible-pointer-types-discards-qualifiers)
endif()
