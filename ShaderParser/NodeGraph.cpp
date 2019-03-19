// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "NodeGraph.h"

namespace GraphLanguage 
{
	const std::string s_resultName = "result";
	const std::string ParameterName_NodeInstantiation = "<instantiation>";

	NodeGraph::NodeGraph() {}
    NodeGraph::~NodeGraph() {}

    void NodeGraph::Add(Node&& a) { _nodes.emplace_back(std::move(a)); }
    void NodeGraph::Add(Connection&& a) { _connections.emplace_back(std::move(a)); }

    bool NodeGraph::IsUpstream(NodeId startNode, NodeId searchingForNode)
    {
            //  Starting at 'startNode', search upstream and see if we find 'searchingForNode'
        if (startNode == searchingForNode) {
            return true;
        }

        for (auto i=_connections.cbegin(); i!=_connections.cend(); ++i) {
            if (i->OutputNodeId() == startNode) {
				auto inputNode = i->InputNodeId();
                if (inputNode != NodeId_Interface && inputNode != NodeId_Constant && IsUpstream(i->InputNodeId(), searchingForNode)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool NodeGraph::IsDownstream(
        NodeId startNode,
        const NodeId* searchingForNodesStart, const NodeId* searchingForNodesEnd)
    {
        if (std::find(searchingForNodesStart, searchingForNodesEnd, startNode) != searchingForNodesEnd) {
            return true;
        }

        for (auto i=_connections.cbegin(); i!=_connections.cend(); ++i) {
            if (i->InputNodeId() == startNode) {
				auto outputNode = i->OutputNodeId();
                if (outputNode != NodeId_Interface && outputNode != NodeId_Constant && IsDownstream(outputNode, searchingForNodesStart, searchingForNodesEnd)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool            NodeGraph::HasNode(NodeId nodeId)
    {
		// Special case node ids are considered to always exist (particularly required when called from Trim())
		if (nodeId == NodeId_Interface || nodeId == NodeId_Constant) return true;
        return std::find_if(_nodes.begin(), _nodes.end(),
            [=](const Node& node) { return node.NodeId() == nodeId; }) != _nodes.end();
    }

    const Node*     NodeGraph::GetNode(NodeId nodeId) const
    {
        auto res = std::find_if(
            _nodes.cbegin(), _nodes.cend(),
            [=](const Node& n) { return n.NodeId() == nodeId; });
        if (res != _nodes.cend()) {
            return &*res;
        }
        return nullptr;
    }

    void NodeGraph::Trim(NodeId previewNode)
    {
        Trim(&previewNode, &previewNode+1);
    }

    void NodeGraph::Trim(const NodeId* trimNodesBegin, const NodeId* trimNodesEnd)
    {
            //
            //      Trim out all of the nodes that are upstream of
            //      'previewNode' (except for output nodes that are
            //      directly written by one of the trim nodes)
            //
            //      Simply
            //          1.  remove all nodes, unless they are downstream
            //              of 'previewNode'
            //          2.  remove all connections that refer to nodes
            //              that no longer exist
            //
            //      Generally, there won't be an output connection attached
            //      to the previewNode at the end of the process. So, we
            //      may need to create one.
            //

        _nodes.erase(
            std::remove_if(
                _nodes.begin(), _nodes.end(),
                [=](const Node& node) { return !IsDownstream(node.NodeId(), trimNodesBegin, trimNodesEnd); }),
            _nodes.end());

        _connections.erase(
            std::remove_if(
                _connections.begin(), _connections.end(),
                [=](const Connection& connection)
                    { return !HasNode(connection.InputNodeId()) || !HasNode(connection.OutputNodeId()) || connection.OutputNodeId() == NodeId_Interface; }),
            _connections.end());
    }

///////////////////////////////////////////////////////////////////////////////////////////////////

	static void OrderNodes(IteratorRange<NodeId*> range)
	{
		// We need to sort the upstreams in some way that maintains a
		// consistant ordering. The simplied way is just to use node id.
		// However we may sometimes want to set priorities for nodes.
		// For example, nodes that can "discard" pixels should be priortized
		// higher.
		std::sort(range.begin(), range.end());
	}

    static bool SortNodesFunction(
        NodeId                  node,
        std::vector<NodeId>&    presorted,
        std::vector<NodeId>&    sorted,
        std::vector<NodeId>&    marks,
        const NodeGraph&        graph)
    {
        if (std::find(presorted.begin(), presorted.end(), node) == presorted.end()) {
            return false;   // hit a cycle
        }
        if (std::find(marks.begin(), marks.end(), node) != marks.end()) {
            return false;   // hit a cycle
        }

        marks.push_back(node);

		std::vector<NodeId> upstream;
		upstream.reserve(graph.GetConnections().size());
        for (const auto& i:graph.GetConnections())
            if (i.OutputNodeId() == node)
				upstream.push_back(i.InputNodeId());

		OrderNodes(MakeIteratorRange(upstream));
		for (const auto& i2:upstream)
			SortNodesFunction(i2, presorted, sorted, marks, graph);

        sorted.push_back(node);
        presorted.erase(std::find(presorted.begin(), presorted.end(), node));
        return true;
    }

	std::vector<NodeId> SortNodes(const NodeGraph& graph, bool& isAcyclic)
	{
            /*

                We need to create a directed acyclic graph from the nodes in 'graph'
                    -- and then we need to do a topological sort.

                This will tell us the order in which to call each function

                Basic algorithms:

                    L <- Empty list that will contain the sorted elements
                    S <- Set of all nodes with no incoming edges
                    while S is non-empty do
                        remove a node n from S
                        add n to tail of L
                        for each node m with an edge e from n to m do
                            remove edge e from the graph
                            if m has no other incoming edges then
                                insert m into S
                    if graph has edges then
                        return error (graph has at least one cycle)
                    else
                        return L (a topologically sorted order)

                Depth first sort:

                    L <- Empty list that will contain the sorted nodes
                    while there are unmarked nodes do
                        select an unmarked node n
                        visit(n)
                    function visit(node n)
                        if n has a temporary mark then stop (not a DAG)
                        if n is not marked (i.e. has not been visited yet) then
                            mark n temporarily
                            for each node m with an edge from n to m do
                                visit(m)
                            mark n permanently
                            add n to head of L

            */

        std::vector<NodeId> presortedNodes, sortedNodes;
        sortedNodes.reserve(graph.GetNodes().size());

        for (const auto& i:graph.GetNodes())
            presortedNodes.push_back(i.NodeId());

		OrderNodes(MakeIteratorRange(presortedNodes));

        isAcyclic = true;
		while (!presortedNodes.empty()) {
            std::vector<NodeId> temporaryMarks;
            bool sortReturn = SortNodesFunction(
                presortedNodes[0],
                presortedNodes, sortedNodes,
                temporaryMarks, graph);

            if (!sortReturn) {
                isAcyclic = false;
                break;
            }
        }

		return sortedNodes;
	}
	
}

