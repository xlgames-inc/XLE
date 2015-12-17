//
// ANTLR grammar for HLSL files
//
// (C) 2009 Christian Schladetsch
// (C) 2009 Blue Lion Software
//
// Permission to use for any purpose is given, as long as this copyright
// information is included in any derived works.

//
//		Modified very heavily by DavidJ!
//			-- this is a very "permissive" grammar for HLSL
//				.. which means that many illegal things will work fine. For our
//				purposes, that is best.
//
//				The grammar is not perfect. I don't think Microsoft publishes
//				and exact grammar for HLSL (but we can get close with GLSL or cg).
//				And there are some things just hacked in... Really, we don't need
//				a perfect parse. We just need to be able to extract the limited 
//				things we're interested in (like function signatures and structure
//				layout)
//

grammar Shader;

options
{
	language = C;
	output = AST;
	backtrack = true; 
	memoize = true;
}

// virtual tokens for AST tree nodes
tokens
{
	SAMPLER;
	SAMPLER_FIELD_TEXTURE;
	SAMPLER_FIELD_ATTRIB;
	
	STRUCT;
	FUNCTION;
	LOCAL_VAR;
	CAST;
	CLASS;
	
	SEMANTIC;
	
	SUBSCRIPT;
	UNIFORM;
	VARIABLE;
	VARIABLE_NAME;
	FUNCTION_CALL;
	ARG_LIST;
	FORMAL_ARG;
	FORMAL_ARG_LIST;
	EMPTY_FORMAL_ARG_LIST;
	IDENT_LIST;
	ASSIGN;
	BLOCK;
	EMPTY;
	
	IF;
	IF_ELSE;
	ELSE;
	
	ASSERT;
	LAMBDA;
	FIELD_ACCESS;
	DEBUG_BREAK;
	BREAK;
	TRACE;
	LITERAL;

	DIRECTION_OUT;
	DIRECTION_IN_OUT;

	TOPLEVEL;
}

@header
{
	#pragma warning(disable:4244)
	void myDisplayRecognitionError (void * recognizer, void * tokenNames);
}

@members
{
}

@parser::apifuncs {
	RECOGNIZER->displayRecognitionError = myDisplayRecognitionError;
}

storage_class : 'extern' | 'nointerpolation' | 'precise' | 'shared' | 'groupshared' | 'static' | 'uniform' | 'volatile';
type_modifier : 'const' | 'row_major' | 'column_major';

fx_file
	:	toplevels+=toplevel* -> ^(TOPLEVEL $toplevels*)
	;
	
toplevel
	:	global
	|	structure
	|	cbuffer
	|	function
	|	compile_fragment
	|	'export' function_signature
	;

global
	:	uniform
	;
	
variablename_single : id=ident sub+=subscript* sem=semantic? registerAssignment? ('=' e=expression)? -> ^(VARIABLE_NAME $id $sub* $sem? $e?);
variablename_list	: variablename_single (',' variablename_single)* -> variablename_single+;

uniform
	:	(storage_class|type_modifier)* ty=type_name names=variablename_list ';' 
		-> ^(UNIFORM $ty $names)
	;

variable
	:	(storage_class|type_modifier)* ty=type_name names=variablename_list ';' 
		-> ^(VARIABLE $ty $names)
	;
	
subscript
	:	'[' (num=expression)? ']' -> ^(SUBSCRIPT $num?)
	;

registerValue : ident;

registerAssignment : ':' 'register' '(' registerValue ')';
	
structure
	:	'struct' ident '{' fields+=structure_field* '}' ';' 
		-> ^(STRUCT ident $fields*)
	;

interface_class
	:	('interface'|'class') ident '{' fields+=class_field* '}' ';'
		-> ^(CLASS ident $fields*)
	;

cbuffer
	:	'cbuffer' ident registerAssignment? '{' fields+=structure_field* '}'
		-> ^(STRUCT ident $fields*)
	;
	
structure_field
	:	variable
	|	isolated_macro
	;

class_field
	:	variable
	|	cbuffer
	|	function_signature
	;

function_attribute
	:	'[' 'numthreads' '(' expression ',' expression ',' expression ')' ']'
	|	'[' 'maxvertexcount' '(' expression ')' ']'
	|
	;

