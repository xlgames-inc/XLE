//
// ANTLR Tree for shader files
//
// (C) 2009 Christian Schladetsch
// (C) 2009 Blue Lion Software
//
// Permission to use for any purpose is given, as long as this copyright
// information is included in any derived works.

tree grammar ShaderTree;

options 
{
	tokenVocab = Shader;				// reuse token types
	ASTLabelType = pANTLR3_BASE_TREE;
	language = C;
	output = AST;
}

//------------------------------------------------------------------------------------------------

entry
	:	a+=toplevel* 
		// -> file(contents={$a})
	;
	
toplevel
	:	function // -> {$function.st}
	|	uniform // -> {$uniform.st}
	|	texture // -> {$texture.st}
	|	sampler // -> {$sampler.st}
	|	structure // -> {$structure.st}
	;
	
function
	:	^(FUNCTION r=ident n=ident a+=formal_arg* semantic? block) 
		// -> function(ret={$r.st}, name={$n.st}, args={$a}, semantic={$semantic.st}, body={$block.st})
	;
	
uniform
	:	^(UNIFORM ty=ident name=ident sub=subscript? sem=semantic?)
		// -> uniform(type={$ty.st}, name={$name.st}, subscript={$sub.st}, semantic={$sem.st})
	;
	
texture
	:	^('texture' ident) // -> texture(tex_name={$ident.st})
	;
	
structure
	:	^(STRUCT ident fields+=struct_field*)
		// -> structure(name={$ident.st}, fields={$fields})
	;
	
struct_field
	:	u=uniform // -> {$u.st}
	;
	
sampler
	:	^(SAMPLER ident f+=sampler_field*)
		// -> sampler(name={$ident.st}, fields={$f})
	;
	
sampler_field
	:	^('Texture' ident)
		// -> sampler_field_texture(tex_name={$ident.st})
	|	^('=' ident expression) 
		// -> sampler_field_assign(ident={$ident.st}, value={$expression.st})
	;
	
block
	:	^(BLOCK s+=statement*) // -> block(statements={$s})
	;
	
ident
	:	id=Identifier // -> {%{id.Text}}
	|	id=UserSemantic // -> {%{id.Text}}
	;
	
formal_arg
	:	^(FORMAL_ARG ty=type_name name=ident dir=direction? sub=subscript? sem=semantic?)
		// -> formal_arg(type={$ty.st}, name={$name.st}, dir={$dir.st}, subscript={$sub.st}, semantic={$sem.st})
	;
	
direction
	:	'in' // -> {%{"in"}}
	|	'out' // -> {%{"out"}}
	;
	
type_name
	:	id=ident // -> {$id.st}
	|	'sampler2D' // -> {%{"sampler2D"}}
	;
	
	
subscript
	:	^(SUBSCRIPT e=expression) // -> subscript(index={$e.st})
	;
	
semantic
	:	^(SEMANTIC n=ident) // -> semantic(name={$n.st})
	|	^(USER_SEMANTIC r=ident) // -> user_semantic(name={$r.st})
	;
	
statement
	:	expression 
		// -> statement(expr={$expression.st})
	|	local_var
		// -> statement(expr={$local_var.st})
	|	block
	;

local_var
	:	^(LOCAL_VAR ty=ident id=ident expression?)
		// -> local_var(type={$ty.st}, name={$id.st}, init={$expression.st})
	;
	
expression
	:	^('=' l=expression r=expression)		
		// -> assign(left={$l.st}, right={$r.st})
	
	|	^(CAST ty=expression e=expression)
		// -> cast(type={$ty.st}, expr={$e.st})
		
	|	^('return' e=expression?)
		// -> return(expr={$e.st})

	|	^(IF statement expression)			
	
	|	^('!=' a=expression b=expression)		
		// -> bin_op(op={"!="}, left={$a.st}, right={$b.st})
		
	|	^('<' a=expression b=expression)		
		// -> bin_op(op={"<"}, left={$a.st}, right={$b.st})
	
	|	^('-' a=expression b=expression)		
		// -> bin_op(op={"-"}, left={$a.st}, right={$b.st})
	
	|	^('+' a=expression b=expression)		
		// -> bin_op(op={"+"}, left={$a.st}, right={$b.st})
	
	|	^('+=' a=expression b=expression)		
		// -> bin_op(op={"+="}, left={$a.st}, right={$b.st})
	
	|	^('*' a=expression b=expression)		
		// -> bin_op(op={"*"}, left={$a.st}, right={$b.st})
		
	|	^('/' a=expression b=expression)		
		// -> bin_op(op={"/"}, left={$a.st}, right={$b.st})
		
	|	^('||' expression expression)		
		
	|	^('&&' expression expression)		
		
	|	^('!' expression)					
		
	|	literal 
		// -> literal(val={$literal.st})
		
	|	ident 
		// -> ident(name={$ident.st})
		
	|	^(FUNCTION_CALL args=arguments n=expression)		
		// -> function_call(name={$n.st}, args={$args.st})
		
	|	^('[' obj=expression field=expression)

	|	^('.' obj=expression field=expression)				
		// -> field_access(object={$obj.st}, field={$field.st})

	;

arguments
	:	^(ARG_LIST e+=expression*)
		// -> arg_list(args={$e})
	;
	
literal	
	:	n=integerLiteral			//{ Append(atoi((const char *)$n.text->chars)); }
		// -> {$n.st}
	|	f=FloatingPointLiteral		//{ Append((float)atof((const char *)$f.text->chars)); }
		// -> {%{$f.Text}}
	|	CharacterLiteral
	|	s=StringLiteral				//{ Append(StripQuotes((const char *)$s.text->chars)); }
	|	b=booleanLiteral			//{ Append(String((const char *)$b.text->chars) == "true"); }
	|	'null'
	;

integerLiteral
	:	a=HexLiteral // -> {%{$a.Text}}
	|	b=OctalLiteral // -> {%{$b.Text}}
	|	c=DecimalLiteral // -> {%{$c.Text}}
	;                    

booleanLiteral
	:	'true'
	|	'false'
	;

//EOF
