// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.ComponentModel.Composition.Hosting;
using System.Windows.Forms;

namespace NodeEditor
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main()
		{
            var engine = new GUILayer.EngineDevice();
            GC.KeepAlive(engine);

            var catalog = new TypeCatalog(
                typeof(ShaderPatcherLayer.Manager),
                typeof(ShaderFragmentArchive.Archive),
                typeof(NodeEditorCore.ShaderFragmentArchiveModel),
                typeof(NodeEditorCore.ModelConversion),
                typeof(NodeEditorCore.ShaderFragmentNodeCreator),
                typeof(NodeEditorCore.DiagramDocument),
                typeof(ExampleForm)
            );

            using (var container = new CompositionContainer(catalog))
            {
                container.ComposeExportedValue<ExportProvider>(container);
                container.ComposeExportedValue<CompositionContainer>(container);

                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(container.GetExport<ExampleForm>().Value);
            }

            engine.Dispose();
		}
	}
}
