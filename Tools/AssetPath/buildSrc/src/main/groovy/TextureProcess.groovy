package xle

import org.gradle.api.DefaultTask
import org.gradle.api.tasks.incremental.IncrementalTaskInputs
import org.gradle.api.tasks.*

///////////////////////////////////////////////////////////////////////////////////////////////////

@groovy.transform.InheritConstructors
class TextureProcessError extends java.lang.Exception
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class ProcessStep
{
	String getCommandLine(File input, File output)	{ return null; }
	boolean doCheckErrorStream()					{ return true; }

	boolean isIntermediate = false;

	static Closure makeIntermediateName = null;
    String asDestinationFileName(File i, String newExt, int intermediateIndex)
    {
        Object intName;
        if (makeIntermediateName != null)	intName = makeIntermediateName(i);
        else								intName = i.getPath();

        return intName.substring(0, intName.lastIndexOf('.')) + (isIntermediate?"_${intermediateIndex}":"") + newExt;
    }
}

class TextureTask extends DefaultTask
{
    File input = null
	File output = null

	List<ProcessStep> steps = new ArrayList<ProcessStep>();
	Map<Integer, Integer> stepInputs = new HashMap<Integer, Integer>();

	@InputFiles
	List<File> getInputFiles()
	{
		def result = new ArrayList<File>();
		if (input) result.add(input);
		return result;
	}

    @OutputFiles
    List<File> getOutputFiles()
	{
		def result = new ArrayList<File>();
		for (int c=0; c<steps.size(); ++c) 
			result.add(getOutputFile(c));
		return result;
	}

	File getOutputFile(int index)
	{
		if (index == steps.size()-1 && output) return output;
		return new File(steps[index].asDestinationFileName(input, ".dds", index));
	}

	static void checkProcessErrorStream(Process process, String operation)
	{
		if (process.getErrorStream().available() > 0)
        {
            def reader = new BufferedReader(new InputStreamReader(process.getErrorStream()));
            def builder = new StringBuilder();
            String line = null;
            while ((line = reader.readLine()) != null) {
                builder.append(line);
                builder.append(System.getProperty("line.separator"));
            }

                // Note that using TextureProcessError here causes a JVM exception in my
                // version of the JDK. It's fixed in a newer version; but let's just use
                // a basic exception, anyway
            throw new java.lang.Exception("${builder.toString()} << ${operation} >>")
        }
	}

	@TaskAction
    void run()
	{
			// Run each step one by one... Sometimes we create intermediate steps, which
			// are stored in temporary files

		for (int c=0; c<steps.size(); ++c) {
			def stepInput = input;
			def t = stepInputs[c];
			if (t) { stepInput = getOutputFile(t) }
			else if (c!=0) { stepInput = getOutputFile(c-1) }

			def stepOutput = getOutputFile(c)
			def cmdLine = steps[c].getCommandLine(stepInput, stepOutput)
			println cmdLine

			def process = cmdLine.execute()
			process.waitFor()

			if (steps[c].doCheckErrorStream())
				checkProcessErrorStream(process, "while processing texture from ${stepInput ? stepInput.getAbsolutePath() : "<no-input>"} to output file ${stepOutput.getAbsolutePath()}");
		}
	}

	/*
	@TaskAction
    void execute(IncrementalTaskInputs inputs)
    {
        println inputs.incremental ? "CHANGED inputs considered out of date"
                                   : "ALL inputs considered out of date"

        inputs.outOfDate { change ->
            def output = asDestinationFileName(change.file)
            def input = change.file.getAbsolutePath()
            def exe = getCommandLine(change.file, output);

            // println "out of date: ${input}"
            // println "writing to ${output}"
            println exe

            def process = exe.execute()
            process.waitFor()
            
			checkProcessErrorStream(process, "while processing texture from ${input} to output file ${output}");
        }

        inputs.removed { change ->
            println "removed: ${change.file.name}"
            new File(asDestinationFileName(change.file)).delete()
        }
    }
	*/

}

///////////////////////////////////////////////////////////////////////////////////////////////////

class TextureCompress extends ProcessStep
{
    @Input
    String compressionMode = "bc1";

    @Input
    String mipFilter = "kaiser"

    @Input
    String wrapping = "wrap"

    @Input
    boolean mips = true

	@Input
	boolean isNormalMap = false

