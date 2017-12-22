tree grammar GraphSyntaxEval;

options 
{
	tokenVocab = GraphSyntax;
	ASTLabelType = pANTLR3_BASE_TREE;
	language = C;
}

@header
{
	#pragma GCC diagnostic ignored "-Wtypedef-redefinition"

	typedef unsigned NodeId;
	typedef unsigned ConnectorId;
	typedef unsigned ConnectionId;
	typedef unsigned GraphSignatureId;

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

	NodeId RNode_Register(const void*, IdentifierAndScope identifierAndScope);
	NodeId RSlot_Register(const void*, IdentifierAndScope identifierAndScope);
	NodeId Graph_Register(const void*, const char name[], GraphSignatureId signature);
	ConnectorId Connector_Register(const void*, NodeId node, const char connectorName[]);
	ConnectorId LiteralConnector_Register(const void*, const char literal[]);
	ConnectionId Connection_Register(const void*, ConnectorId left, ConnectorId right);

	void Node_Name(const void*, NodeId, const char name[]);
	NodeId RNode_Find(const void*, const char name[]);
	NodeId Graph_Find(const void*, const char name[]);

	GraphSignatureId GraphSignature_Register(const void*);
	void GraphSignature_ReturnType(const void*, GraphSignatureId, const char returnType[]);
	void GraphSignature_AddParameter(const void*, GraphSignatureId, const char name[], const char type[]);

	typedef unsigned ObjTypeId;
	static const unsigned ObjType_Graph = 0;
	static const unsigned ObjType_Node = 1;
	static const unsigned ObjType_GraphSignature = 2;

	void Walk_Push(const void*, ObjTypeId objType, unsigned obj);
	void Walk_Pop(const void*, ObjTypeId objType);
	unsigned Walk_GetActive(const void*, ObjTypeId objType);

	void Import_Register(const void*, const char alias[], const char import[]);

	char* StripAngleBrackets(const char* input)
	{
		if (!input || !input[0]) return NULL;
		if (input[0] != '<') return strdup(input);

		char* result = strdup(input+1);
		size_t len = strlen(result);
		if (len && result[len-1] == '>') result[len-1] = '\0';
		return result;
	}

	char* StripQuotesBrackets(const char* input)
	{
		if (!input || !input[0]) return NULL;
		if (input[0] != '"') return strdup(input);

		char* result = strdup(input+1);
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
	RECOGNIZER->displayRecognitionError = CustomDisplayRecognitionError;
}*/

//------------------------------------------------------------------------------------------------

identifier returns [const char* str = NULL] : Identifier { str = (const char*)$Identifier.text->chars; };
identifierToken returns [pANTLR3_COMMON_TOKEN str = NULL] : Identifier { str = (*$Identifier->getToken)($Identifier); };
functionPath returns [IdentifierAndScope res = (IdentifierAndScope){NULL, NULL}] 
	: ^(FUNCTION_PATH importSrc=identifierToken ident=identifierToken) { res._scope = importSrc; res._identifier = ident; }
	| ^(FUNCTION_PATH ident=identifierToken) { res._scope = NULL; res._identifier = ident; }
	;
stringLiteral returns [const char* str = NULL] : StringLiteral { str = (const char*)$StringLiteral.text->chars; };

rnode returns [NodeId node = ~0u]
	: ^(FUNCTION_CALL f=functionPath { $node = RNode_Register(ctx, f); Walk_Push(ctx, ObjType_Node, $node); } scopedConnection*)
	{
		Walk_Pop(ctx, ObjType_Node);
	}
	| nid=identifier { $node = RNode_Find(ctx, nid); }
	;

lnode returns [NodeId node = ~0u]
	: nid=identifier { $node = Graph_Find(ctx, nid); }
	;

scopedConnection returns [ConnectionId connection = ~0u]
	: ^(SCOPED_CONNECTION l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, Walk_GetActive(ctx, ObjType_Node), l);
		$connection = Connection_Register(ctx, left, r);
	}
	;

connection returns [ConnectionId connection = ~0u]
	: ^(CONNECTION n=lnode l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, n, l);
		$connection = Connection_Register(ctx, left, r);
	}
	| ^(RETURN_CONNECTION r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, ~0u, "result");
		$connection = Connection_Register(ctx, left, r);
	}
	;

rconnection returns [ConnectorId connector = ~0u]
	: ^((RCONNECTION_UNIQUE|RCONNECTION_REF) n0=rnode c0=identifier)
	{
		connector = Connector_Register(ctx, n0, c0);
	}
	| ^((RCONNECTION_IDENTIFIER) c0=identifier)
	{
		connector = Connector_Register(ctx, ~0u, c0);
	}
	| ^(LITERAL StringLiteral)
	{
		char* s = StripQuotesBrackets((const char*)$StringLiteral.text->chars);
		connector = LiteralConnector_Register(ctx, s);
		free(s);
	}
	;

graphStatement
	: connection
	| ^(NODE_DECL nid=identifier n=rnode)
	{
		Node_Name(ctx, n, nid);
	}
	;

graphSignature returns [GraphSignatureAndName graphSig = (GraphSignatureAndName){~0u, NULL}]
	: ^(GRAPH_SIGNATURE 
		{
			$graphSig._sigId = GraphSignature_Register(ctx);
		}
		n=identifier returnType=identifier 
		((^(PARAMETER_DECLARATION pname=identifier ptype=identifier)) 
		{
			GraphSignature_AddParameter(ctx,$graphSig._sigId, pname, ptype); 
		})*)

		{
			$graphSig._name = n;
			GraphSignature_ReturnType(ctx, $graphSig._sigId, returnType); 
		}
	;

toplevel
	: 
	^(IMPORT alias=identifier source=stringLiteral) 
	{ 
		char* stripped = StripQuotesBrackets(source);
		Import_Register(ctx, alias, stripped); 
		free(stripped);
	}
	| ^(GRAPH_DECLARATION
		sig=graphSignature 
		{ 
			NodeId n = Graph_Register(ctx, sig._name, sig._sigId); 
			Walk_Push(ctx, ObjType_Graph, n); 
		}

		graphStatement*

		{ Walk_Pop(ctx, ObjType_Graph); }
	)
	;

entrypoint : toplevel* ;
	


