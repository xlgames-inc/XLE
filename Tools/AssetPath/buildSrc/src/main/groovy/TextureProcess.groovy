package xle

import org.gradle.api.DefaultTask
import org.gradle.api.tasks.incremental.IncrementalTaskInputs
import org.gradle.api.tasks.*

///////////////////////////////////////////////////////////////////////////////////////////////////

@groovy.transform.InheritConstructors
class TextureProcessError extends java.lang.Exception
{}

///////////////////////////////////////////////////////////////////////////////////////////////////

class TextureTask extends DefaultTask
{
    @InputFile
    File input = null
	File output = null

    @OutputFile
    File getOutputFile() 
	{ 
		if (output != null) return output;
		return new File(asDestinationFileName(input)) 
	}

    static Closure makeIntermediateName = null;

    String asDestinationFileName(File i)
    {
        Object intName
        if (TextureTask.makeIntermediateName != null)
            intName = TextureTask.makeIntermediateName(i)
        else intName = i.getPath()
        return intName.substring(0, intName.lastIndexOf('.')) + ".dds";
    }

	static void checkProcessErrorStream(Process process, String operation)
	{
		if (process.getErrorStream().available() > 0)
        {
            BufferedReader reader =
                new BufferedReader(new InputStreamReader(process.getErrorStream()));
            StringBuilder builder = new StringBuilder();
            String line = null;
            while ( (line = reader.readLine()) != null) {
                builder.append(line);
                builder.append(System.getProperty("line.separator"));
            }

                // note that using TextureProcessError here causes a JVM exception in my
                // version of the JDK. It's fixed in a newer version; but let's just use
                // a basic exception, anyway
            throw new java.lang.Exception("${builder.toString()} <<  >>")
        }
	}

	String getCommandLine(File input, File output) { return null; }
	boolean doCheckErrorStream() { return true; }

	@TaskAction
    void run()
	{
		def cmdLine = getCommandLine(input, getOutputFile());
		println cmdLine

        def process = cmdLine.execute()
        process.waitFor()

		if (doCheckErrorStream())
			checkProcessErrorStream(process, "while processing texture from ${input} to output file ${output}");
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

class TextureCompress extends TextureTask
{
    @Input
    String compressionMode = "bc1";

    @Input
    String mipFilter = "kaiser"

    @Input
    String wrapping = "wrap"

    @Input
    boolean mips = true

	String getCommandLine(File input, File output)
	{
		return "nvcompress -nocuda -silent -$compressionMode -$wrapping ${mips?"-mipfilter $mipFilter":"-nomips"} ${input.getAbsolutePath()} ${output.getAbsolutePath()}";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class TextureTransformTask extends TextureTask
{
	String makeCommandLine(File output, String shader, String parameters)
	{
		return "TextureTransform o=${output.getAbsolutePath()}; s=${shader}; ~p; ${parameters}";
	}
}

class EquirectToCube extends TextureTransformTask
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

class GenericTextureGen extends TextureTransformTask
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

class CubeMapGen extends TextureTask
{
	String getCommandLine(File input, File output)
	{
		return "CubeMapGen ${input.getAbsolutePath()} -exit -exportCubeDDS -exportMipChain -edgeFixupWidth:0 -exportPixelFormat:A16B16G16R16F -exportFilename:${output.getAbsolutePath()}";
	}

	boolean doCheckErrorStream() { return false; }
}

class DiffuseCubeMapGen extends TextureTask
{
    String asDestinationFileName(File i) 
	{
        def baseName = super.asDestinationFileName(i);
        return baseName.substring(0, baseName.lastIndexOf('.')) + "_diffuse.dds";
    }

	String getCommandLine(File input, File output)
	{ 
		"ModifiedCubeMapGen ${input.getAbsolutePath()} -exit -IrradianceCubemap:180 -exportCubeDDS -exportMipChain -edgeFixupWidth:0 -exportPixelFormat:A16B16G16R16F -exportSize=32 -exportFilename:${output.getAbsolutePath()}";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// 4bpp RGB default compression (with optional 1 bit alpha)
class BC1 extends TextureCompress { BC1() { compressionMode = "bc1" } }
class BC1a extends TextureCompress { BC1a() { compressionMode = "bc1a" } }

// 8bpp RGBA
// BC2: 4 bits/pixel quantized alpha
// BC4: 4 bits/pixel interpolated alpha
class BC2 extends TextureCompress { BC2() { compressionMode = "bc2" } }
class BC3 extends TextureCompress { BC3() { compressionMode = "bc3" } }

class BC6 extends TextureCompress { BC6() { compressionMode = "bc6" } }

// 8bpp RGB or RGBA
// high precision new format
class BC7 extends TextureCompress { BC7() { compressionMode = "bc7" } }

class Lumi extends TextureCompress { Lumi() { compressionMode = "lumi" } }

class RGB extends TextureCompress { RGB() { compressionMode = "rgb" } }