function
	:	function_attribute ret=ident name=ident '(' args=formal_arglist ')' semantic? block
		-> ^(FUNCTION $ret $name $args semantic? block)
	;

function_signature
	:	function_attribute ret=ident name=ident '(' args=formal_arglist ')' ';'
		-> ^(FUNCTION $ret $name $args)
	;

// Note --	isolated_macro macro represents some macro expression in the code that
//			will be expanded by the preprocessor. Since we don't support the preprocessor
//			when parsing, we should assume it does nothing, and just ignore it. This kind
//			of macro is often used for optional function parameters and optional structure
//			members. Unfortunately they will be lost! The only solution is to support 
//			the preprocessor... But that is impractical because of the behaviour of #include...
//
//		Unfortunately we can't know for sure if the identifier is truly a macro here... It would
//		be better if we could test for common naming conventions (eg, uppercase and underscores)
isolated_macro
	:	Identifier
	;
	
semantic
	:	':' n=Identifier -> ^(SEMANTIC $n)
	;

sampler_type_name : 'sampler' | 'sampler1D' | 'sampler2D' | 'sampler3D' | 'samplerCUBE' | 'sampler_state' | 'SamplerState' | 'SamplerComparisonState' ;

texture_type_name	: 'texture' 
					| 'Texture1D'	| 'Texture1DArray'
					| 'Texture2D'	| 'Texture2DArray'	| 'Texture2DMS'			| 'Texture2DMSArray'
					| 'Texture3D'	| 'TextureCube'		| 'TextureCubeArray'
					| 'RWTexture1D' | 'RWTexture1DArray'
					| 'RWTexture2D' | 'RWTexture2DArray'
					| 'RWTexture3D' | 'RWTexture3DArray'
					| 'tbuffer'

					| 'Texture2D_MaybeMS'		// (for convenient, include this custom #define)
					;

structuredBufferTypeName : 'StructuredBuffer' | 'RWStructuredBuffer';

type_name
	:	ident
	|	sampler_type_name
	|	texture_type_name ('<' ident '>')?
	|	structuredBufferTypeName '<' ident '>'
	;
	
compile_fragment
	:	('vertexfragment'^ | 'pixelfragment'^) ident '='! ('compile' | 'compile_fragment') ! ident ident arguments ';'!
	;
	
ident
	:	id=Identifier
	;	
	
block
	:	'{' statements+=statement* '}' -> ^(BLOCK $statements*)
	;
	
formal_arglist
	:	formal_arg (',' formal_arg)*
		-> formal_arg+
	|	-> /* nothing */
	;
	
formal_arg
	:	(dir=direction | storage_class | type_modifier)* ty=type_name id=ident sub+=subscript* sem=semantic? ('=' expression)?
		-> ^(FORMAL_ARG $ty $id $sub* $sem? $dir?)
	|	isolated_macro
	;
	
direction
	:	'in' | 'out' -> ^(DIRECTION_OUT) | 'inout' -> ^(DIRECTION_IN_OUT)
	;
	
literal
	:	d=DecimalLiteral			// -> ^(LITERAL $d)
	|	f=FloatingPointLiteral		// -> ^(LITERAL $f)
	|	h=HexLiteral				//-> ^(LITERAL $h)
	;

//---------------------------------------------------------------------------------
//					 S t a t e m e n t 
//---------------------------------------------------------------------------------

statement
	:	block
	|	if_block
	|	for_loop
	|	while_loop
	|	do_while_loop
	|	'delete'^ expression
	|	lc='assert' e=parExpression ';' -> ^(ASSERT[$lc] $e)
	|	'return' expression? ';' 
		-> ^('return' expression)
	|	'asm_break' ';'!
	|	'break' ';'!
	|	'continue' ';'!
	|	';'!
	|	statementExpression ';'!
	|	isolated_macro
	// also -- switch
	;
	
loop_attribute	: '[' 'unroll' ('(' ident ')')? ']'
				| '[' 'loop' ']' 
				| ;
cond_attribute	: '[' 'branch' ']'
				| '[' 'flatten' ']' 
				| ;