	String getCommandLine(File input, File output)
	{
		return "nvcompress -nocuda -silent -$compressionMode -$wrapping ${mips?"-mipfilter $mipFilter":"-nomips"} ${isNormalMap?"-normal":""} ${input.getAbsolutePath()} ${output.getAbsolutePath()}";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class TextureTransformStep extends ProcessStep
{
	String makeCommandLine(File output, String shader, String parameters)
	{
		return "TextureTransform o=${output.getAbsolutePath()}; s=${shader}; ~p; ${parameters}";
	}
}

class EquirectToCube extends TextureTransformStep
{
    @Input
    int faceSize = 512

    @Input
    String format = "R16G16B16A16_FLOAT"

    @Input
    String shader = "main"

    String getCommandLine(File input, File output)
	{
		return makeCommandLine(
			output, 
			"ToolsHelper/EquirectangularToCube.sh:${shader}", 
			"Input=${input.getAbsolutePath()}; Dims={${faceSize*3}, ${faceSize*4}}; Format=${format}");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class GenericTextureGen extends TextureTransformStep
{
    @Input
    String format = "R16G16B16A16_FLOAT"

    @Input
    String shader;

    @Input
    int width = 256;
    int height = 256;

	String getCommandLine(File input, File output)
	{
		return makeCommandLine(
			output, shader,
			"Dims={${width}, ${height}}; Format=${format}");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class CubeMapGen extends ProcessStep
{
	String getCommandLine(File input, File output)
	{
		return "CubeMapGen ${input.getAbsolutePath()} -exit -exportCubeDDS -exportMipChain -edgeFixupWidth:0 -exportPixelFormat:A16B16G16R16F -exportFilename:${output.getAbsolutePath()}";
	}

	boolean doCheckErrorStream() { return false; }
}

class DiffuseCubeMapGen extends ProcessStep
{
    String asDestinationFileName(File i, String newExt, int intermediateIndex)
	{
        def baseName = super.asDestinationFileName(i, newExt, intermediateIndex);
        return baseName.substring(0, baseName.lastIndexOf('.')) + "_diffuse" + newExt;
    }

	String getCommandLine(File input, File output)
	{ 
		return "ModifiedCubeMapGen ${input.getAbsolutePath()} -exit -IrradianceCubemap:180 -exportCubeDDS -exportMipChain -edgeFixupWidth:0 -exportPixelFormat:A16B16G16R16F -exportSize:32 -exportFilename:${output.getAbsolutePath()}";
	}

	boolean doCheckErrorStream() { return false; }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 4bpp RGB default compression (with optional 1 bit alpha)
class BC1 extends TextureTask		{ BC1() { steps.add(new TextureCompress(compressionMode:"bc1")) } }
class BC1a extends TextureTask		{ BC1a() { steps.add(new TextureCompress(compressionMode:"bc1a")) } }

// 8bpp RGBA
// BC2: 4 bits/pixel quantized alpha
// BC3: 4 bits/pixel interpolated alpha
class BC2 extends TextureTask		{ BC2() { steps.add(new TextureCompress(compressionMode:"bc2")) } }
class BC3 extends TextureTask		{ BC3() { steps.add(new TextureCompress(compressionMode:"bc3")) } }

class NormalMap extends TextureTask { NormalMap() { steps.add(new TextureCompress(compressionMode:"bc5", mipFilter:"box", isNormalMap:true)) } }

class BC6 extends TextureTask		{ BC6() { steps.add(new TextureCompress(compressionMode:"bc6")) } }

// 8bpp RGB or RGBA
// high precision new format
class BC7 extends TextureTask		{ BC7() { steps.add(new TextureCompress(compressionMode:"bc7")) } }

class Lumi extends TextureTask		{ Lumi() { steps.add(new TextureCompress(compressionMode:"lumi")) } }

class RGB extends TextureTask		{ RGB() { steps.add(new TextureCompress(compressionMode:"rgb")) } }

///////////////////////////////////////////////////////////////////////////////////////////////////

class EquiRectEnv extends TextureTask
{
	EquiRectEnv()
	{
		steps.add(new TextureCompress(compressionMode:"rgb", mips:false, isIntermediate:true));
		steps.add(new EquirectToCube(isIntermediate:true));
		steps.add(new CubeMapGen());
		steps.add(new DiffuseCubeMapGen());
		stepInputs[3] = 1;
	}
}

class HemiEquiRectEnv extends TextureTask
{
	HemiEquiRectEnv()
	{
		steps.add(new TextureCompress(compressionMode:"rgb", mips:false, isIntermediate:true));
		steps.add(new EquirectToCube(isIntermediate:true, shader:"hemi"));
		steps.add(new CubeMapGen());
		steps.add(new DiffuseCubeMapGen());
		stepInputs[3] = 1;
	}
}
