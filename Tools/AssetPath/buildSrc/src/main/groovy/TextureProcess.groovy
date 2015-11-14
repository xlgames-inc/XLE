
package xle

import org.gradle.api.DefaultTask
import org.gradle.api.tasks.TaskAction
import org.gradle.api.tasks.incremental.IncrementalTaskInputs
import org.gradle.api.tasks.InputFile
import org.gradle.api.tasks.OutputFile

class CompressTexture extends DefaultTask
{
    @InputFile
    File input

    @OutputFile
    File getOutputFile() { return new File(asDestinationFileName(input)) }

    String asDestinationFileName(File i) { i.getAbsolutePath() + "_processed.dds" }

    @TaskAction
    void execute(IncrementalTaskInputs inputs)
    {
        println inputs.incremental ? "CHANGED inputs considered out of date"
                                   : "ALL inputs considered out of date"

        inputs.outOfDate { change ->
            println "out of date: ${change.file.name}"
            println "writing to ${asDestinationFileName(change.file)}"
            def exe = "nvcompress -bc1 -nocuda -wrap -mipfilter kaiser ${change.file.getAbsolutePath()} ${asDestinationFileName(change.file)}"
            println exe
            exe.execute()
        }

        inputs.removed { change ->
            println "removed: ${change.file.name}"
            new File(asDestinationFileName(change.file)).delete()
        }
    }
}
