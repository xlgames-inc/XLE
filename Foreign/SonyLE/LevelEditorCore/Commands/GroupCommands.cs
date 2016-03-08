//Copyright © 2014 Sony Computer Entertainment America LLC. See License.txt.

using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Windows.Forms;
using System.Linq;

using Sce.Atf;
using Sce.Atf.Adaptation;
using Sce.Atf.Applications;
using Sce.Atf.Dom;
using Sce.Atf.VectorMath;

using LevelEditorCore.VectorMath;

namespace LevelEditorCore.Commands
{
    /// <summary>
    /// Group and Ungroup game objects commands</summary>
    /// <remarks>    
    /// Group: Creates a new GameObjectGroup, moves all the selected GameObjects
    /// to the newly created group.    
    /// Ungroup: Selected GameObjects that are in a group are moved
    /// to the root GameObjectFolder of their level.
    /// </remarks>
    [Export(typeof(GroupCommands))]
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    public class GroupCommands : ICommandClient, IInitializable
    {
        [ImportingConstructor]
        public GroupCommands(ICommandService commandService, IContextRegistry contextRegistry)
        {
            m_commandService = commandService;
            m_contextRegistry = contextRegistry;            
        }

        private IGameObjectFolder GetObjectFolder(DomNode n)
        {
            foreach(var p in n.Lineage) 
            {
                var folder = p.As<IGameObjectFolder>();
                if (folder != null) return folder;
            }
            return null;
        }

        /// <summary>
        /// Tests if the gameobjects can be grouped.
        /// The gameobjects can be grouped if there are more than one 
        /// and they belong to the same level.</summary>        
        public bool CanGroup(IEnumerable<object> gobs)
        {
            // can group if there are more than one gob and they all belong to same level.
            IGameObjectFolder root1 = null;
            int gobcount = 0;
            foreach (var gob in gobs)
            {
                    // ignore objects that are not ITransformables
                if (!gob.Is<ITransformable>()) continue;

                var node = gob.As<DomNode>();
                if (node == null) continue;
                var root = GetObjectFolder(node);
                if (root == null) return false;     // (cannot group detached nodes)
                if (root1 != null && root1 != root)
                    return false;
                root1 = root;

                gobcount++;
            }
            return gobcount > 1;
        }

        /// <summary>
        /// Tests if the Gameobjects can be Ungrouped.        
        /// </summary>        
        public bool CanUngroup(IEnumerable<object> gobs)
        {
            return gobs.AsIEnumerable<ITransformableGroup>().FirstOrDefault() != null;
        }

        /// <summary>
        /// Groups the specified GameObjects</summary>
        /// <param name="gobs">GameObjects to be grouped</param>
        /// <remarks>Creates a new GameObjectGroup and moves all 
        /// the GameObjects into it.</remarks>
        public ITransformableGroup Group(IEnumerable<object> gobs)
        {
            // extra check.
            if (!CanGroup(gobs)) return null;

            IGameObjectFolder root = null;
            var gameObjects = new List<ITransformable>();
            foreach (var gameObject in gobs)
            {
                ITransformable trans = gameObject.As<ITransformable>();
                if (trans==null) continue;
                var node = gameObject.As<DomNode>();
                if (node == null) continue;

                gameObjects.Add(trans);
                var root1 = GetObjectFolder(node);
                if (root != null && root != root1) return null;
                root = root1;
            }

            // sort from shallowest in the tree to deepest. Then remove any nodes that
            // are already children of other nodes in the list
            gameObjects.Sort(
                (lhs, rhs) => { return lhs.As<DomNode>().Lineage.Count().CompareTo(rhs.As<DomNode>().Lineage.Count()); });

            // awkward iteration here... Maybe there's a better way to traverse this list..?
            for (int c=0; c<gameObjects.Count;)
            {
                var n = gameObjects[c].As<DomNode>();
                bool remove = false;
                for (int c2=0; c2<c; ++c2)
                    if (n.IsDescendantOf(gameObjects[c2].As<DomNode>())) {
                        remove = true;
                        break;
                    }

                if (remove) { gameObjects.RemoveAt(c); }
                else ++c;
            }

            // finally, we must have at least 2 valid objects to perform the grouping operation
            if (gameObjects.Count < 2) return null;

            AABB groupBox = new AABB();
            foreach (var gameObject in gameObjects.AsIEnumerable<IBoundable>())
                groupBox.Extend(gameObject.BoundingBox);

            var group = root.CreateGroup();
            if (group == null) return null;

            group.As<DomNode>().InitializeExtensions();

            // try to add the group to the parent of the shallowest item
            var groupParent = gameObjects[0].As<DomNode>().Parent.AsAll<IHierarchical>();
            bool addedToGroupParent = false;
            foreach(var h in groupParent)
                if (h.AddChild(group)) { addedToGroupParent = true; break; }
            if (!addedToGroupParent) return null;

            // arrange the transform for the group so that it's origin is in the center of the objects
            var groupParentTransform = new Matrix4F();
            if (groupParent.Is<ITransformable>())
                groupParentTransform = groupParent.As<ITransformable>().LocalToWorld;

            Matrix4F invgroupParentTransform = new Matrix4F();
            invgroupParentTransform.Invert(groupParentTransform);
            group.Transform = Matrix4F.Multiply(new Matrix4F(groupBox.Center), invgroupParentTransform);
            
            Matrix4F invWorld = new Matrix4F();
            invWorld.Invert(group.Transform);

            // now try to actually add the objects to the group
            var hier = group.AsAll<IHierarchical>();
            foreach (var gameObject in gameObjects)
            {
                Matrix4F oldWorld = gameObject.LocalToWorld;

                bool addedToGroup = false;
                foreach (var h in hier)
                    if (h.AddChild(gameObject)) { addedToGroup = true;  break; }

                if (addedToGroup)
                    gameObject.Transform = Matrix4F.Multiply(oldWorld, invWorld);
            }

            return group;
        }

        /// <summary>
        /// Ungroups the specified gameobjects.</summary>        
        public IEnumerable<ITransformable> Ungroup(IEnumerable<object> gobs)
        {
            if (!CanUngroup(gobs))
                return EmptyArray<ITransformable>.Instance;

            List<ITransformable> ungrouplist = new List<ITransformable>();
            foreach (var group in gobs.AsIEnumerable<ITransformableGroup>())
            {
                // try to move the children into the parent of the group
                var node = group.As<DomNode>();
                if (node == null) continue;
                var insertParent = node.Lineage.Skip(1).AsIEnumerable<IHierarchical>().FirstOrDefault();
                if (insertParent == null) continue;

                var groupTransform = group.Transform;
                var objects = new List<ITransformable>(group.Objects);  // (copy, because we'll remove as we go through)
                foreach (var child in objects)
                {
                    if (insertParent.AddChild(child))
                    {
                        child.Transform = Matrix4F.Multiply(child.Transform, groupTransform);
                        ungrouplist.Add(child);
                    }
                }

                if (group.Objects.Count == 0)
                    group.Cast<DomNode>().RemoveFromParent();
            }
            return ungrouplist;          
        }

        #region IInitializable Members

        void IInitializable.Initialize()
        {
            // Register commands
            m_commandService.RegisterCommand(StandardCommand.EditGroup, StandardMenu.Edit, StandardCommandGroup.EditGroup,
                "Group".Localize(), "Group".Localize(), Keys.Control | Keys.G, null, CommandVisibility.All, this);
            m_commandService.RegisterCommand(StandardCommand.EditUngroup, StandardMenu.Edit, StandardCommandGroup.EditGroup,
                "Ungroup".Localize(), "Ungroup".Localize(), Keys.Control | Keys.U, null, CommandVisibility.All, this);

            if (m_scriptingService != null)
                m_scriptingService.SetVariable("groupCommands", this);

            m_contextRegistry.ActiveContextChanged += ContextRegistry_ActiveContextChanged;
        }

        #endregion

        #region ICommandClient Members

        /// <summary>
        /// Returns true iff the specified command can be performed</summary>
        /// <param name="commandTag">Command</param>
        /// <returns>True iff the specified command can be performed</returns>
        bool ICommandClient.CanDoCommand(object commandTag)
        {
            switch ((StandardCommand)commandTag)
            {
                case StandardCommand.EditGroup:
                    return m_canGroup;
                case StandardCommand.EditUngroup:
                    return m_canUngroup;
            }
            return false;

        }

        /// <summary>
        /// Does the specified command</summary>
        /// <param name="commandTag">Command</param>
        void ICommandClient.DoCommand(object commandTag)
        {
            ITransactionContext transactionContext = m_selectionContext.As<ITransactionContext>();
            if (transactionContext == null) return;

            switch ((StandardCommand)commandTag)
            {
                case StandardCommand.EditGroup:
                    transactionContext.DoTransaction(delegate
                    {
                        var group = Group(SelectedGobs);
                        if (group != null)
                            m_selectionContext.Set(Util.AdaptDomPath(group.As<DomNode>()));
                    }, "Group".Localize());                    
                    break;
                case StandardCommand.EditUngroup:
                    transactionContext.DoTransaction(delegate
                    {
                        var gobs = Ungroup(SelectedGobs);
                        List<object> newselection = new List<object>();
                        foreach(var gob in gobs)
                        {
                            newselection.Add(Util.AdaptDomPath(gob.As<DomNode>()));
                        }
                        m_selectionContext.SetRange(newselection);

                    }, "Ungroup".Localize());                    
                    break;
            }
        }

        /// <summary>
        /// Updates command state for given command</summary>
        /// <param name="commandTag">Command</param>
        /// <param name="state">Command state to update</param>
        void ICommandClient.UpdateCommand(object commandTag, Sce.Atf.Applications.CommandState state)
        {
        }

        #endregion

        /// <summary>
        /// Gets the current GameContext's selection of GameObjects</summary>
        private IEnumerable<object> SelectedGobs
        {
            get
            {
                return
                    m_selectionContext != null
                    ? DomNode.GetRoots(m_selectionContext.GetSelection<DomNode>()) 
                    : EmptyArray<DomNode>.Instance;
            }
        }
       
        private void ContextRegistry_ActiveContextChanged(object sender, System.EventArgs e)
        {
            if (m_selectionContext != null)
            {
                m_selectionContext.SelectionChanged -= SelectionChanged;                
            }

            object context = m_contextRegistry.GetActiveContext<IGameContext>();
            m_selectionContext = (ISelectionContext)context;

            if (m_selectionContext != null)
            {
                m_selectionContext.SelectionChanged += SelectionChanged;             
            }

            m_canGroup = CanGroup(SelectedGobs);
            m_canUngroup = CanUngroup(SelectedGobs);
        }

        private void SelectionChanged(object sender, System.EventArgs e)
        {
            m_canGroup = CanGroup(SelectedGobs);
            m_canUngroup = CanUngroup(SelectedGobs);
        }

        private bool m_canGroup;
        private bool m_canUngroup;
        private ISelectionContext m_selectionContext;
        private readonly ICommandService m_commandService;
        private readonly IContextRegistry m_contextRegistry;

        [Import(AllowDefault = true)]
        private ScriptingService m_scriptingService = null;
    }
}
