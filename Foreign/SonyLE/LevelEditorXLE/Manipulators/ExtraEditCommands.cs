// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.Linq;

using Sce.Atf;
using Sce.Atf.Applications;
using Sce.Atf.Adaptation;

using LevelEditorCore;

namespace LevelEditorXLE.Manipulators
{
    [Export(typeof(IInitializable))]
    [PartCreationPolicy(CreationPolicy.Shared)]
    class ExtraEditCommands : ICommandClient, IInitializable
    {
        #region ICommandClient members
        public bool CanDoCommand(object commandTag)
        {
            if (!(commandTag is Command))
                return false;

            switch ((Command)commandTag)
            {
                case Command.RandomizeTransforms:
                    return _contextRegistry.GetActiveContext<ISelectionContext>().Selection.AsIEnumerable<ITransformable>().Any();
            }

            return false;
        }

        public void DoCommand(object commandTag)
        {
            if (!(commandTag is Command)) return;

            switch ((Command)commandTag)
            {
                case Command.RandomizeTransforms:
                    PerformRandomizeTransforms();
                    break;
            }
        }

        public void UpdateCommand(object commandTag, Sce.Atf.Applications.CommandState state) {}

        private enum Command
        {
            RandomizeTransforms
        }
        #endregion

        public Tuple<double, double> BoxMullerNormalDist(Random rand)
        {
            // Here, we use the Box Muller algorithm to generate a number on the
            // normal distribution from a random number generator that produces
            // a uniform distribution. 
            // Generates two numbers for mean=0 and stddev=1
            // In most cases, we should get a single time through the loop.
            double v1, v2, rSquared;
            do
            {
                v1 = 2.0 * rand.NextDouble() - 1.0;
                v2 = 2.0 * rand.NextDouble() - 1.0;
                rSquared = v1 * v1 + v2 * v2;
            } while (rSquared >= 1.0 || rSquared == 0.0);

            var polar = Math.Sqrt(-2.0 * Math.Log(rSquared) / rSquared);
            return new Tuple<double, double>(v1 * polar, v2 * polar);
        }

        private void PerformRandomizeTransforms()
        {
            RandomizeTransformsForm.Settings settings;
            using (var form = new RandomizeTransformsForm())
            {
                if (form.ShowDialog() != System.Windows.Forms.DialogResult.OK)
                    return;
                settings = form.Value;
            }

            // We want to randomly modify the transform of every ITransformable in the
            // selection.

            var objects = _contextRegistry.GetActiveContext<ISelectionContext>().Selection.AsIEnumerable<ITransformable>();
            var rand = new Random();

            if (settings.RandomizeScales == RandomizeTransformsForm.Settings.RandomizeMode.Normal) 
            {
                foreach (var o in objects)
                {
                    if (    (   (o.TransformationType & TransformationTypes.Scale)!=0 
                            ||  (o.TransformationType & TransformationTypes.UniformScale)!=0))
                    {
                        var s = (float)BoxMullerNormalDist(rand).Item1 * settings.ScaleStdDev + settings.ScaleMean;
                        o.Scale = new Sce.Atf.VectorMath.Vec3F(s, s, s);
                    }
                }
            }
            if (settings.RandomizeScales == RandomizeTransformsForm.Settings.RandomizeMode.Uniform)
            {
                foreach (var o in objects)
                {
                    if (((o.TransformationType & TransformationTypes.Scale) != 0
                            || (o.TransformationType & TransformationTypes.UniformScale) != 0))
                    {
                        var s = settings.ScaleMinimum + (settings.ScaleMaximum - settings.ScaleMinimum) * (float)rand.NextDouble();
                        o.Scale = new Sce.Atf.VectorMath.Vec3F(s, s, s);
                    }
                }
            }
            if (settings.RandomizeRotations)
            {
                foreach (var o in objects)
                {
                    if ((o.TransformationType & TransformationTypes.Rotation)!=0)
                    {
                        // Set the rotation to a rotation around the Z axis. We're going to wipe any other
                        // rotations on the object.
                        // There's no point in using any distribution other than a uniform distribution here
                        var r = (float)(2.0 * Math.PI * rand.NextDouble() - Math.PI);
                        r = Math.Max(r, 0.01f); // clamping a minimum value to prevent strange results (such as negative scales)
                        o.Rotation = new Sce.Atf.VectorMath.Vec3F(0, 0, r);
                    }
                }
            }
        }

        public virtual void Initialize()
        {
            _commandService.RegisterCommand(
                Command.RandomizeTransforms,
                StandardMenu.Edit,
                "Manipulators",
                "Randomize Transforms...".Localize(),
                "Sets the scales and rotations of all selected objects to random values".Localize(),
                System.Windows.Forms.Keys.None,
                LevelEditorCore.Resources.CubesImage,
                CommandVisibility.Menu,
                this);
        }

        [Import(AllowDefault = false)]
        private ICommandService _commandService;
        [Import(AllowDefault = false)]
        private IContextRegistry _contextRegistry;
    }
}
