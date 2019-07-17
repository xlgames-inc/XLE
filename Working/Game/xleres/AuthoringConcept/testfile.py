
from Facebook.Yoga import YogaValue
from Sce.Atf.VectorMath import Vec2F
from Sce.Atf.VectorMath import Vec3F
from Sce.Atf.VectorMath import Vec4F

class Emitter(object):

	@staticmethod
	def Declare(b):
		b.Bool("Checkbox0", False).NativeProperty = "Checkbox0"
		b.Int("cmbo-label2", 2).NativeProperty = "cmbo-label2"
		b.Int("some-slider", 3).NativeProperty = "some-slider"

	@staticmethod
	def Layout(gui, storage):

		###################################################################################################

		checkboxNode = gui.Checkbox(
			"Some Text",
			lambda: storage.GetBool("Checkbox0"),
			lambda x: storage.SetBool("Checkbox0", x))
		checkboxNode.Node.Margin = YogaValue.Point(10)

		###################################################################################################

		labelNode = gui.Label("XXXXXXXXX ------ Some Label ----- XXXXXXX")
		labelNode.Node.Margin = YogaValue.Point(10)

		###################################################################################################

		if False:
			container = gui.BeginCollapsingContainer("SomeContainer")
			container.Node.Margin = YogaValue.Point(5)
			if container.IsOpen:
				label = gui.Label("Some internal node")
				label.Node.Margin = YogaValue.Point(20)
				gui.Label("Another internal node")
				gui.Label("yet Another internal node")
			gui.EndCollapsingContainer()

		###################################################################################################

		if False:
			hoveringContainer = gui.BeginHoveringContainer()
			if True:
				items = ["First", "Second", "Third", "Forth"]
				for i in items:
					gui.Label(i).Node.Margin = YogaValue.Point(5)

				container2 = gui.BeginCollapsingContainer("InternalContainer")
				if container2.IsOpen:
					label = gui.Label("internal-internal-node")
					label.Node.Margin = YogaValue.Point(20)
				gui.EndCollapsingContainer()
			gui.EndHoveringContainer()

			def HoveringContainerRootFrame(n):
				n.X = YogaValue.Point(20)
				n.Y = YogaValue.Point(20)
				n.MinWidth = YogaValue.Point(200)
			hoveringContainer.RootFrame = HoveringContainerRootFrame

		###################################################################################################

		container = gui.BeginCollapsingContainer("SomeContainer")
		container.Node.Margin = YogaValue.Point(5)
		if container.IsOpen:

			if False:
				if gui.BeginComboBox("cmbo-label", lambda:"combo content").IsOpen:
					items = ["First", "Second", "Third", "Forth"]
					for i in items:
						gui.Label(i).Node.Margin = YogaValue.Point(5)
					gui.EndComboBox()

			items2 = ["First2", "Second2", "Third2", "Forth2"]
			gui.ComboBox("cmbo-label2", items2, 
				lambda:storage.GetInt("cmbo-label2"),
				lambda x:storage.SetInt("cmbo-label2", x))

			gui.ScalarSlider(
				"some-slider", 
				lambda:storage.GetInt("some-slider"),
				lambda x:storage.SetInt("some-slider", x),
				1, 10)
				
		gui.EndCollapsingContainer()

def SetFloat2(storage, label, newValue, ele):
	oldValue = storage.GetFloat2(label)
	if ele==1:
		return storage.SetFloat2(label, Vec2F(oldValue.X, newValue))
	else:
		return storage.SetFloat2(label, Vec2F(newValue, oldValue.Y))

