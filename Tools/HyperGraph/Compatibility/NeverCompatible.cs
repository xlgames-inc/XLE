using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace HyperGraph.Compatibility {
	/// <summary>
	/// Will behave as if all node item connectors aren't compatible.
	/// </summary>
	public class NeverCompatible : ICompatibilityStrategy {
		/// <summary>
		/// Determine if two node item connectors could be connected to each other.
		/// </summary>
		/// <param name="from">From which node connector are we connecting.</param>
		/// <param name="to">To which node connector are we connecting?</param>
		/// <returns><see langword="true"/> if the connection is valid; <see langword="false"/> otherwise</returns>
        public HyperGraph.Compatibility.ConnectionType CanConnect(NodeConnector @from, NodeConnector to)
		{
            return HyperGraph.Compatibility.ConnectionType.Incompatible;
		}
	}
}
