#region License
// Copyright (c) 2009 Sander van Rossen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#endregion

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Drawing;

namespace HyperGraph
{
	public static class GraphConstants
	{
		public const int MinimumItemWidth		= 64+8;
		public const int MinimumItemHeight		= 12;
		public const int TitleHeight			= 20;
		public const int ItemSpacing			= 2;
		public const int TopHeight				= 2;
        public const int BottomHeight           = 2;
        public const int TopHeightCollapsed     = 6;
        public const int BottomHeightCollapsed  = 6;
        public const int CornerSize             = 4;
        public const int SubGraphCornerSize     = 64;
        public const int ConnectorCornerSize    = 12;
		public const int ConnectorWidth			= 128;
        public const int ConnectorWidthCollapsed= 8;
		public const int HorizontalSpacing		= 8;
        public const int NodeExtraWidth         = GraphConstants.HorizontalSpacing * 2;
        public const float ConnectionWidth      = 1.5f;

        public const TextFormatFlags TitleTextFlags =       TextFormatFlags.ExternalLeading |
															TextFormatFlags.GlyphOverhangPadding |
															TextFormatFlags.HorizontalCenter |
															TextFormatFlags.NoClipping |
															TextFormatFlags.NoPadding |
															TextFormatFlags.NoPrefix |
															TextFormatFlags.VerticalCenter;

        public const TextFormatFlags CenterTextFlags =      TextFormatFlags.ExternalLeading |
															TextFormatFlags.GlyphOverhangPadding |
															TextFormatFlags.HorizontalCenter |
															TextFormatFlags.NoClipping |
															TextFormatFlags.NoPadding |
															TextFormatFlags.NoPrefix |
															TextFormatFlags.VerticalCenter;

        public const TextFormatFlags LeftTextFlags =        TextFormatFlags.ExternalLeading |
															TextFormatFlags.GlyphOverhangPadding |
															TextFormatFlags.Left |
															TextFormatFlags.NoClipping |
															TextFormatFlags.NoPadding |
															TextFormatFlags.NoPrefix |
															TextFormatFlags.VerticalCenter;

        public const TextFormatFlags RightTextFlags =       TextFormatFlags.ExternalLeading |
															TextFormatFlags.GlyphOverhangPadding |
															TextFormatFlags.Right |
															TextFormatFlags.NoClipping |
															TextFormatFlags.NoPadding |
															TextFormatFlags.NoPrefix |
															TextFormatFlags.VerticalCenter;

		public static readonly StringFormat TitleStringFormat;
		public static readonly StringFormat CenterTextStringFormat;
		public static readonly StringFormat LeftTextStringFormat;
		public static readonly StringFormat RightTextStringFormat;
		public static readonly StringFormat TitleMeasureStringFormat;
		public static readonly StringFormat CenterMeasureTextStringFormat;
		public static readonly StringFormat LeftMeasureTextStringFormat;
		public static readonly StringFormat RightMeasureTextStringFormat;

		static GraphConstants()
		{
			var defaultFlags = StringFormatFlags.NoClip | StringFormatFlags.NoWrap | StringFormatFlags.LineLimit;
			TitleStringFormat							= new StringFormat(defaultFlags);
			TitleMeasureStringFormat					= new StringFormat(defaultFlags);
			TitleMeasureStringFormat.Alignment			=
			TitleStringFormat.Alignment					= StringAlignment.Center;
			TitleMeasureStringFormat.LineAlignment		= 
			TitleStringFormat.LineAlignment				= StringAlignment.Center;
			TitleStringFormat.Trimming					= StringTrimming.EllipsisCharacter;
			TitleMeasureStringFormat.Trimming			= StringTrimming.None;

			CenterTextStringFormat						= new StringFormat(defaultFlags);
			CenterMeasureTextStringFormat				= new StringFormat(defaultFlags);
			CenterMeasureTextStringFormat.Alignment		= 
			CenterTextStringFormat.Alignment			= StringAlignment.Center;
			CenterMeasureTextStringFormat.LineAlignment = 
			CenterTextStringFormat.LineAlignment		= StringAlignment.Center;
			CenterTextStringFormat.Trimming				= StringTrimming.EllipsisCharacter;
			CenterMeasureTextStringFormat.Trimming		= StringTrimming.None;

			LeftTextStringFormat						= new StringFormat(defaultFlags);
			LeftMeasureTextStringFormat					= new StringFormat(defaultFlags);
			LeftMeasureTextStringFormat.Alignment		= 
			LeftTextStringFormat.Alignment				= StringAlignment.Near;
			LeftMeasureTextStringFormat.LineAlignment	= 
			LeftTextStringFormat.LineAlignment			= StringAlignment.Center;
            LeftTextStringFormat.Trimming               = StringTrimming.None; // EllipsisCharacter;
			LeftMeasureTextStringFormat.Trimming		= StringTrimming.None;

			RightTextStringFormat						= new StringFormat(defaultFlags);
			RightMeasureTextStringFormat				= new StringFormat(defaultFlags);
			RightMeasureTextStringFormat.Alignment		= 
			RightTextStringFormat.Alignment				= StringAlignment.Far;
			RightMeasureTextStringFormat.LineAlignment	= 
			RightTextStringFormat.LineAlignment			= StringAlignment.Center;
			RightTextStringFormat.Trimming				= StringTrimming.EllipsisCharacter;
			RightMeasureTextStringFormat.Trimming		= StringTrimming.None;
		}
	}
}