class Shape(object):

	@staticmethod
	def Declare(b):
		# note -- capitalisation is incorrect here. level_editor.xsd beings with a lower case
		# letter for attribute name, and upper case for native property name
		b.Int("Shape", 0).NativeProperty = "shape"
		b.Int("SpheroidSegment", 10).NativeProperty = "spheroidSegment"
		b.Float("SpheroidArc", 360).NativeProperty = "spheroidArc"
		b.Float("SphereCut", 1).NativeProperty = "sphereCut"
		b.Float("CylinderLength", 1).NativeProperty = "cylinderLength"
		b.Float("CylinderR", 1).NativeProperty = "cylinderR"
		b.Bool("BillboardsExpanded", True).NativeProperty = "billboardsExpanded"
		b.Int("BillboardType", 1).NativeProperty = "billboardType"
		b.Float("StretchFactor", 0).NativeProperty = "stretchFactor"
		b.Bool("StretchYOnly", False).NativeProperty = "stretchYOnly"
		b.Bool("StretchFaceDirection", False).NativeProperty = "stretchFaceDirection"
		b.Bool("StretchFaceCamera", False).NativeProperty = "stretchFaceCamera"

		b.Float2("SpriteRotation", Vec2F(0, 0)).NativeProperty = "spriteRotation"
		b.Float2("SpriteRotationSpeed", Vec2F(0, 0)).NativeProperty = "spriteRotationSpeed"
		b.Float3("NbRotation", Vec3F(0, 0, 0)).NativeProperty = "nbRotation"
		b.Float3("NbRotationInitialSpeed", Vec3F(0, 0, 0)).NativeProperty = "nbRotationInitialSpeed"
		b.Float3("NbRotationFinalSpeed", Vec3F(0, 0, 0)).NativeProperty = "nbRotationFinalSpeed"

	@staticmethod
	def NBRotationEditor(gui, storage, ele):
		names = ["X", "Y", "Z"]
		gui.BeginGroup(names[ele]).Node.FlexGrow = 1
		gui.Label("S: " + storage.GetFloat3("NbRotationInitialSpeed")[ele].ToString())
		gui.Label("E: " + storage.GetFloat3("NbRotationFinalSpeed")[ele].ToString())
		gui.Label("D: " + storage.GetFloat3("NbRotation")[ele].ToString())
		gui.EndGroup()

	@staticmethod
	def Layout(gui, storage):
		with gui.BeginGroup("Shape settings") as group:

			items2 = ["Quads", "Spheres", "Cylinders"]
			gui.ComboBox("Primitive", items2, 
				lambda:storage.GetInt("Shape"),
				lambda x:storage.SetInt("Shape", x))

			if storage.GetInt("Shape") == 1:		# Spheres
				gui.BoundedIntLog(
					"Segment Count", 1, 32,
					lambda:storage.GetInt("SpheroidSegment"),
					lambda x:storage.SetInt("SpheroidSegment", x))
				gui.BoundedFloat(
					"Arc", 0, 360,
					lambda:storage.GetFloat("SpheroidArc"),
					lambda x:storage.SetFloat("SpheroidArc", x))
				gui.Float(
					"Sphere Cut",
					lambda:storage.GetFloat("SphereCut"),
					lambda x:storage.SetFloat("SphereCut", x))

			if storage.GetInt("Shape") == 2:		# Cylinders
				gui.BoundedIntLog(
					"Segment Count", 1, 32,
					lambda:storage.GetInt("SpheroidSegment"),
					lambda x:storage.SetInt("SpheroidSegment", x))
				gui.BoundedFloat(
					"Arc", 0, 360,
					lambda:storage.GetFloat("SpheroidArc"),
					lambda x:storage.SetFloat("SpheroidArc", x))
				gui.Float(
					"Length",
					lambda:storage.GetFloat("CylinderLength"),
					lambda x:storage.SetFloat("CylinderLength", x))
				gui.BoundedFloat(
					"Cone Radius", 0, 3,
					lambda:storage.GetFloat("CylinderR"),
					lambda x:storage.SetFloat("CylinderR", x))

		gui.EndGroup()

		with gui.BeginHiddenHorizontal() as group:

			with gui.BeginGroup("Billboards") as group:
				group.Node.FlexGrow = 1
				gui.Checkbox(
					"Particles are sprites",
					lambda:storage.GetBool("BillboardsExpanded"),
					lambda x:storage.SetBool("BillboardsExpanded", x))
				gui.Checkbox(
					"Directional billboards",
					lambda:(storage.GetInt("BillboardType") == 2) or (storage.GetInt("BillboardType") == 3),
					lambda x:storage.SetInt("BillboardType", 2 if x else 1))

				if (storage.GetInt("BillboardType") == 2) or (storage.GetInt("BillboardType") == 3):
					with gui.BeginHiddenHorizontal() as group:
						gui.Checkbox(
							"X",
							lambda:(storage.GetInt("BillboardType") == 2),
							lambda x:storage.SetInt("BillboardType", 2)).Node.FlexGrow=1
						gui.Checkbox(
							"Y",
							lambda:(storage.GetInt("BillboardType") == 3),
							lambda x:storage.SetInt("BillboardType", 3)).Node.FlexGrow=1
					gui.EndHiddenHorizontal()
			gui.EndGroup()

			with gui.BeginGroup("Stretch") as group:
				group.Node.FlexGrow = 1
				with gui.BeginHiddenHorizontal() as group:
					gui.Float(
						"Value",
						lambda:storage.GetFloat("StretchFactor"),
						lambda x:storage.SetFloat("StretchFactor", x))
					gui.Checkbox(
						"Y-Only",
						lambda:storage.GetBool("StretchYOnly"),
						lambda x:storage.SetBool("StretchYOnly", x))
				gui.EndHiddenHorizontal()

				gui.Checkbox(
					"Rotate to face direction",
					lambda:storage.GetBool("StretchFaceDirection"),
					lambda x:storage.SetBool("StretchFaceDirection", x))
				if storage.GetBool("StretchFaceDirection"):
					gui.Checkbox(
						"Face camera",
						lambda:storage.GetBool("StretchFaceCamera"),
						lambda x:storage.SetBool("StretchFaceCamera", x))

			gui.EndGroup()

		gui.EndHiddenHorizontal()

		if gui.BeginCollapsingContainer("Sprite rotation").IsOpen:
			gui.BoundedFloat(
				"Initial deg", 0, 360,
				lambda:storage.GetFloat2("SpriteRotation").X,
				lambda newValue:SetFloat2(storage, "SpriteRotation", newValue, 0))
			gui.BoundedFloat(
				"delta deg", 0, 360,
				lambda:storage.GetFloat2("SpriteRotation").Y,
				lambda newValue:SetFloat2(storage, "SpriteRotation", newValue, 1))
			gui.BoundedFloat(
				"deg/s", 0, 360,
				lambda:storage.GetFloat2("SpriteRotationSpeed").X,
				lambda newValue:SetFloat2(storage, "SpriteRotationSpeed", newValue, 0))
			gui.BoundedFloat(
				"delta deg/s", 0, 360,
				lambda:storage.GetFloat2("SpriteRotationSpeed").Y,
				lambda newValue:SetFloat2(storage, "SpriteRotationSpeed", newValue, 1))
		gui.EndCollapsingContainer()

		if gui.BeginCollapsingContainer("Non billboard rotation").IsOpen:
			with gui.BeginHiddenHorizontal() as group:
				Shape.NBRotationEditor(gui, storage, 0)
				Shape.NBRotationEditor(gui, storage, 1)
				Shape.NBRotationEditor(gui, storage, 2)
			gui.EndHiddenHorizontal()
		gui.EndCollapsingContainer()


schemaService.RegisterBlock("Emitter", Emitter)
schemaService.RegisterBlock("Shape", Shape)
