tree grammar GraphSyntaxEval;

options 
{
	tokenVocab = GraphSyntax;
	ASTLabelType = pANTLR3_BASE_TREE;
	language = C;
}

@header
{
	typedef unsigned NodeId;
	typedef unsigned ConnectorId;
	typedef unsigned ConnectionId;

	// #pragma warning(disable:4244)
	// void CustomDisplayRecognitionError(void * recognizer, void * tokenNames);

	typedef struct IdentifierAndScopeTag IdentifierAndScope;
}

@context
{
	const void* _userData;
}

@members
{
	NodeId RNode_Register(const void*, const char file[]);
	NodeId LSlot_Register(const void*, const char file[]);
	NodeId RSlot_Register(const void*, const char file[]);
	ConnectorId Connector_Register(const void*, NodeId node, const char connectorName[]);
	ConnectorId LiteralConnector_Register(const void*, const char literal[]);
	ConnectionId Connection_Register(const void*, ConnectorId left, ConnectorId right);

	void Node_Name(const void*, NodeId, const char name[]);
	void Slot_Name(const void*, NodeId, NodeId, const char name[]);
	NodeId RNode_Find(const void*, const char name[]);
	NodeId LNode_Find(const void*, const char name[]);

	void RNode_Push(const void*, NodeId);
	void RNode_Pop(const void*);
	NodeId RNode_GetActive(const void*);

	void LNode_Push(const void*, NodeId);
	void LNode_Pop(const void*);
	NodeId LNode_GetActive(const void*);

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

	struct IdentifierAndScopeTag
	{
		const char* _scope;
		const char* _identifier;
	};
}

/*@apifuncs
{
	RECOGNIZER->displayRecognitionError = CustomDisplayRecognitionError;
}*/

//------------------------------------------------------------------------------------------------

identifier returns [const char* str = NULL] : Identifier { str = (const char*)$Identifier.text->chars; };
functionPath returns [IdentifierAndScope res] 
	: ^(FUNCTION_PATH importSrc=identifier ident=identifier) { res._scope = importSrc; res._identifier = ident; }
	| ^(FUNCTION_PATH ident=identifier) { res._scope = NULL; res._identifier = ident; }
	;
stringLiteral returns [const char* str = NULL] : StringLiteral { str = (const char*)$StringLiteral.text->chars; };

rnode returns [NodeId node = ~0u]
	: ^(FUNCTION_CALL f=functionPath { $node = RNode_Register(ctx, f._identifier); RNode_Push(ctx, $node); } scopedConnection*)
	{
		RNode_Pop(ctx);
	}
	| nid=identifier { $node = RNode_Find(ctx, nid); }
	;

lnode returns [NodeId node = ~0u]
	: nid=identifier { $node = LNode_Find(ctx, nid); }
	;

scopedConnection returns [ConnectionId connection = ~0u]
	: ^(SCOPED_CONNECTION l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, RNode_GetActive(ctx), l);
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
		ConnectorId left = Connector_Register(ctx, LNode_GetActive(ctx), "result");
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
		connector = Connector_Register(ctx, LNode_GetActive(ctx), c0);
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

graphSignature returns [const char* name = NULL]
	: ^(GRAPH_SIGNATURE n=identifier returnType=identifier (^(PARAMETER_DECLARATION pname=identifier ptype=identifier))*) { $name = n; }
	;

toplevel
	: 
	/*| ^(SLOT_DECL n1id=identifier f=functionPath)
	{
		//char* s = StripAngleBrackets(f);
		NodeId lnode = LSlot_Register(ctx, f._identifier);
		NodeId rnode = RSlot_Register(ctx, f._identifier);
		//free(s);
		Slot_Name(ctx, lnode, rnode, n1id);
	}
	|*/ 
	
	^(IMPORT name=identifier source=stringLiteral) { /*Import_Register(name, source);*/ }
	| ^(GRAPH_DECLARATION 
		name=graphSignature { NodeId n = LSlot_Register(ctx, name); Node_Name(ctx, n, name); LNode_Push(ctx, n); } 
		graphStatement*
		{ LNode_Pop(ctx); }
	)

	//| ^(EXPORT Identifier)
	;

entrypoint : toplevel* ;
	
