from Facebook.Yoga import YogaValue
from Facebook.Yoga import YogaAlign
from Sce.Atf.VectorMath import Vec2F
from Sce.Atf.VectorMath import Vec3F
from Sce.Atf.VectorMath import Vec4F

def ToggleFloatValue(storage, label, x):
	if not x:
		storage.RemoveValue(label)
	else:
		storage.SetFloat(label, storage.GetFloat(label))

class Freeze2(object):

	@staticmethod
	def Declare(b):
		b.Float("sheen_speed", 3)
		b.Float("freezeAmt", .55)

	@staticmethod
	def Layout(gui, storage):

		with gui.BeginHiddenHorizontal() as group:
			gui.Checkbox(
				"##SheenSpeedEnable",
				lambda:storage.HasValue("sheen_speed"),
				lambda x:ToggleFloatValue(storage, "sheen_speed", x))
			if storage.HasValue("sheen_speed"):
				gui.BoundedFloat(
					"Sheen speed", 0, 10,
					lambda:storage.GetFloat("sheen_speed"),
					lambda x:storage.SetFloat("sheen_speed", x)).Node.FlexGrow = 1
			else:
				gui.Label("Sheen Speed <no value>").Node.AlignSelf = YogaAlign.Center
		gui.EndHiddenHorizontal()

		with gui.BeginHiddenHorizontal() as group:
			gui.Checkbox(
				"##FreezeAmountEnable",
				lambda:storage.HasValue("freezeAmt"),
				lambda x:ToggleFloatValue(storage, "freezeAmt", x))
			if storage.HasValue("freezeAmt"):
				gui.BoundedFloat(
					"Freeze Amount", 0, 1,
					lambda:storage.GetFloat("freezeAmt"),
					lambda x:storage.SetFloat("freezeAmt", x)).Node.FlexGrow = 1
			else:
				gui.Label("Freeze Amount <no value>").Node.AlignSelf = YogaAlign.Center
		gui.EndHiddenHorizontal()

schemaService.RegisterBlock("Freeze2", Freeze2)
