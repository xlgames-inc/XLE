tree grammar GraphSyntaxEval;

options 
{
	tokenVocab = GraphSyntax;
	ASTLabelType = pANTLR3_BASE_TREE;
	language = C;
}

@header
{
	#pragma warning(disable:4068)	// unknown pragma
	#pragma GCC diagnostic ignored "-Wtypedef-redefinition"

	typedef unsigned NodeId;
	typedef unsigned GraphId;
	typedef unsigned ConnectorId;
	typedef unsigned ConnectionId;
	typedef unsigned GraphSignatureId;
	typedef unsigned AttributeTableId;

	// #pragma warning(disable:4244)
	// void CustomDisplayRecognitionError(void * recognizer, void * tokenNames);

	typedef struct IdentifierAndScopeTag IdentifierAndScope;
	typedef struct GraphSignatureAndNameTag GraphSignatureAndName;
}

@context
{
	const void* _userData;
}

@members
{
	struct IdentifierAndScopeTag
	{
		pANTLR3_COMMON_TOKEN _scope;
		pANTLR3_COMMON_TOKEN _identifier;
	};

	NodeId Node_Register(const void*, GraphId gid, IdentifierAndScope identifierAndScope, const char attributeTableName[]);
	ConnectorId Connector_Register(const void*, GraphId gid, NodeId node, const char connectorName[]);
	ConnectorId LiteralConnector_Register(const void*, GraphId gid, const char literal[]);
	ConnectorId IdentifierConnector_Register(const void*, GraphId gid, IdentifierAndScope identifierAndScope);
	ConnectorId PartialInstantiationConnector_Register(const void* ctx, GraphId gid, NodeId node);
	ConnectionId Connection_Register(const void*, GraphId gid, ConnectorId left, ConnectorId right);
	void Connection_SetCondition(const void* ctx, GraphId gid, ConnectionId connection, const char condition[]);

	void Node_Name(const void*, GraphId, NodeId, const char name[]);
	NodeId Node_Find(const void*, GraphId, const char name[]);

	GraphId Graph_Register(const void*, const char name[], GraphSignatureId signature);

	GraphSignatureId GraphSignature_Register(const void*);
	void GraphSignature_ReturnType(const void*, GraphSignatureId, const char returnType[]);
	void GraphSignature_AddParameter(const void*, GraphSignatureId, const char name[], const char type[], unsigned direction, const char def[]);
	void GraphSignature_AddGraphParameter(const void*, GraphSignatureId, const char name[], IdentifierAndScope prototype);
	void GraphSignature_Implements(const void*, GraphSignatureId, IdentifierAndScope templ);

	AttributeTableId AttributeTable_Register(const void*, const char name[]);
	void AttributeTable_AddValue(const void*, AttributeTableId, const char key[], const char value[]);

	void Import_Register(const void*, const char alias[], const char import[]);

	void Captures_Register(const void*, GraphId, const char name[], GraphSignatureId params, const char attributeTableName[]);

	char* StringDupe(const char* input)
	{
		size_t len = strlen(input);
		char* res = malloc(len+1);
		if (!res) return res;
		strcpy(res, input);
		return res;
	}

	char* StripQuotesBrackets(const char* input)
	{
		if (!input || !input[0]) return NULL;
		if (input[0] != '"') return StringDupe(input);

		char* result = StringDupe(input+1);
		size_t len = strlen(result);
		if (len && result[len-1] == '"') result[len-1] = '\0';
		return result;
	}

	struct GraphSignatureAndNameTag {
		GraphSignatureId _sigId;
		const char* _name;
	};
}

/*@apifuncs
{
	RECOGNIZER->displayRecognitionError = (void (*)(struct ANTLR3_BASE_RECOGNIZER_struct *, pANTLR3_UINT8 *))&CustomDisplayRecognitionError;
}*/

//------------------------------------------------------------------------------------------------

// ------ B A S I C   C O M P O N E N T S ---------------------------------------------------------

