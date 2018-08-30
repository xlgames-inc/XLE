grammar GraphSyntax;

options
{
	language = C;
	output = AST;
}

tokens
{
	TOPLEVEL;

	NODE_DECL;
	CAPTURES_DECL;

	FUNCTION_PATH;
	FUNCTION_CALL;

	IN_PARAMETER_DECLARATION;
	OUT_PARAMETER_DECLARATION;
	GRAPH_SIGNATURE;
	GRAPH_DEFINITION;
	IMPLEMENTS;

	CONNECTION;
	FUNCTION_CALL_CONNECTION;			// this is a connection expressed in function call syntax; eg -- graph(lhs:rhs)
	RCONNECTION_INLINE_FUNCTION_CALL;	// this is a rconnection which involves an inline function call; eg -- graph(lhs:graph2().result)
	RCONNECTION_REF;					// this is a reference to a node and connector
	RCONNECTION_IDENTIFIER;
	RCONNECTION_PARTIAL_INSTANTIATION;
	RETURN_CONNECTION;
	OUTPUT_CONNECTION;

	ATTRIBUTE_TABLE_NAME;
	ATTRIBUTE_TABLE;
	ATTRIBUTE;

	GRAPH_TYPE;

	IMPORT;

	LITERAL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
@parser::header
{
	#pragma warning(disable:4244)
	void CustomDisplayRecognitionError(void * recognizer, void * tokenNames);
}

@parser::apifuncs 
{
	RECOGNIZER->displayRecognitionError = CustomDisplayRecognitionError;
}

@lexer::header
{
	#pragma warning(disable:4244)
	void CustomDisplayRecognitionError(void * recognizer, void * tokenNames);
}

@lexer::apifuncs
{
	RECOGNIZER->displayRecognitionError = CustomDisplayRecognitionError;
}
///////////////////////////////////////////////////////////////////////////////////////////////////


// ------ B A S I C   C O M P O N E N T S ---------------------------------------------------------

functionPath 
	: i=Identifier '::' f=Identifier -> ^(FUNCTION_PATH $i $f)
	| f=Identifier -> ^(FUNCTION_PATH $f)
	;
functionCall : ('[[' attributeTableTable=Identifier ']]')? f=functionPath '(' (functionCallConnection (',' functionCallConnection)*)? ')' -> ^(FUNCTION_CALL $f ^(ATTRIBUTE_TABLE_NAME $attributeTableTable)? functionCallConnection*);

typeName
	: Identifier
	| 'graph' '<' functionPath '>' -> ^(GRAPH_TYPE functionPath)
	;

numberLiteral : HexLiteral | DecimalLiteral | OctalLiteral | FloatingPointLiteral;

valueLiteral
	: StringLiteral
	| numberLiteral
	| '{' numberLiteral (',' numberLiteral)* '}'
	;

// ------ S I G N A T U R E S ---------------------------------------------------------------------

signatureParameter
	: ('in')? type=typeName name=Identifier ('=' StringLiteral)? -> ^(IN_PARAMETER_DECLARATION $name $type StringLiteral?)
	| 'out' type=typeName name=Identifier -> ^(OUT_PARAMETER_DECLARATION $name $type)
	;

signatureParameters
	: '(' parameters += signatureParameter (',' parameters += signatureParameter)* ')' -> $parameters*
	| '(' ')'
	;

implementsQualifier
	: 'implements' fn=functionPath -> ^(IMPLEMENTS $fn)
	;

graphSignature
	: returnType=Identifier name=Identifier params=signatureParameters impl=implementsQualifier?
		-> ^(GRAPH_SIGNATURE $name $returnType $impl $params)
	;

// ------ M A I N   B O D Y -----------------------------------------------------------------------

lconnection : n=Identifier -> $n;
rconnection
	: f=functionCall 
		(
			'.' n0=Identifier		-> ^(RCONNECTION_INLINE_FUNCTION_CALL $f $n0)
			|						-> ^(RCONNECTION_PARTIAL_INSTANTIATION $f)
		)
	
	| frag=Identifier '.' n1=Identifier		-> ^(RCONNECTION_REF $frag $n1)
	| c=StringLiteral						-> ^(LITERAL $c)
	| ident=Identifier						-> ^(RCONNECTION_IDENTIFIER $ident)
	;
functionCallConnection : l=lconnection ':' r=rconnection -> ^(FUNCTION_CALL_CONNECTION $l $r);

nodeDeclaration
	:	'node' n1=Identifier '=' f=functionCall -> ^(NODE_DECL $n1 $f)
	;

connection
	: n=Identifier '.' l=lconnection ':' r=rconnection -> ^(CONNECTION $n $l $r)
	| out=Identifier '=' r=rconnection -> ^(OUTPUT_CONNECTION $out $r)
	| 'return' r=rconnection -> ^(RETURN_CONNECTION $r)
	;

capturesDeclaration
	: 'captures' Identifier '=' ('[[' attributeTableTable=Identifier ']]')? signatureParameters -> ^(CAPTURES_DECL Identifier ^(ATTRIBUTE_TABLE_NAME $attributeTableTable)? signatureParameters)
	;

graphStatement
	:	nodeDeclaration ';'? -> nodeDeclaration
	|	capturesDeclaration ';'? -> capturesDeclaration
	|	connection ';'? -> connection
	;

attributeDeclaration
	: attribute=Identifier ':' value=StringLiteral
		-> ^(ATTRIBUTE $attribute $value)
	;

toplevel
	:	'import' name=Identifier '=' source=StringLiteral 
			-> ^(IMPORT $name $source)
	|	sig=graphSignature '{' statements += graphStatement* '}'
			-> ^(GRAPH_DEFINITION $sig $statements*)
	|	'attributes' name=Identifier '(' (attributes += attributeDeclaration (',' attributes += attributeDeclaration)*)? ')'
			-> ^(ATTRIBUTE_TABLE $name $attributes*)
	;

entrypoint
	:	(toplevels+=toplevel ';'?)* -> $toplevels*
	;

//------------------------------------------------------------------------------
//						L E X E R 
//------------------------------------------------------------------------------

HexLiteral : '0' ('x'|'X') HexDigit+ IntegerTypeSuffix? ;

DecimalLiteral : ('0' | '1'..'9' '0'..'9'*) IntegerTypeSuffix? ;

OctalLiteral : '0' ('0'..'7')+ IntegerTypeSuffix? ;

fragment HexDigit : ('0'..'9'|'a'..'f'|'A'..'F') ;

fragment IntegerTypeSuffix : ('l'|'L'|'u'|'U'|'ul'|'UL') ;

FloatingPointLiteral
	:	('0'..'9')+ '.' ('0'..'9')* Exponent? FloatTypeSuffix?
	|	'.' ('0'..'9')+ Exponent? FloatTypeSuffix?
	|	('0'..'9')+ Exponent? FloatTypeSuffix?
	;

fragment Exponent : ('e'|'E') ('+'|'-')? ('0'..'9')+ ;

fragment FloatTypeSuffix : ('f'|'F'|'d'|'D') ;

fragment EscapeSequence
	:	'\\' ('b'|'t'|'n'|'f'|'r'|'\"'|'\''|'\\')
	|	UnicodeEscape
	|	OctalEscape
	;

fragment OctalEscape
	:	'\\' ('0'..'3') ('0'..'7') ('0'..'7')
	|	'\\' ('0'..'7') ('0'..'7')
	|	'\\' ('0'..'7')
	;

fragment UnicodeEscape
	:	'\\' 'u' HexDigit HexDigit HexDigit HexDigit
	;

StringLiteral
	:  '"' ( EscapeSequence | ~('\\'|'"') )* '"'
	;

fragment Letter
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

fragment JavaIDDigit
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

Identifier 
	:	Letter (Letter|JavaIDDigit)*
	;

WS  :  (' '|'\r'|'\t'|'\u000C'|'\n') {$channel=HIDDEN;}
	;

COMMENT
	:	'/*' ( options {greedy=false;} : . )* '*/' { $channel=HIDDEN; }
	;

LINE_COMMENT
	:	'//' ~('\n'|'\r')* '\r'? '\n' { $channel=HIDDEN; }
	;
