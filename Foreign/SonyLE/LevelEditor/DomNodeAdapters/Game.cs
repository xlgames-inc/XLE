//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.


using System.Collections.Generic;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;

using LevelEditorCore;

namespace LevelEditor.DomNodeAdapters
{
   
    /// <summary>
    /// Root node in a LevelEditor Game document</summary>
    public class Game : DomNodeAdapter, IGame
    {        
        #region INameable Members

        /// <summary>
        /// Gets or sets the name</summary>
        public string Name
        {
            get { return (string)DomNode.GetAttribute(Schema.gameType.nameAttribute); }
            set { DomNode.SetAttribute(Schema.gameType.nameAttribute, value); }
        }

        #endregion

        #region IListable Members

        /// <summary>
        /// Provides info for the ProjectLister tree view and other controls</summary>
        /// <param name="info">Item info passed in and modified by the method</param>
        public void GetInfo(ItemInfo info)
        {
            info.ImageIndex = Util.GetTypeImageIndex(DomNode.Type, info.GetImageList());
            info.Label = Name;
        }

        #endregion

        #region IHierarchical Members

        /// <summary>
        /// Returns true iff the specified child can be inserted</summary>
        /// <param name="child">Child to be inserted</param>
        /// <returns>True iff the specified child can be inserted as a child of this object</returns>
        /// <remarks>Unlike most other IHierarchical implementations, the specified objects
        /// are NOT inserted as children of this object but rather as children
        /// of the GameObjectFolder child (thus becoming grand children of Game)</remarks>
        public bool CanAddChild(object child)
        {
            if (child.Is<GameReference>()) return true;

            // <<XLE
            var domNode = child as DomNode;
            if (domNode != null) {
                foreach (var type in domNode.Type.Lineage) {
                    if (type == Schema.placementsFolderType.Type) return true;
                    if (type == Schema.abstractPlacementObjectType.Type) return true;
                    if (type == Schema.envSettingsFolderType.Type) return true;
                    if (type == Schema.envSettingsType.Type) return true;
                }
            }
            // XLE>>

            return false;
        }

        /// <summary>
        /// Inserts the specified child</summary>
        /// <param name="child">Child to be inserted</param>
        /// <remarks>Unlike most other IHierarchical implementations, the specified objects
        /// are NOT inserted as children of this object but rather as children
        /// of the GameObjectFolder child (thus becoming grand children of Game).</remarks>
        public bool AddChild(object child)
        {            
            GameReference gameref = child.As<GameReference>();
            if (gameref != null)
            {
                IList<GameReference> grefList = GetChildList<GameReference>(Schema.gameType.gameReferenceChild);
                grefList.Add(gameref);
                return true;
            }

            // <<XLE
            var domNode = child.As<DomNode>();
            if (domNode != null)
            {
                foreach (var type in domNode.Type.Lineage)
                {
                    if (domNode.Type == Schema.placementsFolderType.Type)
                    {
                        SetChild(Schema.gameType.placementsChild, domNode);
                        return true;
                    }

                    if (type == Schema.abstractPlacementObjectType.Type)
                    {
                        // We need to look for the appropriate placement cell to put this placement in...
                        // If there are no placement cells, or if there isn't a cell that can contain this
                        // object, then we have to abort
                        var plc = domNode.As<XLEPlacementObject>();
                        var keyPoint = (plc != null) ? plc.Translation : new Sce.Atf.VectorMath.Vec3F(0.0f, 0.0f, 0.0f);

                        var placementsFolder = GetChild<PlacementsFolder>(Schema.gameType.placementsChild);
                        if (placementsFolder != null)
                        {
                            foreach (var cellRef in placementsFolder.Cells)
                            {
                                if (cellRef.IsResolved()
                                    && keyPoint.X >= cellRef.Mins.X && keyPoint.X < cellRef.Maxs.X
                                    && keyPoint.Y >= cellRef.Mins.Y && keyPoint.Y < cellRef.Maxs.Y
                                    && keyPoint.Z >= cellRef.Mins.Z && keyPoint.Z < cellRef.Maxs.Z
                                    && cellRef.Target.CanAddChild(domNode))
                                {
                                    return cellRef.Target.AddChild(domNode);
                                }
                            }
                        }
                    }

                    if (domNode.Type == Schema.envSettingsFolderType.Type)
                    {
                        SetChild(Schema.gameType.environmentChild, domNode);
                        return true;
                    }

                    if (domNode.Type == Schema.envSettingsType.Type)
                    {
                        var folder = GetChild<DomNode>(Schema.gameType.environmentChild);
                        if (folder != null)
                        {
                            folder.GetChildList(Schema.envSettingsFolderType.settingsChild).Add(domNode);
                            return true;
                        }
                    }
                }
            }
            // XLE>>

            return false;           
        }

        #endregion
                       
        #region IGame Members

        /// <summary>
        /// Gets the Grid child</summary>        
        public IGrid Grid
        {
            get { return GetChild<IGrid>(Schema.gameType.gridChild); }
        }

        public IReference<IGame> Parent
        {
            get { return m_parent; }
        }
              
        public IEnumerable<IReference<IGame>> GameReferences
        {
            get { return GetChildList<IReference<IGame>>(Schema.gameType.gameReferenceChild); }
        }

        public IGameObjectFolder RootGameObjectFolder
        {
            get 
            {
                GameObjectFolder rootFolder = GetChild<GameObjectFolder>(Schema.gameType.gameObjectFolderChild);
                if (rootFolder == null)
                {
                    rootFolder = (GameObjectFolder)GameObjectFolder.Create();
                    rootFolder.Name = "GameObjects".Localize("this is the name of a folder in the project lister");
                    SetChild(Schema.gameType.gameObjectFolderChild, rootFolder);
                }
                return rootFolder;              
            }

        }

        public IGameObjectGroup CreateGameObjectGroup()
        {
            GameObjectGroup gobGroup = new DomNode(Schema.gameObjectGroupType.Type).As<GameObjectGroup>();
            gobGroup.Name = "GameObjectGroup".Localize("this is the name of a folder in the project lister");
            return gobGroup;
        }
       
        public IReference<IResource> CreateResourceReference(IResource resource)
        {
            return ResourceReference.Create(resource);
        }


        public IGameObjectFolder CreateGameObjectFolder()
        {
            return GameObjectFolder.Create();
        }

        #endregion

        internal void SetParent(IReference<IGame> gameref)
        {
            m_parent = gameref;
        }

        private IReference<IGame> m_parent;
    }
}