identifier returns [const char* str = NULL] : Identifier { str = (const char*)$Identifier.text->chars; };
identifierToken returns [pANTLR3_COMMON_TOKEN str = NULL] : Identifier { str = (*$Identifier->getToken)($Identifier); };
functionPath returns [IdentifierAndScope res = (IdentifierAndScope){NULL, NULL}] 
	: ^(FUNCTION_PATH importSrc=identifierToken ident=identifierToken) { res._scope = importSrc; res._identifier = ident; }
	| ^(FUNCTION_PATH ident=identifierToken) { res._scope = NULL; res._identifier = ident; }
	;
stringLiteral returns [const char* str = NULL] : StringLiteral { str = (const char*)$StringLiteral.text->chars; };

// ------ S I G N A T U R E S ---------------------------------------------------------------------

graphParameter[GraphSignatureId sigId]
	: ^(IN_PARAMETER_DECLARATION pname=identifier ptype=identifier) 						{ GraphSignature_AddParameter(ctx, $sigId, pname, ptype, 0, NULL); }
	| ^(IN_PARAMETER_DECLARATION pname=identifier ptype=identifier def=stringLiteral)		{ char* stripped = StripQuotesBrackets(def); GraphSignature_AddParameter(ctx, $sigId, pname, ptype, 0, stripped); free(stripped); }
	| ^(OUT_PARAMETER_DECLARATION pname=identifier ptype=identifier)						{ GraphSignature_AddParameter(ctx, $sigId, pname, ptype, 1, NULL); }
	| ^(IN_PARAMETER_DECLARATION pname=identifier ^(GRAPH_TYPE prototype=functionPath))		{ GraphSignature_AddGraphParameter(ctx, $sigId, pname, prototype);  }
	;

implementsQualifier[GraphSignatureId sigId]
	:^(IMPLEMENTS fn=functionPath) { GraphSignature_Implements(ctx, $sigId, fn); }
	;

graphSignature returns [GraphSignatureAndName graphSig = (GraphSignatureAndName){~0u, NULL}]
	: ^(GRAPH_SIGNATURE
		{
			$graphSig._sigId = GraphSignature_Register(ctx);
		}

		n=identifier returnType=identifier
		{
			$graphSig._name = n;
			if (strcmp(returnType, "void") != 0)
				GraphSignature_ReturnType(ctx, $graphSig._sigId, returnType);
		}

		implementsQualifier[$graphSig._sigId]?
		
		graphParameter[$graphSig._sigId]*
	);

// ------ M A I N   B O D Y -----------------------------------------------------------------------

optionalAttributeTableName returns [const char* res = NULL]
	: ^(ATTRIBUTE_TABLE_NAME atn=identifier) { res = atn; }
	| ;

rnode returns [NodeId node = ~0u]
	: ^(FUNCTION_CALL f=functionPath atn=optionalAttributeTableName { $node = Node_Register(ctx, $graphDefinition::graphId, f, atn); } functionCallConnection[$node]*)
	| nid=identifier { $node = Node_Find(ctx, $graphDefinition::graphId, nid); }
	;

lnode returns [NodeId node = ~0u]
	: nid=identifier { $node = Node_Find(ctx, $graphDefinition::graphId, nid); }
	;

functionCallConnection[NodeId rnode] returns [ConnectionId connection = ~0u]
	: ^(FUNCTION_CALL_CONNECTION l=identifier r=rconnection)
		{
			ConnectorId left = Connector_Register(ctx, $graphDefinition::graphId, $rnode, l);
			$connection = Connection_Register(ctx, $graphDefinition::graphId, left, r);
		}
	;

