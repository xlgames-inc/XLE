using System;
using System.Collections.Generic;
using System.Text;
using Microsoft.VisualStudio.Shell;
using System.Runtime.InteropServices;
using System.IO;
using System.ComponentModel;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio;
using EnvDTE;

namespace RibbonGenerator
{
    public abstract class VsMultipleFileGenerator<IterativeElementType> : IEnumerable<IterativeElementType>, IVsSingleFileGenerator, IObjectWithSite
    {
        #region Visual Studio Specific Fields
        private object site;
        private Microsoft.VisualStudio.OLE.Interop.IServiceProvider serviceProvider = null;
        #endregion

        #region Our Fields
        private string bstrInputFileContents;
        private string wszInputFilePath;

        private List<string> newFileNames = new List<string>();
        #endregion


        /// <summary>
        /// Method to get a service by its Type
        /// </summary>
        /// <param name="serviceType">Type of service to retrieve</param>
        /// <returns>An object that implements the requested service</returns>
        protected object GetService(Type serviceType)
        {
            return GetService(SiteServiceProvider, serviceType.GUID);
        }

        /// <summary>
        /// Gets the specified service from known service provider and for given GUID.
        /// </summary>
        public static object GetService(Microsoft.VisualStudio.OLE.Interop.IServiceProvider serviceProvider, Guid serviceGUID)
        {
            object objService = null;
            IntPtr objInPtr;
            Guid objSIDGuid = serviceGUID;
            Guid objIIDGuid = serviceGUID;

            int hResult = serviceProvider.QueryService(ref objSIDGuid, ref objIIDGuid, out objInPtr);
            if (hResult != 0)
                //Marshal.ThrowExceptionForHR(hResult);
                return null;

            if (objInPtr != IntPtr.Zero)
            {
                objService = Marshal.GetObjectForIUnknown(objInPtr);
                Marshal.Release(objInPtr);
            }

            return objService;
        }


        /// <summary>
        /// Returns the EnvDTE.ProjectItem object that corresponds to the project item the code 
        /// generator was called on
        /// </summary>
        /// <returns>The EnvDTE.ProjectItem of the project item the code generator was called on</returns>
        protected ProjectItem GetProjectItem()
        {
            object p = GetService(typeof(ProjectItem));

            return (ProjectItem)p;
        }

        /// <summary>
        /// Returns the EnvDTE.Project object of the project containing the project item the code 
        /// generator was called on
        /// </summary>
        /// <returns>
        /// The EnvDTE.Project object of the project containing the project item the code generator was called on
        /// </returns>
        protected Project GetProject()
        {
            return GetProjectItem().ContainingProject;
        }




        private Microsoft.VisualStudio.OLE.Interop.IServiceProvider SiteServiceProvider
        {
            get
            {
                if (serviceProvider == null)
                    serviceProvider = site as Microsoft.VisualStudio.OLE.Interop.IServiceProvider;

                return serviceProvider;
            }
        }




        protected string InputFileContents
        {
            get
            {
                return bstrInputFileContents;
            }
        }

        protected string InputFilePath
        {
            get
            {
                return wszInputFilePath;
            }
        }

        
        public VsMultipleFileGenerator()
        {
        }
        public abstract IEnumerator<IterativeElementType> GetEnumerator();

        System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }

        IVsGeneratorProgress _pGenerateProgress;
        public IVsGeneratorProgress GeneratorProgress
        {
            get
            {
                return _pGenerateProgress;
            }
        }

        protected abstract string GetFileName(IterativeElementType element);
        public abstract byte[] GenerateContent(IterativeElementType element);
        public abstract int DefaultExtension(out string extension);
        public abstract string GetMultiExtension();


        public abstract byte[] GenerateSummaryContent();

