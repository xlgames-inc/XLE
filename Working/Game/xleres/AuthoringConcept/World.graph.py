from Facebook.Yoga import YogaValue
from Sce.Atf.VectorMath import Vec2F
from Sce.Atf.VectorMath import Vec3F
from Sce.Atf.VectorMath import Vec4F

class CloudShadows(object):

	@staticmethod
	def Declare(b):
		b.Float("UVFreq", 1)
		b.Float("UVFreq2", 1)
		b.Float("hgrid", 1)

	@staticmethod
	def Layout(gui, storage):
		gui.BoundedFloat(
			"Mapping Scale 1", 0, 10,
			lambda:storage.GetFloat("UVFreq"),
			lambda x:storage.SetFloat("UVFreq", x))

		gui.BoundedFloat(
			"Mapping Scale 2", 0, 10,
			lambda:storage.GetFloat("UVFreq2"),
			lambda x:storage.SetFloat("UVFreq2", x))

		gui.BoundedFloat(
			"Noisiness", 0, 2,
			lambda:storage.GetFloat("hgrid"),
			lambda x:storage.SetFloat("hgrid", x))

schemaService.RegisterBlock("CloudShadows", CloudShadows)