for_loop
	:	loop_attribute 'for' '(' start=statementExpression? ';' cond=expression? ';' next=expression? ')' body=statement
		-> ^('for' $cond $next $body $start )
	;

do_while_loop
	:	loop_attribute 'do' body=statement 'while' '(' cond=expression ')' ';'
		-> ^('do' $cond $body )
	;

while_loop
	:	loop_attribute 'while' '(' cond=expression ')' body=statement
		-> ^('while' $cond $body )
	;
	
// TODO: there is a more efficient way to do this, and still generate the correct AST tree
if_block
	:	cond_attribute 'if' parExpression statement 'else' statement -> ^(IF_ELSE statement statement parExpression)
	|	cond_attribute 'if' parExpression statement -> ^(IF statement parExpression)
	;

statementExpression
	:	expression
	|	(storage_class|type_modifier)* ty=ident names=variablename_list		-> ^(LOCAL_VAR $ty $names)
	;
	
//---------------------------------------------------------------------------------
//					 E x p r e s s i o n
//---------------------------------------------------------------------------------

expressionList
	:	expression (','! expression)*
	;

curly_brace_initialiser
	: '{' expression (',' expression)* '}'
	| '{' '}'
	;

expression
	:	conditionalExpression (assignmentOperator^ expression)?
	|	curly_brace_initialiser
	;
	
assignmentOperator
	:	'='
	|	'+='
	|	'-='
	|	'*='
	|	'/='
	|	'&='
	|	'|='
	|	'^='
	|	'%='
	|	'>>='
	|	'<<='
	;

conditionalExpression
	:	conditionalOrExpression ( '?' expression ':' expression )?
	;

conditionalOrExpression
	:	conditionalAndExpression ( '||'^ conditionalAndExpression )*
	;

conditionalAndExpression
	:	inclusiveOrExpression ( '&&'^ inclusiveOrExpression )*
	;

inclusiveOrExpression
	:	exclusiveOrExpression ( '|'^ exclusiveOrExpression )*
	;

exclusiveOrExpression
	:	andExpression ( '^'^ andExpression )*
	;

andExpression
	:	equalityExpression ( '&'^ equalityExpression )*
	;

equalityExpression
	:	instanceOfExpression ( ('=='^ | '!='^) instanceOfExpression )*
	;

instanceOfExpression
	:	relationalExpression
	;

relationalExpression
	:	shiftExpression ( relationalOp^ shiftExpression )*
	;
	
relationalOp
	:	('<' '=' | '>' '=' | '<' | '>')
	;

shiftExpression
	:	additiveExpression ( shiftOp^ additiveExpression )*
	;

// TODO: need a sem pred to check column on these >>>
shiftOp
	:	('<' '<' | '>' '>')
	;

additiveExpression
	:	multiplicativeExpression ( ('+'^ | '-'^) multiplicativeExpression )*
	;

multiplicativeExpression
	:	unaryExpression ( ( '*'^ | '/'^ | '%'^ ) unaryExpression )*
	;
	
unaryExpression
	:	'+'^ unaryExpression
	|	'-'^ unaryExpression
	|	'++'^ unaryExpression
	|	'--'^ unaryExpression
	|	unaryExpressionNotPlusMinus
	;
	
unaryExpressionNotPlusMinus
	:	'~'^ unaryExpression
	| 	'!'^ unaryExpression
	|	postfixExpression ('++'^|'--'^)?
	;
	
postfixExpression
	:	(primary->primary) // set return tree to just primary
		(
			arguments				-> ^(FUNCTION_CALL arguments $postfixExpression)
			|	'[' expression ']'	-> ^('[' $postfixExpression expression)
			|	'.' primary			-> ^('.' $postfixExpression primary)
		)*
	;
	
primary
	:	cast_expression
	|	parExpression
	|	literal
	|	ident
	;
	
cast_expression
	:	'(' ty=type_name ')' e=expression
		-> ^(CAST $ty $e)
	;
	
parExpression
	:	'(' expression ')' -> expression
	;
	
arguments
	:	'(' expressionList? ')' -> ^(ARG_LIST expressionList)
	;

//--------------------------------------------------------------------
//						L E X E R 
//--------------------------------------------------------------------