connection returns [ConnectionId connection = ~0u]
	: ^(CONNECTION n=lnode l=identifier r=rconnection)
		{
			ConnectorId left = Connector_Register(ctx, $graphDefinition::graphId, n, l);
			$connection = Connection_Register(ctx, $graphDefinition::graphId, left, r);
		}
	| ^(OUTPUT_CONNECTION out=identifier r=rconnection)
		{
			ConnectorId left = Connector_Register(ctx, $graphDefinition::graphId, ~0u, out);
			$connection = Connection_Register(ctx, $graphDefinition::graphId, left, r);
		}
	| ^(RETURN_CONNECTION r=rconnection)
		{
			ConnectorId left = Connector_Register(ctx, $graphDefinition::graphId, ~0u, "result");
			$connection = Connection_Register(ctx, $graphDefinition::graphId, left, r);
		}
	;

rconnection returns [ConnectorId connector = ~0u]
	: ^((RCONNECTION_INLINE_FUNCTION_CALL|RCONNECTION_REF) n0=rnode c0=identifier)
		{
			connector = Connector_Register(ctx, $graphDefinition::graphId, n0, c0);
		}
	| ^(RCONNECTION_FUNCTION_PATH ^(FUNCTION_PATH importSrc=identifierToken ident0=identifierToken))
		{
			IdentifierAndScope i;
			i._scope = importSrc;
			i._identifier = ident0;
			connector = IdentifierConnector_Register(ctx, $graphDefinition::graphId, i);
		}
	| ^(RCONNECTION_IDENTIFIER ident1=identifier)
		{
			connector = Connector_Register(ctx, $graphDefinition::graphId, ~0u, ident1);
		}
	| ^(RCONNECTION_PARTIAL_INSTANTIATION n1=rnode)
		{
			connector = PartialInstantiationConnector_Register(ctx, $graphDefinition::graphId, n1);
		}
	| ^(LITERAL StringLiteral)
		{
			char* s = StripQuotesBrackets((const char*)$StringLiteral.text->chars);
			connector = LiteralConnector_Register(ctx, $graphDefinition::graphId, s);
			free(s);
		}
	;

capturesDeclaration scope { GraphSignatureId sigId; }
	: ^(CAPTURES_DECL { $capturesDeclaration::sigId = GraphSignature_Register(ctx); }
		cid=identifier
		atn=optionalAttributeTableName
		graphParameter[$capturesDeclaration::sigId]*)
		{ 
			Captures_Register(ctx, $graphDefinition::graphId, cid, $capturesDeclaration::sigId, atn); 
		}
	;

graphStatement
	: connection
	| ^(NODE_DECL nid=identifier n=rnode) { Node_Name(ctx, $graphDefinition::graphId, n, nid); }
	| capturesDeclaration
	| ^(CONDITIONAL_CONNECTION StringLiteral c=connection)
		{
			char* s = StripQuotesBrackets((const char*)$StringLiteral.text->chars);
			Connection_SetCondition(ctx, $graphDefinition::graphId, c, s);
			free(s);
		}
	;

graphDefinition returns [GraphId result = ~0u;] scope { GraphId graphId; }
	: ^(GRAPH_DEFINITION
		sig=graphSignature
		{
			$result = Graph_Register(ctx, sig._name, sig._sigId);
			$graphDefinition::graphId = $result;
		}

		graphStatement*
	);

attributeDefinition [AttributeTableId tableId]
	: ^(ATTRIBUTE k=identifier v=stringLiteral) { char* s = StripQuotesBrackets(v); AttributeTable_AddValue(ctx, tableId, k, s); free(s); }
	;

attributeTableDefinition returns [AttributeTableId result = ~0u;]
	: ^(ATTRIBUTE_TABLE
		name=identifier { $result = AttributeTable_Register(ctx, name); }
		attributeDefinition[$result]*)
	;

toplevel
	: ^(IMPORT alias=identifier source=stringLiteral)
		{
			char* stripped = StripQuotesBrackets(source);
			Import_Register(ctx, alias, stripped);
			free(stripped);
		}
	| graphDefinition
	| attributeTableDefinition
	;

entrypoint : toplevel* ;
	
