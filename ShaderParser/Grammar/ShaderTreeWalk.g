tree grammar ShaderTreeWalk;

options
{
	tokenVocab = Shader;
	ASTLabelType = pANTLR3_BASE_TREE;
	language = C;
}

@header
{
	#pragma warning(disable:4068)	// unknown pragma
	#pragma GCC diagnostic ignored "-Wtypedef-redefinition"
	
	typedef unsigned StringId;
	typedef unsigned FormalArgId;
	typedef unsigned VariableId;
	typedef unsigned FunctionId;
	typedef unsigned ParameterStructId;
}

@includes
{
	#include <limits.h>
	#include <stdint.h>
}

@context
{
	const void* _userData;
}

@members
{
	struct FormalArg
	{
		StringId _name, _type, _semantic;
		unsigned _directionFlags;
	};

	struct Function
	{
		StringId _name, _returnType, _returnSemantic;
		unsigned _firstArg, _lastArg;
		unsigned _hasImplementation;
	};

	struct ParameterStruct
	{
		StringId _name;
		VariableId _firstParameter, _lastParameter;
	};

	StringId String_Register(const void*, const pANTLR3_BASE_TREE str);
	FormalArgId FormalArg_Register(const void*, struct FormalArg arg);
	VariableId Variable_Register(const void*, StringId name, StringId type, StringId semantic);
	FunctionId Function_Register(const void*, struct Function*);
	ParameterStructId ParameterStruct_Register(const void*, struct ParameterStruct*);
}

//------------------------------------------------------------------------------------------------

direction returns [unsigned directionFlags = 0u]
	: DIRECTION_OUT { directionFlags = 1<<1; }
	| DIRECTION_IN_OUT { directionFlags = (1<<0) | (1<<1); }
	| { directionFlags = 1<<0; }
	;

identifier returns [StringId str = ~0u] : Identifier { str = String_Register(ctx, $Identifier); } ;

typeName returns [StringId str = ~0u]
	: ^(TYPE_NAME s=identifier identifier*) { str = s; }	// extra identifiers can be for template arguments. We should in theory also support literals here
	;

semantic returns [StringId str = ~0u]
	: ^(SEMANTIC n=identifier) { str = n; }
	;

subscript : SUBSCRIPT;

formalArg returns [struct FormalArg result = (struct FormalArg){~0u, ~0u, ~0u, 0}]
	: ^(FORMAL_ARG
			n=identifier { result._name = n; }
			t=typeName { result._type = t; }
			subscript*
			(s=semantic { result._semantic = s; })?
			d=direction { result._directionFlags = d; }
		)
	;

function returns [struct Function result = (struct Function){~0u, ~0u, ~0u, UINT_MAX, 0, 0}]
	: ^(FUNCTION
			name=Identifier { result._name = String_Register(ctx, name); }
			ret=typeName { result._returnType = ret; }
			(a=formalArg
			{
				unsigned idx = FormalArg_Register(ctx, a);
				result._firstArg = (idx < result._firstArg) ? idx : result._firstArg;
				result._lastArg = (idx > result._lastArg) ? idx : result._lastArg;
			})*
			(retSemantic=semantic { result._returnSemantic = retSemantic; })?
			(^(BLOCK { result._hasImplementation = 1; }))?
		)
	;

parameterStruct returns [struct ParameterStruct result = (struct ParameterStruct){~0u, UINT_MAX, 0}]
	: ^((STRUCT | CBUFFER)
			n=identifier { result._name = n; }
			(^(VARIABLE t=typeName
				(
					^(VARIABLE_NAME n=identifier subscript* (s=semantic|{s=~0u;}))
					{
						VariableId var = Variable_Register(ctx, n, t, s);
						result._firstParameter = (var < result._firstParameter) ? var : result._firstParameter;
						result._lastParameter = (var > result._lastParameter) ? var : result._lastParameter;
					}
				)*
			))*
		)
	;

toplevel
	: p=parameterStruct { ParameterStruct_Register(ctx, &p); }
	| f=function { Function_Register(ctx, &f); }
	| ^(UNIFORM t=typeName
		(
			^(VARIABLE_NAME identifier subscript* semantic?)
		)*
	  )
	;

entrypoint : toplevel* ;
