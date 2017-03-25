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
}

@context
{
	const void* _userData;
}

@members
{
	NodeId LNode_Register(const void*, const char file[]);
	NodeId RNode_Register(const void*, const char file[]);
	NodeId SlotParams_Register(const void*, const char file[]);
	ConnectorId Connector_Register(const void*, NodeId node, const char connectorName[]);
	ConnectorId LiteralConnector_Register(const void*, const char literal[]);
	ConnectionId Connection_Register(const void*, ConnectorId left, ConnectorId right);

	void Node_Name(const void*, NodeId, const char name[]);
	void Slot_Name(const void*, NodeId, NodeId, const char name[]);
	NodeId RNode_Find(const void*, const char name[]);
	NodeId LNode_Find(const void*, const char name[]);
	void Node_Push(const void*, NodeId);
	void Node_Pop(const void*);
	NodeId Node_GetActive(const void*);

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
}

/*@apifuncs
{
	RECOGNIZER->displayRecognitionError = CustomDisplayRecognitionError;
}*/

//------------------------------------------------------------------------------------------------

identifier returns [const char* str = NULL] : Identifier { str = (const char*)$Identifier.text->chars; };
functionPath returns [const char* str = NULL] : FileSpecLiteral { str = (const char*)$FileSpecLiteral.text->chars; };

rnode returns [NodeId node = ~0u]
	: ^(FUNCTION_CALL ^(FUNCTION_PATH f=functionPath) { char* s = StripAngleBrackets(f); $node = RNode_Register(ctx, s); free(s); Node_Push(ctx, $node); } scopedConnection*)
	{
		Node_Pop(ctx);
	}
	| nid=identifier { $node = RNode_Find(ctx, nid); }
	;

lnode returns [NodeId node = ~0u]
	: nid=identifier { $node = LNode_Find(ctx, nid); }
	;

scopedConnection returns [ConnectionId connection = ~0u]
	: ^(SCOPED_CONNECTION l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, Node_GetActive(ctx), l);
		$connection = Connection_Register(ctx, left, r);
	}
	;

connection returns [ConnectionId connection = ~0u]
	: ^(CONNECTION n=lnode l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, n, l);
		$connection = Connection_Register(ctx, left, r);
	}
	;

rconnection returns [ConnectorId connector = ~0u]
	: ^((RCONNECTION_UNIQUE|RCONNECTION_REF) n0=rnode c0=identifier)
	{
		connector = Connector_Register(ctx, n0, c0);
	}
	| ^(LITERAL StringLiteral)
	{
		char* s = StripQuotesBrackets((const char*)$StringLiteral.text->chars);
		connector = LiteralConnector_Register(ctx, s);
		free(s);
	}
	;

toplevel
	: ^(NODE_DECL nid=identifier n=rnode)
	{
		Node_Name(ctx, n, nid);
	}
	| ^(SLOT_DECL n1id=identifier ^(FUNCTION_PATH f=functionPath))
	{
		char* s = StripAngleBrackets(f);
		NodeId lnode = LNode_Register(ctx, s);
		NodeId rnode = SlotParams_Register(ctx, s);
		free(s);
		Slot_Name(ctx, lnode, rnode, n1id);
	}
	| connection
	| ^(EXPORT Identifier)
	;

entrypoint : toplevel* ;
	
