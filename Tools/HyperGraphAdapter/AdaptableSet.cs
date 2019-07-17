// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

using System;
using System.Collections.Generic;
using Sce.Atf.Adaptation;

namespace HyperGraphAdapter
{
    class AdaptableSet : IAdaptable, IDecoratable
    {
        public object GetAdapter(Type type)
        {
            foreach (var o in _subObjects)
            {
                var r = o.As(type);
                if (r != null) return r;
            }
            return null;
        }

        public IEnumerable<object> GetDecorators(Type type)
        {
            foreach (var o in _subObjects)
            {
                var d = o.As<IDecoratable>();
                if (d == null) continue;
                var l = d.GetDecorators(type);
                foreach (var i in l) yield return i;
            }
        }

        public AdaptableSet(IEnumerable<object> subObjects) { _subObjects = subObjects; }

        private IEnumerable<object> _subObjects;
    }
}
