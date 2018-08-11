// CompoundDocument:1
#include "xleres/System/Prefix.h"

#include "xleres/Nodes/Basic.sh"
#include "xleres/Objects/Example/ProcWood/WoodRings002.sh"
#include "xleres/Objects/Example/ProcWood/WoodPlanks.sh"
#include "xleres/Nodes/Color.sh"
#include "xleres/Objects/Example/ProcWood/WoodGnarlGrainStain.sh"


void Wood_04_G(float3 coords, out float3 color : SV_Target0)
{
	float3 Output_39_result;
	Output_39_result = RGB( 1, .606, .284 );

	float3 Output_40_result;
	Output_40_result = RGB( .307, .122, .042 );

	float3 Output_37_plankCoords;
	WoodPlanks( 2, .5, .15, 5, .4, .333, .1, coords, Output_37_plankCoords );

	float Output_43_grain;
	float3 Output_43_gnarledCoords;
	WoodGnarlGrainStain( .2, 4, 4, Output_37_plankCoords, 100, .5, Output_43_grain, Output_43_gnarledCoords );

	float3 Output_38_result;
	Output_38_result = Mix3( Output_40_result, Output_39_result, Output_43_grain );

	float3 Output_41_result;
	Output_41_result = RGB( .073, .011, 0 );

	float3 Output_42_result;
	Output_42_result = Mix3( Output_41_result, Output_40_result, Output_43_grain );

	float Output_44_rings;
	WoodRings002( Output_43_gnarledCoords, 50, .5, 2.5, Output_44_rings );

	float3 Output_36_result;
	Output_36_result = Mix3( Output_42_result, Output_38_result, Output_44_rings );

	color = Output_36_result;

}
/* <<Chunk:NodeGraph:Wood_04_G>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraph xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<ConstantConnections>
		<ConstantConnection>
			<OutputNodeID>44</OutputNodeID>
			<OutputParameterName>strength</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>44</OutputNodeID>
			<OutputParameterName>density</OutputParameterName>
			<Value>50</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>44</OutputNodeID>
			<OutputParameterName>shape</OutputParameterName>
			<Value>2.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>offcenter</OutputParameterName>
			<Value>5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>width</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>height</OutputParameterName>
			<Value>.15</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>shift1</OutputParameterName>
			<Value>.4</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>shift2</OutputParameterName>
			<Value>.1</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>shift3</OutputParameterName>
			<Value>.333</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>37</OutputNodeID>
			<OutputParameterName>length</OutputParameterName>
			<Value>2</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>39</OutputNodeID>
			<OutputParameterName>r</OutputParameterName>
			<Value>1</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>39</OutputNodeID>
			<OutputParameterName>g</OutputParameterName>
			<Value>.606</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>39</OutputNodeID>
			<OutputParameterName>b</OutputParameterName>
			<Value>.284</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>40</OutputNodeID>
			<OutputParameterName>r</OutputParameterName>
			<Value>.307</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>40</OutputNodeID>
			<OutputParameterName>g</OutputParameterName>
			<Value>.122</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>40</OutputNodeID>
			<OutputParameterName>b</OutputParameterName>
			<Value>.042</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>41</OutputNodeID>
			<OutputParameterName>r</OutputParameterName>
			<Value>.073</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>41</OutputNodeID>
			<OutputParameterName>g</OutputParameterName>
			<Value>.011</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>41</OutputNodeID>
			<OutputParameterName>b</OutputParameterName>
			<Value>0</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>gnarlStrength</OutputParameterName>
			<Value>.2</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>stain</OutputParameterName>
			<Value>4</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>gnarlDensity</OutputParameterName>
			<Value>.5</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>grainStrength</OutputParameterName>
			<Value>4</Value>
		</ConstantConnection>
		<ConstantConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>grainDensity</OutputParameterName>
			<Value>100</Value>
		</ConstantConnection>
	</ConstantConnections>
	<InputParameterConnections />
	<NodeConnections>
		<NodeConnection>
			<OutputNodeID>36</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>44</InputNodeID>
			<InputParameterName>rings</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>43</OutputNodeID>
			<OutputParameterName>coords</OutputParameterName>
			<InputNodeID>37</InputNodeID>
			<InputParameterName>plankCoords</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>36</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>38</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>38</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>39</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>42</OutputNodeID>
			<OutputParameterName>rhs</OutputParameterName>
			<InputNodeID>40</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>38</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>40</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>42</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>41</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>36</OutputNodeID>
			<OutputParameterName>lhs</OutputParameterName>
			<InputNodeID>42</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>42</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>43</InputNodeID>
			<InputParameterName>grain</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>44</OutputNodeID>
			<OutputParameterName>coords</OutputParameterName>
			<InputNodeID>43</InputNodeID>
			<InputParameterName>gnarledCoords</InputParameterName>
			<InputType>float3</InputType>
			<OutputType>float3</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
		<NodeConnection>
			<OutputNodeID>38</OutputNodeID>
			<OutputParameterName>factor</OutputParameterName>
			<InputNodeID>43</InputNodeID>
			<InputParameterName>grain</InputParameterName>
			<InputType>float</InputType>
			<OutputType>float</OutputType>
			<Semantic i:nil="true" />
		</NodeConnection>
	</NodeConnections>
	<Nodes>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Mix3</FragmentArchiveName>
			<NodeId>36</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>0</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Objects/Example/ProcWood\WoodRings002.sh:WoodRings002</FragmentArchiveName>
			<NodeId>44</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>2</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Objects/Example/ProcWood\WoodPlanks.sh:WoodPlanks</FragmentArchiveName>
			<NodeId>37</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>3</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Mix3</FragmentArchiveName>
			<NodeId>38</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>4</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes\Color.sh:RGB</FragmentArchiveName>
			<NodeId>39</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>5</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes\Color.sh:RGB</FragmentArchiveName>
			<NodeId>40</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>6</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes\Color.sh:RGB</FragmentArchiveName>
			<NodeId>41</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>7</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Nodes/Basic.sh:Mix3</FragmentArchiveName>
			<NodeId>42</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>8</VisualNodeId>
		</Node>
		<Node>
			<FragmentArchiveName>xleres/Objects/Example/ProcWood\WoodGnarlGrainStain.sh:WoodGnarlGrainStain</FragmentArchiveName>
			<NodeId>43</NodeId>
			<NodeType>Procedure</NodeType>
			<VisualNodeId>9</VisualNodeId>
		</Node>
	</Nodes>
	<OutputParameterConnections>
		<OutputParameterConnection>
			<InputNodeID>36</InputNodeID>
			<InputParameterName>result</InputParameterName>
			<Name>color</Name>
			<Semantic></Semantic>
			<Type>float3</Type>
			<VisualNodeId>1</VisualNodeId>
		</OutputParameterConnection>
	</OutputParameterConnections>
	<PreviewSettingsObjects>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>0</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>2</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>3</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>4</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>5</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>6</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>7</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Sphere</Geometry>
			<OutputToVisualize></OutputToVisualize>
			<VisualNodeId>8</VisualNodeId>
		</PreviewSettings>
		<PreviewSettings>
			<Geometry>Plane2D</Geometry>
			<OutputToVisualize>result</OutputToVisualize>
			<VisualNodeId>9</VisualNodeId>
		</PreviewSettings>
	</PreviewSettingsObjects>
	<VisualNodes>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1975</d4p1:x>
				<d4p1:y>551</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>2613</d4p1:x>
				<d4p1:y>682</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1447</d4p1:x>
				<d4p1:y>-61</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>310</d4p1:x>
				<d4p1:y>364</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1515</d4p1:x>
				<d4p1:y>783</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>923</d4p1:x>
				<d4p1:y>815</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>928</d4p1:x>
				<d4p1:y>753</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>936</d4p1:x>
				<d4p1:y>699</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>1511</d4p1:x>
				<d4p1:y>682</d4p1:y>
			</Location>
			<State>Collapsed</State>
		</VisualNode>
		<VisualNode>
			<Location xmlns:d4p1="http://schemas.datacontract.org/2004/07/System.Drawing">
				<d4p1:x>905</d4p1:x>
				<d4p1:y>161</d4p1:y>
			</Location>
			<State>Normal</State>
		</VisualNode>
	</VisualNodes>
</NodeGraph>
)-- */
/* <<Chunk:NodeGraphContext:Wood_04_G>>--(
<?xml version="1.0" encoding="utf-8"?>
<NodeGraphContext xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://schemas.datacontract.org/2004/07/ShaderPatcherLayer">
	<HasTechniqueConfig>false</HasTechniqueConfig>
	<ShaderParameters xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
	<Variables xmlns:d2p1="http://schemas.microsoft.com/2003/10/Serialization/Arrays" />
</NodeGraphContext>
)-- */