        public int Generate(string wszInputFilePath, string bstrInputFileContents, string wszDefaultNamespace, IntPtr[] rgbOutputFileContents, out uint pcbOutput, IVsGeneratorProgress pGenerateProgress)
        {
            this._pGenerateProgress = pGenerateProgress;
            this.bstrInputFileContents = bstrInputFileContents;
            this.wszInputFilePath = wszInputFilePath;
            this.newFileNames.Clear();

            int iFound = 0;
            uint itemId = 0;
            EnvDTE.ProjectItem item;
            Microsoft.VisualStudio.Shell.Interop.VSDOCUMENTPRIORITY[] pdwPriority = new Microsoft.VisualStudio.Shell.Interop.VSDOCUMENTPRIORITY[1];

            // obtain a reference to the current project as an IVsProject type
            Microsoft.VisualStudio.Shell.Interop.IVsProject VsProject = VsHelper.ToVsProject(GetProject());
            // this locates, and returns a handle to our source file, as a ProjectItem
            VsProject.IsDocumentInProject(InputFilePath, out iFound, pdwPriority, out itemId);


            // if our source file was found in the project (which it should have been)
            if (iFound != 0 && itemId != 0)
            {
                Microsoft.VisualStudio.OLE.Interop.IServiceProvider oleSp = null;
                VsProject.GetItemContext(itemId, out oleSp);
                if (oleSp != null)
                {
                    ServiceProvider sp = new ServiceProvider(oleSp);
                    // convert our handle to a ProjectItem
                    item = sp.GetService(typeof(EnvDTE.ProjectItem)) as EnvDTE.ProjectItem;
                }
                else
                    throw new ApplicationException("Unable to retrieve Visual Studio ProjectItem");
            }
            else
                throw new ApplicationException("Unable to retrieve Visual Studio ProjectItem");

            // now we can start our work, iterate across all the 'elements' in our source file 
            foreach (IterativeElementType element in this)
            {
                try
                {
                    // obtain a name for this target file
                    string fileName = GetFileName(element);
                    // add it to the tracking cache
                    newFileNames.Add(fileName);
                    // fully qualify the file on the filesystem
                    string strFile = Path.Combine(wszInputFilePath.Substring(0, wszInputFilePath.LastIndexOf(Path.DirectorySeparatorChar)), fileName);
                    // create the file
                    FileStream fs = File.Create(strFile);

                    try
                    {
                        // generate our target file content
                        byte[] data = GenerateContent(element);

                        // write it out to the stream
                        fs.Write(data, 0, data.Length);

                        fs.Close();

                        // add the newly generated file to the solution, as a child of the source file...
                        EnvDTE.ProjectItem itm = item.ProjectItems.AddFromFile(strFile);
                        /*
                         * Here you may wish to perform some addition logic
                         * such as, setting a custom tool for the target file if it
                         * is intented to perform its own generation process.
                         * Or, set the target file as an 'Embedded Resource' so that
                         * it is embedded into the final Assembly.
                         
                        EnvDTE.Property prop = itm.Properties.Item("CustomTool");
                        */
                        
                        //// set to embedded resource
                        itm.Properties.Item("BuildAction").Value = 3;
                        /*
                        if (String.IsNullOrEmpty((string)prop.Value) || !String.Equals((string)prop.Value, typeof(AnotherCustomTool).Name))
                        {
                            prop.Value = typeof(AnotherCustomTool).Name;
                        }
                        */
                    }
                    catch (Exception)
                    {
                        fs.Close();
                        if (File.Exists(strFile))
                            File.Delete(strFile);
                    }
                }
                catch (Exception ex)
                {
                    throw ex;
                }
            }

            // perform some clean-up, making sure we delete any old (stale) target-files
            foreach (EnvDTE.ProjectItem childItem in item.ProjectItems)
            {
                if (!(childItem.Name.EndsWith(GetMultiExtension()) || newFileNames.Contains(childItem.Name)))
                    // then delete it
                    childItem.Delete();
            }

            // generate our summary content for our 'single' file
            byte[] summaryData = GenerateSummaryContent();

            if (summaryData == null)
            {
                rgbOutputFileContents[0] = IntPtr.Zero;

                pcbOutput = 0;
            }
            else
            {
                // return our summary data, so that Visual Studio may write it to disk.
                rgbOutputFileContents[0] = Marshal.AllocCoTaskMem(summaryData.Length);
                Marshal.Copy(summaryData, 0, rgbOutputFileContents[0], summaryData.Length);

                pcbOutput = (uint)summaryData.Length;
            }

            return VSConstants.S_OK;
        }

        #region IObjectWithSite Members

        public void GetSite(ref Guid riid, out IntPtr ppvSite)
        {
            if (this.site == null)
            {
                throw new Win32Exception(-2147467259);
            }

            IntPtr objectPointer = Marshal.GetIUnknownForObject(this.site);

            try
            {
                Marshal.QueryInterface(objectPointer, ref riid, out ppvSite);
                if (ppvSite == IntPtr.Zero)
                {
                    throw new Win32Exception(-2147467262);
                }
            }
            finally
            {
                if (objectPointer != IntPtr.Zero)
                {
                    Marshal.Release(objectPointer);
                    objectPointer = IntPtr.Zero;
                }
            }
        }

        public void SetSite(object pUnkSite)
        {
            this.site = pUnkSite;
        }

        #endregion

    }
}

