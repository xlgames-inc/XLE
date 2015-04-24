using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Text.RegularExpressions;

namespace ControlsLibrary
{
    public partial class FileCompare : UserControl
    {
        public FileCompare()
        {
            InitializeComponent();
        }

        public Tuple<object, object> Comparison
        {
            set 
            {
                if (value==null)
                {
                    _originalVersion.Clear();
                    _newVersion.Clear();
                    return;
                }

                    //  Given to versions of the same file, use
                    //  the DiffMatchPatch library to generate a
                    //  list of difference. Then we can write those
                    //  differences to some RichEditControls using some
                    //  basic highlighting
                string original = value.Item1.ToString();
                string changed = value.Item2.ToString();

                var comparer = new DiffMatchPatch.diff_match_patch();
                var diffs = comparer.diff_main(original, changed);
                comparer.diff_cleanupSemantic(diffs);

                var lhs = new StringBuilder();
                var rhs = new StringBuilder();

                var rtfHeader = @"{\rtf1\ansi{\fonttbl{\f0\fnil\fcharset0 Courier New;}}{\colortbl;\red160\green32\blue32;\red32\green160\blue32;}\f1\fs18\pard";

                lhs.Append(rtfHeader); lhs.Append(Environment.NewLine);
                rhs.Append(rtfHeader); lhs.Append(Environment.NewLine);
                foreach (var d in diffs)
                {
                    var text = FixLineBreaks(d.text);
                    if (d.operation == DiffMatchPatch.Operation.EQUAL) 
                    {
                        lhs.Append(text);
                        rhs.Append(text);
                    }
                    else if (d.operation == DiffMatchPatch.Operation.DELETE)
                    {
                        lhs.Append(@"{\cf1\ul ");
                        lhs.Append(text);
                        lhs.Append("}");
                    }
                    else if (d.operation == DiffMatchPatch.Operation.INSERT)
                    {
                        rhs.Append(@"{\cf2\b ");
                        rhs.Append(text);
                        rhs.Append("}");
                    }
                }

                lhs.Append("\n}");
                rhs.Append("\n}");
                _originalVersion.Rtf = lhs.ToString();
                _newVersion.Rtf = rhs.ToString();
            }
        }

        static string FixLineBreaks(string input) { return Regex.Replace(input, @"\r\n?|\n", @"\par "); }
    }
}
