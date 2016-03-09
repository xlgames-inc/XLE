// IQueryableContext implementation that uses the IEnumerable context for traversing the tree.
// This can allow for following links to sub documents
// Based on ATF\Framework\Atf.Core\SearchAndReplace\DomNodeQueryable.cs with the dom node traversal replaced
// with IEnumerable traversal. Unfortunately we can't extend from DomNodeQuery easily!

using System;
using System.Collections.Generic;
using Sce.Atf.Adaptation;
using Sce.Atf;
using Sce.Atf.Dom;
using LevelEditorCore;

namespace LevelEditor.Search
{
    public class EnumerableContextQueryable : DomNodeAdapter, IQueryableContext, IQueryableResultContext, IQueryableReplaceContext
    {
        #region IQueryableContext members
        public IEnumerable<object> Query(IQueryPredicate predicate)
        {
            m_results.Clear();
            var enumerable = DomNode.As<IEnumerableContext>();
            if (enumerable == null || predicate == null)
            {
                ResultsChanged.Raise(this, EventArgs.Empty);
                return m_results;
            }

            // Iterate over all dom nodes under this adapter
            foreach (var o in enumerable.Items)
            {
                var domNode = o.As<DomNode>();
                if (domNode == null) continue;

                // The results of one DomNode query associate each predicate with matching dom node properties
                Dictionary<IQueryPredicate, IList<IQueryMatch>> predicateMatchResults
                    = new Dictionary<IQueryPredicate, IList<IQueryMatch>>();

                // For each queryable item (ie a DomNode) there may be 0 to many "query matches" 
                // (ie a DomNode property).  On success, predicate.Test() will supply one 
                // IQueryMatch per DomNode property that matched.
                IList<IQueryMatch> matchingPropertiesList;
                if (predicate.Test(domNode, out matchingPropertiesList))
                {
                    if (matchingPropertiesList != null)
                        predicateMatchResults[predicate] = matchingPropertiesList;

                    // For this queryable, a match is the DomNode that passed the predicate test,
                    // paired with all its properties that allowed it to satisfy the test
                    m_results.Add(new DomNodeQueryMatch(domNode, predicateMatchResults));
                }
            }

            // Announce that the search results have changed
            ResultsChanged.Raise(this, EventArgs.Empty);

            return m_results;
        }
        #endregion

        #region IQueryableResultContext members
        public event EventHandler ResultsChanged;
        public IEnumerable<object> Results 
        { 
            get 
            {
                return m_results;
                // foreach (var r in m_results)
                // {
                //     var n = r.As<DomNode>();
                //     if (n != null)
                //         yield return Util.AdaptDomPath(n);
                // }
            }
        }
        #endregion

        #region IQueryableReplaceContext members
        public IEnumerable<object> Replace(object replaceInfo)
        {
            ITransactionContext currentTransaction = null;
            try
            {
                foreach (DomNodeQueryMatch match in m_results)
                {
                    DomNode domNode = match.DomNode;

                    // Set up undo/redo for the replacement operation
                    ITransactionContext newTransaction = domNode != null ? domNode.GetRoot().As<ITransactionContext>() : null;
                    if (newTransaction != currentTransaction)
                    {
                        {
                            if (currentTransaction != null)
                                currentTransaction.End();
                            currentTransaction = newTransaction;
                            if (currentTransaction != null)
                                currentTransaction.Begin("Replace".Localize());
                        }
                    }

                    // Apply replacement to all matching items that were found on last search
                    foreach (IQueryPredicate predicateInfo in match.PredicateMatchResults.Keys)
                        predicateInfo.Replace(match.PredicateMatchResults[predicateInfo], replaceInfo);
                }
            }
            catch (InvalidTransactionException ex)
            {
                // cancel the replacement transaction in the undo/redo queue
                if (currentTransaction != null && currentTransaction.InTransaction)
                    currentTransaction.Cancel();

                if (ex.ReportError)
                    Outputs.WriteLine(OutputMessageType.Error, ex.Message);
            }
            finally
            {
                // finish the replacement transaction for the undo/redo queue
                if (currentTransaction != null && currentTransaction.InTransaction)
                    currentTransaction.End();
            }

            ResultsChanged.Raise(this, EventArgs.Empty);

            return m_results;
        }
        #endregion

        private readonly List<object> m_results = new List<object>();
    }
}