HexLiteral : '0' ('x'|'X') HexDigit+ IntegerTypeSuffix? ;

DecimalLiteral : ('0' | '1'..'9' '0'..'9'*) IntegerTypeSuffix? ;

OctalLiteral : '0' ('0'..'7')+ IntegerTypeSuffix? ;

fragment
HexDigit : ('0'..'9'|'a'..'f'|'A'..'F') ;

fragment
IntegerTypeSuffix : ('l'|'L') ;

FloatingPointLiteral
	:	('0'..'9')+ '.' ('0'..'9')* Exponent? FloatTypeSuffix?
	|	'.' ('0'..'9')+ Exponent? FloatTypeSuffix?
	|	('0'..'9')+ (	  Exponent FloatTypeSuffix?
						| FloatTypeSuffix
					)
	;

fragment
Exponent : ('e'|'E') ('+'|'-')? ('0'..'9')+ ;

fragment
FloatTypeSuffix : ('f'|'F'|'d'|'D') ;

CharacterLiteral
	:	'\'' ( EscapeSequence | ~('\''|'\\') ) '\''
	;

StringLiteral
	:  '"' ( EscapeSequence | ~('\\'|'"') )* '"'
	;

fragment
EscapeSequence
	:	'\\' ('b'|'t'|'n'|'f'|'r'|'\"'|'\''|'\\')
	|	UnicodeEscape
	|	OctalEscape
	;

fragment
OctalEscape
	:	'\\' ('0'..'3') ('0'..'7') ('0'..'7')
	|	'\\' ('0'..'7') ('0'..'7')
	|	'\\' ('0'..'7')
	;

fragment
UnicodeEscape
	:	'\\' 'u' HexDigit HexDigit HexDigit HexDigit
	;
	
Identifier 
	:	Letter (Letter|JavaIDDigit)*
	;

QuotedIdentifier 
	:	'`' Identifier
	;

/**I found this char range in JavaCC's grammar, but Letter and Digit overlap.
	Still works, but...
 */
fragment
Letter
	:  '\u0024' |
		'\u0041'..'\u005a' |
		'\u005f' |
		'\u0061'..'\u007a' |
		'\u00c0'..'\u00d6' |
		'\u00d8'..'\u00f6' |
		'\u00f8'..'\u00ff' |
		'\u0100'..'\u1fff' |
		'\u3040'..'\u318f' |
		'\u3300'..'\u337f' |
		'\u3400'..'\u3d2d' |
		'\u4e00'..'\u9fff' |
		'\uf900'..'\ufaff'
	;

fragment
JavaIDDigit
	:  '\u0030'..'\u0039' |
		'\u0660'..'\u0669' |
		'\u06f0'..'\u06f9' |
		'\u0966'..'\u096f' |
		'\u09e6'..'\u09ef' |
		'\u0a66'..'\u0a6f' |
		'\u0ae6'..'\u0aef' |
		'\u0b66'..'\u0b6f' |
		'\u0be7'..'\u0bef' |
		'\u0c66'..'\u0c6f' |
		'\u0ce6'..'\u0cef' |
		'\u0d66'..'\u0d6f' |
		'\u0e50'..'\u0e59' |
		'\u0ed0'..'\u0ed9' |
		'\u1040'..'\u1049'
	;

WS  :  (' '|'\r'|'\t'|'\u000C'|'\n') {$channel=HIDDEN;}
	;

COMMENT
	:	'/*' ( options {greedy=false;} : . )* '*/' {$channel=HIDDEN;}
	;

LINE_COMMENT
	:	'//' ~('\n'|'\r')* '\r'? '\n'	{$channel=HIDDEN;}
	;

//	Hide any line that begins with "#" (just ignore preprocessor stuff)... 
//	We also need to suport the '\' line extension... so just eat everything
//	until we hit a newline that isn't preceeded by a '\'. Of course, this will
//	mean any preprocessor directive can get extended to the next line (not just
//	#define)
PRE_PROCESSOR_LINE
	:	'#' (options {greedy=false;}: . )* (~'\\' '\r'? '\n')	{$channel=HIDDEN;}
	;

// EOF
