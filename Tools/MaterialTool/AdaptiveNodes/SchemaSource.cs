using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;

using Sce.Atf.Dom;
using AuthoringConcept.AdaptiveEditing;
using Aga.Controls.Tree;
using System.Drawing;
using System.IO;
using System.ComponentModel;

namespace MaterialTool
{
    [Export(typeof(PythonAdaptiveSchemaSource))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    internal class PythonAdaptiveSchemaSource : AdaptiveSchemaSource
    {
        public struct ScriptAndState { public PythonSchemaScript Script; public string ExceptionString; }

        public ScriptAndState GetScript(string scriptFile)
        {
            if (PythonSchemaScripts.TryGetValue(scriptFile, out ScriptAndState scriptAndState))
            {
                return scriptAndState;
            }

            ScriptAndState result = new ScriptAndState { Script = null, ExceptionString = null };
            try
            {
                result.Script = new PythonSchemaScript(scriptFile, _scriptingService);
            }
            catch (Exception e)
            {
                result.ExceptionString = e.Message;
            }

            PythonSchemaScripts.Add(scriptFile, result);

            return result;
        }

        private Dictionary<string, ScriptAndState> PythonSchemaScripts = new Dictionary<string, ScriptAndState>();

        protected override IEnumerable<KeyValuePair<string, ISchemaScript>> SchemaScripts
        {
            get
            {
                foreach (var script in PythonSchemaScripts)
                    yield return new KeyValuePair<string, ISchemaScript>(script.Key, script.Value.Script);
            }
        }

        internal class PythonSchemaScript : ISchemaScript
        {
            public PythonSchemaScript(string scriptFile, Sce.Atf.Applications.ScriptingService scriptingService)
            {
                // Generate the datablock definition
                // When we call ExecuteScriptSource, we're expecting the python script to access the global variable "schemaSource"
                // and call the RegisterBlock method to register it's types.
                // In RegisterBlock, we will receive an object we can use for the Declare() and Layout() methods needed to
                // define the type in question
                scriptingService.SetVariable("schemaService", this);
                var compileResult = scriptingService.ExecuteFile(scriptFile);
                scriptingService.RemoveVariable("schemaService");

                if (!string.IsNullOrEmpty(compileResult))
                {
                    throw new InvalidOperationException("Script compile failed with message: " + compileResult);
                }
            }

            public void RegisterBlock(string name, dynamic dataType)      // (note that this must be public to allow the IronPython script to call it)
            {
                var helper = new DataBlockDeclarationHelper();
                dataType.Declare(helper);       // run the "Declare" method on the 

                var decl = new PythonDataBlockDeclaration
                {
                    Type = helper,
                    DefiningType = dataType
                };

                if (DataBlockDeclarations.ContainsKey(name))
                {
                    DataBlockDeclarations[name] = decl;     // reset previously set value
                }
                else
                {
                    DataBlockDeclarations.Add(name, decl);
                }
            }

            private class PythonDataBlockDeclaration : IDataBlockDeclaration
            {
                public DataBlockDeclarationHelper Type { internal set; get; }
                internal dynamic DefiningType;

                public void PerformLayout(AuthoringConcept.ImmediateGUI.Arbiter gui, IDataBlock storage)
                {
                    DefiningType.Layout(gui, storage);
                }

                public IDataBlock CreateStorage(DomDocument document, DomNode parent)
                {
                    return new VanillaStorage();
                }
            }

            public IDictionary<string, IDataBlockDeclaration> DataBlockDeclarations
            {
                get { return _dataBlockDeclarations; }
            }

            private Dictionary<string, IDataBlockDeclaration> _dataBlockDeclarations = new Dictionary<string, IDataBlockDeclaration>();
        }

        [Import] private Sce.Atf.Applications.ScriptingService _scriptingService;
    }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    [Export(typeof(AdaptiveSchemaSourceArchiveModel))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public sealed class AdaptiveSchemaSourceArchiveModel : ITreeModel
    {
        /////////////////////////////////////////////////
        public abstract class BaseItem
        {
            private string _path = "";
            public string ItemPath
            {
                get { return _path; }
                set { _path = value; }
            }

            private Image _icon;
            public Image Icon
            {
                get { return _icon; }
                set { _icon = value; }
            }

            private long _size = 0;
            public long Size
            {
                get { return _size; }
                set { _size = value; }
            }

            private DateTime _date;
            public DateTime Date
            {
                get { return _date; }
                set { _date = value; }
            }

            private BaseItem _parent;
            public BaseItem Parent
            {
                get { return _parent; }
                set { _parent = value; }
            }

            private bool _isChecked;
            public bool IsChecked
            {
                get { return _isChecked; }
                set
                {
                    _isChecked = value;
                    if (Owner != null)
                        Owner.OnNodesChanged(this);
                }
            }

            public AdaptiveSchemaSourceArchiveModel Owner;

            public override string ToString()
            {
                return _path;
            }
        }

        private static Bitmap ResizeImage(Bitmap imgToResize, Size size)
        {
            // (handy utility function; thanks to http://stackoverflow.com/questions/10839358/resize-bitmap-image)
            try
            {
                Bitmap b = new Bitmap(size.Width, size.Height);
                using (Graphics g = Graphics.FromImage((Image)b))
                {
                    g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
                    g.DrawImage(imgToResize, 0, 0, size.Width, size.Height);
                }
                return b;
            }
            catch { }
            return null;
        }

        private static Image ProcessImage(Bitmap img)
        {
            const int normalHeight = 32;
            return ResizeImage(img, new Size(normalHeight * img.Width / img.Height, normalHeight));
        }

        static Image folderIcon = null;
        static Image shaderFileIcon = null;
        static Image shaderFragmentIcon = null;
        static Image parameterIcon = null;

        private static Image GetFolderIcon()
        {
            // if (folderIcon == null) { folderIcon = ProcessImage(Properties.Resources.icon_triangle); }
            return folderIcon;
        }

        private static Image GetShaderFileIcon()
        {
            // if (shaderFileIcon == null) { shaderFileIcon = ProcessImage(Properties.Resources.icon_paper); }
            return shaderFileIcon;
        }

        private static Image GetShaderFragmentIcon()
        {
            // if (shaderFragmentIcon == null) { shaderFragmentIcon = ProcessImage(Properties.Resources.icon_circle); }
            return shaderFragmentIcon;
        }

        private static Image GetParameterIcon()
        {
            // if (parameterIcon == null) { parameterIcon = ProcessImage(Properties.Resources.icon_hexagon); }
            return parameterIcon;
        }

        public class FolderItem : BaseItem
        {
            public string FunctionName { get; set; }
            public string Name { get { return FunctionName; } }
            public FolderItem(string name, BaseItem parent, AdaptiveSchemaSourceArchiveModel owner)
            {
                Icon = GetFolderIcon();
                ItemPath = name;
                FunctionName = Path.GetFileName(name);
                Parent = parent;
                Owner = owner;
            }
        }
        public class PythonFileItem : BaseItem
        {
            private string _exceptionString = "";
            public string ExceptionString
            {
                get { return _exceptionString; }
                set { _exceptionString = value; }
            }
            public string FileName { get; set; }
            public string Name { get { return FileName; } }

            public PythonFileItem(string name, BaseItem parent, AdaptiveSchemaSourceArchiveModel owner)
            {
                Icon = GetShaderFileIcon();
                Parent = parent;
                ItemPath = name;
                FileName = Path.GetFileName(name);
            }
        }

        public class DataBlockItem : BaseItem
        {
            public string _signature = "";
            public string Signature
            {
                get { return _signature; }
                set { _signature = value; }
            }

            public string FunctionName { get; set; }
            public string ArchiveName { get; set; }
            public string Name { get { return FunctionName; } }

            public DataBlockItem(BaseItem parent, AdaptiveSchemaSourceArchiveModel owner)
            {
                Icon = GetShaderFragmentIcon();
                Parent = parent;
                Owner = owner;
            }
        }
        /////////////////////////////////////////////////

        private BackgroundWorker _worker;
        private List<BaseItem> _itemsToRead;
        private Dictionary<string, List<BaseItem>> _cache = new Dictionary<string, List<BaseItem>>();

        public AdaptiveSchemaSourceArchiveModel()
        {
            _itemsToRead = new List<BaseItem>();

            _worker = new BackgroundWorker();
            _worker.WorkerReportsProgress = true;
            _worker.DoWork += new DoWorkEventHandler(ReadFilesProperties);
            // _worker.ProgressChanged += new ProgressChangedEventHandler(ProgressChanged);     (this causes bugs when expanding items)
        }

        void ReadFilesProperties(object sender, DoWorkEventArgs e)
        {
            while (_itemsToRead.Count > 0)
            {
                BaseItem item = _itemsToRead[0];
                _itemsToRead.RemoveAt(0);

                if (item is FolderItem)
                {
                    DirectoryInfo info = new DirectoryInfo(item.ItemPath);
                    item.Date = info.CreationTime;
                }
                else if (item is PythonFileItem)
                {
                    FileInfo info = new FileInfo(item.ItemPath);
                    item.Size = info.Length;
                    item.Date = info.CreationTime;

                    //  We open the file and create children for functions in 
                    //  the GetChildren function
                }

                _worker.ReportProgress(0, item);
            }
        }

        void ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            OnNodesChanged(e.UserState as BaseItem);
        }

        private TreePath GetPath(BaseItem item)
        {
            if (item == null)
                return TreePath.Empty;
            else
            {
                Stack<object> stack = new Stack<object>();
                return new TreePath(stack.ToArray());
            }
        }

        public System.Collections.IEnumerable GetChildren(TreePath treePath)
        {
            const string FragmentArchiveDirectoryRoot = "game/xleres/";

            string basePath = null;
            BaseItem parent = null;
            List<BaseItem> items = null;
            if (treePath.IsEmpty())
            {
                if (_cache.ContainsKey("ROOT"))
                    items = _cache["ROOT"];
                else
                {
                    basePath = FragmentArchiveDirectoryRoot;
                }
            }
            else
            {
                parent = treePath.LastNode as BaseItem;
                if (parent != null)
                {
                    basePath = parent.ItemPath;
                }
            }

            if (basePath != null)
            {
                if (_cache.ContainsKey(basePath))
                    items = _cache[basePath];
                else
                {
                    items = new List<BaseItem>();
                    var fileAttributes = File.GetAttributes(basePath);
                    if ((fileAttributes & FileAttributes.Directory) == FileAttributes.Directory)
                    {
                        // It's a directory... 
                        //          Try to find the files within and create child nodes
                        try
                        {
                            foreach (string str in Directory.GetDirectories(basePath))
                                items.Add(new FolderItem(str, parent, this));
                            foreach (string str in Directory.GetFiles(basePath))
                            {
                                var extension = Path.GetExtension(str);
                                if (extension.Equals(".py", StringComparison.CurrentCultureIgnoreCase))
                                {
                                    var sfi = new PythonFileItem(str, parent, this);
                                    sfi.FileName = Path.GetFileName(str);
                                    items.Add(sfi);
                                }
                            }
                        }
                        catch (IOException)
                        {
                            return null;
                        }
                    }
                    else
                    {
                        // It's a file. Let's try to parse it as a shader file and get the information within
                        var fragment = AdaptiveSchemaSource.GetScript(basePath);

                        PythonFileItem sfi = (PythonFileItem)parent;
                        sfi.ExceptionString = fragment.ExceptionString;

                        if (fragment.Script != null)
                        {
                            foreach (var f in fragment.Script.DataBlockDeclarations)
                            {
                                DataBlockItem fragItem = new DataBlockItem(parent, this);
                                fragItem.FunctionName = f.Key;
                                // fragItem.Signature = BuildSignature(f.Value);
                                fragItem.ArchiveName = basePath + ":" + f.Key;
                                items.Add(fragItem);
                            }
                        }
                    }
                    _cache.Add(basePath, items);
                    _itemsToRead.AddRange(items);
                    if (!_worker.IsBusy)
                        _worker.RunWorkerAsync();
                }
            }
            return items;
        }

        public bool IsLeaf(TreePath treePath)
        {
            return treePath.LastNode is DataBlockItem;
        }

        public event EventHandler<TreeModelEventArgs> NodesChanged;
        internal void OnNodesChanged(BaseItem item)
        {
            if (NodesChanged != null)
            {
                TreePath path = GetPath(item.Parent);
                NodesChanged(this, new TreeModelEventArgs(path, new object[] { item }));
            }
        }

        public event EventHandler<TreeModelEventArgs> NodesInserted;
        public event EventHandler<TreeModelEventArgs> NodesRemoved;
        public event EventHandler<TreePathEventArgs> StructureChanged;

        private void OnStructureChanged(Object sender, EventArgs args)
        {
            _cache = new Dictionary<string, List<BaseItem>>();
            if (StructureChanged != null)
                StructureChanged(this, new TreePathEventArgs());
        }

        [Import]
        internal PythonAdaptiveSchemaSource AdaptiveSchemaSource;
    }
}
