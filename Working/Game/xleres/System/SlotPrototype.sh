// CompoundDocument:1

/* <<Chunk:GraphSyntax:SlotPrototype>>--(

slot s0 implements <SlotPrototype2.sh:Signal_SomeSignal>;
node n0 = <SlotPrototype2.sh:MakeParam1>(o : <SlotPrototype2.sh:MakeParam>().result, t : "<two>");
s0.result : <SlotPrototype2.sh:SomeFunction>(
	position : s0.position, texCoord : n0.result, normal : s0.normal
	).result;

export s0;

)-- */
