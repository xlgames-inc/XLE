//*****************************************************************************
//
//  File:       ColorHelper.cs
//
//  Contents:   Class which supply color related helper functions for
//              transforming from one color format to the other
//
//*****************************************************************************

using System;
using System.Drawing;

namespace RibbonLib
{
    internal struct HSL
    {
        public double H;
        public double S;
        public double L;
    }

    internal struct HSB
    {
        public byte H;
        public byte S;
        public byte B;
    }

    internal static class ColorHelper
    {
        // based on http://www.geekymonkey.com/Programming/CSharp/RGB2HSL_HSL2RGB.htm
        // Given H,S,L in range of 0-1
        // Returns a Color (RGB struct) in range of 0-255
        public static Color HSL2RGB(HSL hsl)
        {
            double v;
            double r, g, b;

            r = hsl.L;   // default to gray
            g = hsl.L;
            b = hsl.L;
            v = (hsl.L <= 0.5) ? (hsl.L * (1.0 + hsl.S)) : (hsl.L + hsl.S - hsl.L * hsl.S);
            if (v > 0)
            {
                double m;
                double sv;
                int sextant;
                double fract, vsf, mid1, mid2;

                m = hsl.L + hsl.L - v;
                sv = (v - m) / v;
                hsl.H *= 6.0;
                sextant = (int)hsl.H;
                fract = hsl.H - sextant;
                vsf = v * sv * fract;
                mid1 = m + vsf;
                mid2 = v - vsf;

                switch (sextant)
                {
                    case 0:
                        r = v;
                        g = mid1;
                        b = m;
                        break;

                    case 1:
                        r = mid2;
                        g = v;
                        b = m;
                        break;

                    case 2:
                        r = m;
                        g = v;
                        b = mid1;
                        break;

                    case 3:
                        r = m;
                        g = mid2;
                        b = v;
                        break;

                    case 4:
                        r = mid1;
                        g = m;
                        b = v;
                        break;

                    case 5:
                        r = v;
                        g = m;
                        b = mid2;
                        break;
                }
            }

            return Color.FromArgb(Convert.ToByte(r * 255.0f), Convert.ToByte(g * 255.0f), Convert.ToByte(b * 255.0f));
        }

        // Given a Color (RGB Struct) in range of 0-255
        // Return H,S,L in range of 0-1
        public static HSL RGB2HSL(Color rgb)
        {
            HSL hsl;

            double r = rgb.R / 255.0;
            double g = rgb.G / 255.0;
            double b = rgb.B / 255.0;
            double v;
            double m;
            double vm;
            double r2, g2, b2;

            hsl.H = 0; // default to black
            hsl.S = 0;
            hsl.L = 0;
            v = Math.Max(r, g);
            v = Math.Max(v, b);
            m = Math.Min(r, g);
            m = Math.Min(m, b);
            hsl.L = (m + v) / 2.0;

            if (hsl.L <= 0.0)
            {
                return hsl;
            }

            vm = v - m;
            hsl.S = vm;

            if (hsl.S > 0.0)
            {
                hsl.S /= (hsl.L <= 0.5) ? (v + m) : (2.0 - v - m);
            }
            else
            {
                return hsl;
            }

            r2 = (v - r) / vm;
            g2 = (v - g) / vm;
            b2 = (v - b) / vm;

            if (r == v)
            {
                hsl.H = (g == m ? 5.0 + b2 : 1.0 - g2);
            }
            else if (g == v)
            {
                hsl.H = (b == m ? 1.0 + r2 : 3.0 - b2);
            }
            else
            {
                hsl.H = (r == m ? 3.0 + g2 : 5.0 - r2);
            }

            hsl.H /= 6.0;

            return hsl;
        }

        public static HSB HSL2HSB(HSL hsl)
        {
            HSB hsb;

            hsb.H = (byte)(255.0 * hsl.H);
            hsb.S = (byte)(255.0 * hsl.S);
            if ((0.1793 <= hsl.L) && (hsl.L <= 0.9821))
            {
                hsb.B = (byte)(257.7 + 149.9 * Math.Log(hsl.L));
            }
            else
            {
                hsb.B = 0;
            }

            return hsb;
        }

        public static uint HSB2Uint(HSB hsb)
        {
            return (uint)(hsb.H | (hsb.S << 8) | (hsb.B << 16));
        }
    }
}
