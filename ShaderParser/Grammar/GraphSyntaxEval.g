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
	NodeId Node_Register(const void*, const char file[]);
	ConnectorId Connector_Register(const void*, NodeId node, const char connectorName[]);
	ConnectorId LiteralConnector_Register(const void*, const char literal[]);
	ConnectionId Connection_Register(const void*, ConnectorId left, ConnectorId right);

	void Node_Name(const void*, NodeId, const char name[]);
	NodeId Node_Find(const void*, const char name[]);
	void Node_Push(const void*, NodeId);
	void Node_Pop(const void*);
	NodeId Node_GetActive(const void*);
}

/*@apifuncs
{
	RECOGNIZER->displayRecognitionError = CustomDisplayRecognitionError;
}*/

//------------------------------------------------------------------------------------------------

identifier returns [const char* str = NULL] : Identifier { str = (const char*)$Identifier.text->chars; };
functionPath returns [const char* str = NULL] : FileSpecLiteral { str = (const char*)$FileSpecLiteral.text->chars; };

node returns [NodeId node = ~0u]
	: ^(FUNCTION_CALL ^(FUNCTION_PATH f=functionPath) { $node = Node_Register(ctx, f); Node_Push(ctx, $node); } scopedConnection*)
	{
		Node_Pop(ctx);
	}
	| nid=identifier { $node = Node_Find(ctx, nid); }
	;

scopedConnection returns [ConnectionId connection = ~0u]
	: ^(SCOPED_CONNECTION l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, Node_GetActive(ctx), l);
		$connection = Connection_Register(ctx, left, r);
	}
	;

connection returns [ConnectionId connection = ~0u]
	: ^(CONNECTION n=node l=identifier r=rconnection)
	{
		ConnectorId left = Connector_Register(ctx, n, l);
		$connection = Connection_Register(ctx, left, r);
	}
	;

rconnection returns [ConnectorId connector = ~0u]
	: ^((RCONNECTION_UNIQUE|RCONNECTION_REF) n0=node c0=identifier)
	{
		connector = Connector_Register(ctx, n0, c0);
	}
	| ^(LITERAL StringLiteral)
	{
		connector = LiteralConnector_Register(ctx, (const char*)$StringLiteral.text->chars);
	}
	;

toplevel
	: ^(NODE_DECL nid=identifier n=node)
	{
		Node_Name(ctx, n, nid);
	}
	| connection
	| ^(ENTRYPOINT_ASSIGNMENT Identifier)
	;

entrypoint : toplevel* ;
	
