package xle

import org.gradle.api.DefaultTask
import org.gradle.api.tasks.incremental.IncrementalTaskInputs
import org.gradle.api.tasks.*

///////////////////////////////////////////////////////////////////////////////////////////////////

@groovy.transform.InheritConstructors
class TextureProcessError extends java.lang.Exception
{}

///////////////////////////////////////////////////////////////////////////////////////////////////

class TextureCompress extends DefaultTask
{
    @InputFile
    File input

    @OutputFile
    File getOutputFile() { return new File(asDestinationFileName(input)) }

    static Closure makeIntermediateName = null;

    String asDestinationFileName(File i) {
        Object intName
        if (makeIntermediateName != null) intName = makeIntermediateName(i)
        else intName = i.getPath()
        return intName.substring(0, intName.lastIndexOf('.')) + ".dds";
    }

    @Input
    String compressionMode = "bc1";

    @Input
    String mipFilter = "kaiser"

    @Input
    String wrapping = "wrap"

    @TaskAction
    void execute(IncrementalTaskInputs inputs)
    {
        println inputs.incremental ? "CHANGED inputs considered out of date"
                                   : "ALL inputs considered out of date"

        inputs.outOfDate { change ->
            def output = asDestinationFileName(change.file)
            def input = change.file.getAbsolutePath()
            def exe = "nvcompress -nocuda -silent -$compressionMode -$wrapping -mipfilter $mipFilter ${input} ${output}"

            // println "out of date: ${input}"
            // println "writing to ${output}"
            // println exe

            def process = exe.execute()
            process.waitFor()

            // Check the output from the error stream, and throw an exception
            // if we get something...
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
                throw new java.lang.Exception("${builder.toString()} << when processing ${input} to output file ${output} >>")
            }
        }

        inputs.removed { change ->
            println "removed: ${change.file.name}"
            new File(asDestinationFileName(change.file)).delete()
        }
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

// 8bpp RGB or RGBA
// high precision new format
class BC7 extends TextureCompress { BC7() { compressionMode = "bc7" } }

class Lumi extends TextureCompress { Lumi() { compressionMode = "lumi" } }
