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
	SLOT_DECL;

	FUNCTION_PATH;
	FUNCTION_CALL;

	CONNECTION;
	SCOPED_CONNECTION;
	RCONNECTION_UNIQUE;
	RCONNECTION_REF;

	EXPORT;

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

functionPath : f=FileSpecLiteral -> ^(FUNCTION_PATH $f);
functionCall : f=functionPath '(' (scopedConnection (',' scopedConnection)*)? ')' -> ^(FUNCTION_CALL $f scopedConnection*);

lconnection : n=Identifier -> $n;
rconnection
	: f=functionCall '.' n0=Identifier -> ^(RCONNECTION_UNIQUE $f $n0)
	| frag=Identifier '.' n1=Identifier -> ^(RCONNECTION_REF $frag $n1)
	| c=StringLiteral -> ^(LITERAL $c)
	;
scopedConnection : l=lconnection ':' r=rconnection -> ^(SCOPED_CONNECTION $l $r);

declaration
	:	'node' n1=Identifier '=' f=functionCall -> ^(NODE_DECL $n1 $f)
	|	'slot' n0=Identifier '=' f0=functionPath -> ^(SLOT_DECL $n0 $f0)
	;

connection : n=Identifier '.' l=lconnection ':' r=rconnection -> ^(CONNECTION $n $l $r);

toplevel
	:	declaration ';' -> declaration
	|	connection ';' -> connection
	|	'export' n=Identifier ';' -> ^(EXPORT $n)
	;

entrypoint
	:	toplevels+=toplevel* -> $toplevels*
	;

//------------------------------------------------------------------------------
//						L E X E R 
//------------------------------------------------------------------------------

DecimalLiteral : ('0' | '1'..'9' '0'..'9'*) IntegerTypeSuffix? ;

fragment HexDigit : ('0'..'9'|'a'..'f'|'A'..'F') ;

fragment IntegerTypeSuffix : ('l'|'L'|'u'|'U'|'ul'|'UL') ;

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

FileSpecLiteral : '<' (~'>')* '>';

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
