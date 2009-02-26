/*
 * Copyright (c) 2002-2006 Milan Cutka
 *               2007-2009 h.yamagata
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "stdafx.h"
#include "Tsubreader.h"
#include "TsubtitleText.h"
#include "Tfont.h"
#include "TfontSettings.h"
#include "IffdshowBase.h"
#include "IffdshowDecVideo.h"
#include "TfontManager.h"
#include "simd.h"
#include "Tconfig.h"
#include "ffdebug.h"
#include "postproc/swscale.h"
#include "Tlibmplayer.h"
#include "TwordWrap.h"
#include <mbstring.h>
#include "TsubreaderMplayer.h"
#pragma warning(disable:4244)

//============================ TrenderedSubtitleWordBase =============================
TrenderedSubtitleWordBase::~TrenderedSubtitleWordBase()
{
 if (own)
  for (int i=0;i<3;i++)
   {
    aligned_free(bmp[i]);
    aligned_free(msk[i]);
    aligned_free(outline[i]);
    aligned_free(shadow[i]);
   }
}

//============================== TrenderedTextSubtitleWord ===============================

// custom copy constructor for karaoke
TrenderedTextSubtitleWord::TrenderedTextSubtitleWord(
                       const TrenderedTextSubtitleWord &parent,
                       bool senondaryColor
                       ):
 TrenderedSubtitleWordBase(true),
 prefs(parent.prefs)
{
 *this = parent;
 secondaryColoredWord = NULL;
 bmp[0]     = (unsigned char*)aligned_malloc(dx[0] * dy[0] + 16, 16);
 bmp[1]     = (unsigned char*)aligned_malloc(dx[1] * dy[1] + 16, 16);
 outline[0] = (unsigned char*)aligned_malloc(dx[0] * dy[0] + 16, 16);
 outline[1] = (unsigned char*)aligned_malloc(dx[1] * dy[1] + 16, 16);
 shadow[0]  = (unsigned char*)aligned_malloc(dx[0] * dy[0] + 16, 16);
 shadow[1]  = (unsigned char*)aligned_malloc(dx[1] * dy[1] + 16, 16);
 msk[0]     = (unsigned char*)aligned_malloc(dx[0] * dy[0] + 16, 16);

 memcpy(bmp[0], parent.bmp[0], dx[0] * dy[0]);
 memcpy(bmp[1], parent.bmp[1], dx[1] * dy[1]);
 if (props.karaokeMode == TSubtitleProps::KARAOKE_ko)
  {
   memset(outline[0], 0, dx[0] * dy[0]);
   memset(outline[1], 0, dx[1] * dy[1]);
   memset(shadow[0] , 0, dx[0] * dy[0]);
   memset(shadow[1] , 0, dx[1] * dy[1]);
   m_outlineYUV.A = 0;
  }
 else
  {
   memcpy(outline[0], parent.outline[0], dx[0] * dy[0]);
   memcpy(outline[1], parent.outline[1], dx[1] * dy[1]);
   memcpy(shadow[0] , parent.shadow[0] , dx[0] * dy[0]);
   memcpy(shadow[1] , parent.shadow[1] , dx[1] * dy[1]);
  }

 if (parent.msk[1])
  {
   msk[1] = (unsigned char*)aligned_malloc(dx[1] * dy[1] + 16, 16);
   memset(msk[1], 0, dx[1] * dy[1]);
  }
 m_bodyYUV = YUVcolorA(props.SecondaryColour,props.SecondaryColourA);
 oldFader = 0;
 updateMask(1 << 16, 2);
}

// full rendering
TrenderedTextSubtitleWord::TrenderedTextSubtitleWord(
                       HDC hdc,
                       const wchar_t *s0,
                       size_t strlens,
                       const YUVcolorA &YUV,
                       const YUVcolorA &outlineYUV,
                       const YUVcolorA &shadowYUV,
                       const TrenderedSubtitleLines::TprintPrefs &Iprefs,
                       LOGFONT lf,
                       double xscale,
                       TSubtitleProps Iprops,
                       unsigned int gdi_font_scale,
                       unsigned int GDI_rendering_window):
 TrenderedSubtitleWordBase(true),
 props(Iprops),
 m_bodyYUV(YUV),
 m_outlineYUV(outlineYUV),
 m_shadowYUV(shadowYUV),
 prefs(Iprefs),
 secondaryColoredWord(NULL),
 dstOffset(0),
 oldBodyYUVa(256),
 oldOutlineYUVa(256)
{
 csp=prefs.csp & FF_CSPS_MASK;
 strings s1;
 strtok(ffstring(s0,strlens).c_str(),L"\t",s1);
 SIZE sz;sz.cx=sz.cy=0;ints cxs;
 for (strings::iterator s=s1.begin();s!=s1.end();s++)
  {
   SIZE sz0;
   GetTextExtentPoint32W(hdc,s->c_str(),(int)s->size(),&sz0);
   sz.cx+=sz0.cx;
   if (s+1!=s1.end())
    {
     int tabsize=prefs.tabsize*sz0.cy;
     int newpos=(sz.cx/tabsize+1)*tabsize;
     sz0.cx+=newpos-sz.cx;
     sz.cx=newpos;
    }
   cxs.push_back(sz0.cx);
   sz.cy=std::max(sz.cy,sz0.cy);
  }
 OUTLINETEXTMETRIC otm;
 SIZE sz1=sz;
 if (GetOutlineTextMetrics(hdc,sizeof(otm),&otm))
  {
   baseline=otm.otmTextMetrics.tmAscent;
   if (otm.otmItalicAngle)
    sz1.cx += ff_abs(LONG(sz1.cy*sin(otm.otmItalicAngle*M_PI/1800)));
   else
    if (otm.otmTextMetrics.tmItalic)
     sz1.cx+=sz1.cy*0.35;
   m_shadowSize = getShadowSize(otm.otmTextMetrics.tmHeight, gdi_font_scale);
  }
 else
  { // non true-type
   baseline=sz1.cy*0.8;
   m_shadowSize = getShadowSize(lf.lfHeight, gdi_font_scale);
   if (lf.lfItalic)
    sz1.cx+=sz1.cy*0.35;
  }
 dx[0] = ((sz1.cx + GDI_rendering_window) / gdi_font_scale + 1) * gdi_font_scale;
 dy[0] = sz1.cy + GDI_rendering_window;
 unsigned char *bmp16=(unsigned char*)aligned_calloc3(dx[0] * size_of_rgb32,dy[0], 32, 16);
 HBITMAP hbmp=CreateCompatibleBitmap(hdc,dx[0],dy[0]);
 HGDIOBJ old=SelectObject(hdc,hbmp);
 RECT r={0,0,dx[0],dy[0]};
 FillRect(hdc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
 SetTextColor(hdc,RGB(255,255,255));
 SetBkColor(hdc,RGB(0,0,0));
 int x=GDI_rendering_window/2;
 ints::const_iterator cx=cxs.begin();
 for (strings::const_iterator s=s1.begin();s!=s1.end();s++,cx++)
  {
   const char *t=(const char *)s->c_str();
   int sz=(int)s->size();
   TextOutW(hdc,x,GDI_rendering_window/2,s->c_str(),sz/*(int)s->size()*/);
   x+=*cx;
  }
 if (gdi_font_scale == 4)
  drawShadow<4>(hdc,hbmp,bmp16,old,xscale,sz,gdi_font_scale);  // sharp and fast, good for OSD.
 else
  drawShadow<16>(hdc,hbmp,bmp16,old,xscale,sz,gdi_font_scale); // anti aliased, good for subtitles.
}
template<int GDI_rendering_window> void TrenderedTextSubtitleWord::drawShadow(
      HDC hdc,
      HBITMAP hbmp,
      unsigned char *bmp16,
      HGDIOBJ old,
      double xscale,
      const SIZE &sz,
      unsigned int gdi_font_scale
      )
{
 // if GDI_rendering_window > gdi_font_scale, blur is applied.
 // gdi_font_scale is set by the constructor of Tfont.
 m_outlineWidth=1;
 double outlineWidth_double = prefs.outlineWidth;
 //if (props.refResY && prefs.clipdy)
 // outlineWidth_double = outlineWidth_double * prefs.clipdy / props.refResY;

 if (!prefs.opaqueBox)
  {
   if (csp==FF_CSP_420P && outlineWidth_double < 0.6 && !m_bodyYUV.isGray())
    {
     m_outlineWidth=1;
     outlineWidth_double=0.6;
     m_outlineYUV=0;
    }
   else
    {
     m_outlineWidth=int(outlineWidth_double);
     if ((double)m_outlineWidth < outlineWidth_double)
      m_outlineWidth++;
    }
  }
 else
  m_outlineWidth = 0;

 if (outlineWidth_double < 1.0 && outlineWidth_double > 0)
  outlineWidth_double = 0.5 + outlineWidth_double/2.0;

 BITMAPINFO bmi;
 bmi.bmiHeader.biSize=sizeof(bmi.bmiHeader);
 bmi.bmiHeader.biWidth=dx[0];
 bmi.bmiHeader.biHeight=-1*dy[0];
 bmi.bmiHeader.biPlanes=1;
 bmi.bmiHeader.biBitCount=32;
 bmi.bmiHeader.biCompression=BI_RGB;
 bmi.bmiHeader.biSizeImage=dx[0]*dy[0];
 bmi.bmiHeader.biXPelsPerMeter=75;
 bmi.bmiHeader.biYPelsPerMeter=75;
 bmi.bmiHeader.biClrUsed=0;
 bmi.bmiHeader.biClrImportant=0;
 GetDIBits(hdc,hbmp,0,dy[0],bmp16,&bmi,DIB_RGB_COLORS);  // copy bitmap, get it in bmp16 (RGB32).
 SelectObject(hdc,old);
 DeleteObject(hbmp);

 if (Tconfig::cpu_flags&FF_CPU_MMXEXT)
  {
   YV12_lum2chr_min=YV12_lum2chr_min_mmx2;
   YV12_lum2chr_max=YV12_lum2chr_max_mmx2;
  }
 else
  {
   YV12_lum2chr_min=YV12_lum2chr_min_mmx;
   YV12_lum2chr_max=YV12_lum2chr_max_mmx;
  }
#ifndef WIN64
 if (Tconfig::cpu_flags&FF_CPU_SSE2)
  {
#endif
   alignXsize=16;
   TtextSubtitlePrintY=TtextSubtitlePrintY_sse2;
   TtextSubtitlePrintUV=TtextSubtitlePrintUV_sse2;
#ifndef WIN64
  }
 else
  {
   alignXsize=8;
   TtextSubtitlePrintY=TtextSubtitlePrintY_mmx;
   TtextSubtitlePrintUV=TtextSubtitlePrintUV_mmx;
  }
#endif
 unsigned int _dx,_dy,_dxCore,_dyCore;
 topOverhang=getTopOverhang();
 bottomOverhang=getBottomOverhang();
 leftOverhang=getLeftOverhang();
 rightOverhang=getRightOverhang();
 _dx      = xscale * dx[0] / (size_of_rgb32 * 100) + leftOverhang + rightOverhang;
 _dxCore  = xscale * dx[0] / (size_of_rgb32 * 100) + leftOverhang + m_outlineWidth;
 _dy      = dy[0] / gdi_font_scale + topOverhang + bottomOverhang;
 _dyCore  = dy[0] / gdi_font_scale + topOverhang + m_outlineWidth;
 dxCharY  = xscale * sz.cx / (gdi_font_scale * 100);
 dyCharY  = sz.cy / gdi_font_scale;
 baseline = baseline / gdi_font_scale + GDI_rendering_window/2;

 unsigned int al=csp==FF_CSP_420P ? alignXsize : 8; // swscaler requires multiple of 8.
 _dx=((_dx+al-1)/al)*al;
 if (_dx < 16) _dx=16;
 if (csp==FF_CSP_420P)
  _dy=((_dy+1)/2)*2;
 stride_t extra_dx=_dx + m_outlineWidth*2; // add margin to simplify the outline drawing process.
 stride_t extra_dy=_dy + m_outlineWidth*2;
 extra_dx=((extra_dx+7)/8)*8;     // align for swscaler
 bmp[0]=(unsigned char*)aligned_calloc3(extra_dx,extra_dy,16,16);
 msk[0]=(unsigned char*)aligned_calloc3(_dx,_dy,16,16);
 outline[0]=(unsigned char*)aligned_calloc3(_dx,_dy,16,16);
 shadow[0]=(unsigned char*)aligned_calloc3(_dx,_dy,16,16);

 // Here we scale to 1/gdi_font_scale.
 // For OSD, gdi_font_scale is 4. The simplest way is to average 4 x 4.
 // But in that case, it will have only 16 gradation becasue the bitmap from GDI has only 0 or 0xffffff.
 // 4x5 for OSD, 16x16 for subtitles seems to look nice.
 unsigned int xstep = xscale == 100 ?
                          gdi_font_scale * 65536 :
                          gdi_font_scale * 100 * 65536 / xscale;
 unsigned int gdi_rendering_window_width = std::max<unsigned int>(
                      xscale == 100 ?
                          GDI_rendering_window :
                          GDI_rendering_window * 100 / xscale
                          , 1);
 // coeff calculation
 // To averave gdi_rendering_window_width * GDI_rendering_window pixels, add them all and
 // multiply (65536 / (gdi_rendering_window_width * GDI_rendering_window))
 // and shift right 16 bits is just fine.
 // One problem, it make the body a little thin and darker, if blur is applied (GDI_rendering_window > gdi_font_scale).
 // To avoid this, for subtitles, if only one of the overhanging edge is not filled, consider it is fully filled.
 unsigned int coeff;
 if (GDI_rendering_window == 4) // OSD
  coeff = 65536.0 / (gdi_rendering_window_width * 5);
 else // subtitles
  {
   if (GDI_rendering_window > gdi_font_scale)
    coeff = 65536.0 / ((gdi_rendering_window_width - (gdi_rendering_window_width - (gdi_font_scale * 100 / xscale))/2) * GDI_rendering_window);
   else
    coeff = 65536.0 / (gdi_rendering_window_width * GDI_rendering_window);
  }
 int dx0_mult_4 = dx[0] * size_of_rgb32;
 unsigned int xstep_sse2 = xstep * 8;
 unsigned int startx = (GDI_rendering_window/2 << 16) + xstep;
 unsigned int endx = (dx[0] - GDI_rendering_window/2) << 16;
 __m128i xmm0,xmm1,xmm2,xmm3,xmm_sum,xmm_000000ff,xmm_00000000;
 if (Tconfig::cpu_flags & FF_CPU_SSE2 && GDI_rendering_window == 16)
  {
   xmm_000000ff = _mm_set1_epi32(0xff);
   pxor(xmm_00000000,xmm_00000000);
  }
 for (unsigned int y = GDI_rendering_window/2 ; y < dy[0] - (GDI_rendering_window == 4 ? 3 : GDI_rendering_window/2) ; y += gdi_font_scale)
  {
   unsigned char *dstBmpY = bmp[0] + (y/gdi_font_scale + topOverhang + m_outlineWidth) * extra_dx + leftOverhang + m_outlineWidth;
   unsigned int x = startx;
   const unsigned char *bmp16srcLineStart = bmp16 + ((y - GDI_rendering_window/2) * dx[0]) * size_of_rgb32;
   const unsigned char *bmp16srcEnd;
   if (GDI_rendering_window == 4)
    bmp16srcEnd = bmp16srcLineStart + (GDI_rendering_window+1) * dx0_mult_4;
   else
    bmp16srcEnd = bmp16srcLineStart + GDI_rendering_window * dx0_mult_4;
   for (; x < endx ; x += xstep, dstBmpY++)
    {
     unsigned int sum;
     const unsigned char *bmp16src = bmp16srcLineStart + ((x >> 16) - GDI_rendering_window/2) * size_of_rgb32;

     if (xscale == 100)
      {
       if (Tconfig::cpu_flags & FF_CPU_SSE2 && GDI_rendering_window == 16)
        {
         pxor(xmm_sum, xmm_sum);
         for (; bmp16src < bmp16srcEnd ; bmp16src += dx0_mult_4)
          {
           xmm0 = _mm_loadu_si128((__m128i *)bmp16src);
           xmm1 = _mm_loadu_si128((__m128i *)(bmp16src+16));
           pand(xmm0,xmm_000000ff);
           xmm2 = _mm_loadu_si128((__m128i *)(bmp16src+32));
           pand(xmm1,xmm_000000ff);
           paddd(xmm_sum,xmm0);
           xmm3 = _mm_loadu_si128((__m128i *)(bmp16src+48));
           pand(xmm2,xmm_000000ff);
           paddd(xmm_sum,xmm1);
           pand(xmm3,xmm_000000ff);
           paddd(xmm_sum,xmm2);
           paddd(xmm_sum,xmm3);
          }

         sum = _mm_cvtsi128_si32(xmm_sum);
         xmm_sum = _mm_srli_si128(xmm_sum, size_of_rgb32);
         sum += _mm_cvtsi128_si32(xmm_sum);
         xmm_sum = _mm_srli_si128(xmm_sum, size_of_rgb32);
         sum += _mm_cvtsi128_si32(xmm_sum);
         xmm_sum = _mm_srli_si128(xmm_sum, size_of_rgb32);
         sum += _mm_cvtsi128_si32(xmm_sum);
        }
       else
        {
         sum = 0;
         for (; bmp16src < bmp16srcEnd ; bmp16src += dx0_mult_4)
          {
           // a bit of optimization: Only one if block will be compiled. Loops are unrolled.
           if (GDI_rendering_window == 4)
            sum += bmp16src[0] + bmp16src[4] + bmp16src[8] + bmp16src[12];
           else if (GDI_rendering_window == 16)
            sum += bmp16src[0] + bmp16src[4] + bmp16src[8] + bmp16src[12] + bmp16src[16] + bmp16src[20] + bmp16src[24] + bmp16src[28]
                 + bmp16src[32] + bmp16src[36] + bmp16src[40] + bmp16src[44] + bmp16src[48] + bmp16src[52] + bmp16src[56] + bmp16src[60];
           else
            for (unsigned int i = 0 ; i < size_of_rgb32 * GDI_rendering_window ; i += size_of_rgb32) // not used
             sum += bmp16src[i];
          }
        }

       sum = (sum * coeff) >> 16;
       *dstBmpY = (unsigned char)std::min<unsigned int>(sum,255);
      }
     else
      {
       sum = 0;
       for (; bmp16src < bmp16srcEnd ; bmp16src += dx0_mult_4)
        {
         for (unsigned int i = 0 ; i < size_of_rgb32 * gdi_rendering_window_width ; i += size_of_rgb32) // not used
          sum += bmp16src[i];
        }
       sum = (sum * coeff) >> 16;
       *dstBmpY = (unsigned char)std::min<unsigned int>(sum,255);
      }

    }
  }

 if (prefs.blur)
  {
   int startx=leftOverhang + m_outlineWidth - 1;
   int starty=topOverhang + m_outlineWidth - 1;
   int endy=extra_dy - bottomOverhang - m_outlineWidth+1;
   int endx=extra_dx - rightOverhang - m_outlineWidth+1;
   bmp[0]=blur(bmp[0], extra_dx, extra_dy, startx, starty, endx, endy, true);
  }

 aligned_free(bmp16);

 dx[0]=_dx;dy[0]=_dy;

 // Prepare matrix for outline calculation
 short *matrix=NULL;
 unsigned int matrixSizeH = ((m_outlineWidth*2+8)/8)*8; // 2 bytes for one.
 unsigned int matrixSizeV = m_outlineWidth*2+1;
 if (m_outlineWidth>0)
  {
   double r_cutoff=1.5;
   if (outlineWidth_double<4.5)
    r_cutoff=outlineWidth_double/3.0;
   double r_mul=512.0/r_cutoff;
   matrix=(short*)aligned_calloc(matrixSizeH*2,matrixSizeV,16);
   for (int y = -m_outlineWidth ; y <= m_outlineWidth ; y++)
    for (int x = -m_outlineWidth ; x <= m_outlineWidth ; x++)
     {
      int pos=(y + m_outlineWidth)*matrixSizeH+x+m_outlineWidth;
      double r=0.5+outlineWidth_double-sqrt(double(x*x+y*y));
      if (r>r_cutoff)
       matrix[pos]=512;
      else if (r>0)
       matrix[pos]=r*r_mul;
     }
  }

 if (prefs.opaqueBox)
  memset(msk[0],255,dx[0]*dy[0]);
 else if (m_outlineWidth)
  {
   // Prepare outline
   if (Tconfig::cpu_flags&FF_CPU_SSE2
#ifndef WIN64
       && m_outlineWidth>=2
#endif
      )
    {
     size_t matrixSizeH_sse2=matrixSizeH>>3;
     size_t srcStrideGap=extra_dx-matrixSizeH;
     for (unsigned int y = topOverhang - m_outlineWidth ; y < _dyCore ; y++)
      for (unsigned int x = leftOverhang - m_outlineWidth ; x < _dxCore ; x++)
       {
        unsigned int sum=fontPrepareOutline_sse2(bmp[0]+extra_dx*y+x,srcStrideGap,matrix,matrixSizeH_sse2,matrixSizeV) >> 9;
        msk[0][_dx*y+x]=sum>255 ? 255 : sum;
       }
    }
#ifndef WIN64
   else if (Tconfig::cpu_flags&FF_CPU_MMX)
    {
     size_t matrixSizeH_mmx=(matrixSizeV+3)/4;
     size_t srcStrideGap=extra_dx-matrixSizeH_mmx*4;
     size_t matrixGap=matrixSizeH_mmx & 1 ? 8 : 0;
     for (unsigned int y = topOverhang - m_outlineWidth ; y < _dyCore ; y++)
      for (unsigned int x = leftOverhang - m_outlineWidth ; x < _dxCore ; x++)
       {
        unsigned int sum=fontPrepareOutline_mmx(bmp[0]+extra_dx*y+x,srcStrideGap,matrix,matrixSizeH_mmx,matrixSizeV,matrixGap) >> 9;
        msk[0][_dx*y+x]=sum>255 ? 255 : sum;
       }
    }
#endif
   else
    {
     for (unsigned int y = topOverhang - m_outlineWidth ; y < _dyCore ; y++)
      for (unsigned int x = leftOverhang - m_outlineWidth ; x < _dxCore ; x++)
       {
        unsigned char *srcPos=bmp[0]+extra_dx*y+x; // (x-outlineWidth,y-outlineWidth)
        unsigned int sum=0;
        for (unsigned int yy=0;yy<matrixSizeV;yy++,srcPos+=extra_dx-matrixSizeV)
         for (unsigned int xx=0;xx<matrixSizeV;xx++,srcPos++)
          {
           sum+=(*srcPos)*matrix[matrixSizeH*yy+xx];
          }
        sum>>=9;
        msk[0][_dx*y+x]=sum>255 ? 255 : sum;
       }
    }

   // remove the margin that we have just added.
   for (unsigned int y=0;y<_dy;y++)
    memcpy(bmp[0]+_dx*y,bmp[0]+extra_dx*(y + m_outlineWidth) + m_outlineWidth, _dx);

   if (prefs.outlineBlur || (prefs.shadowMode==0 && m_shadowSize>0)) // blur outline and msk
    msk[0]=blur(msk[0], _dx, _dy, 0, 0, _dx, _dy, false);
  }
 else // m_outlineWidth==0
  memcpy(msk[0],bmp[0],_dx*_dy);

 // Draw outline.
 unsigned int count=_dx*_dy;
 for (unsigned int c=0;c<count;c++)
  {
   int b=bmp[0][c];
   int o=msk[0][c]-b;
   if (o>0)
    outline[0][c]=o;//*(255-b)>>8;
  }
 m_shadowMode=prefs.shadowMode;

 if (csp==FF_CSP_420P)
  {
   dx[1]=dx[0]>>1;
   dy[1]=dy[0]>>1;
   dx[1]=(dx[1]/alignXsize+1)*alignXsize;
   bmp[1]     = (unsigned char*)aligned_calloc(dx[1],dy[1],16);
   outline[1] = (unsigned char*)aligned_calloc(dx[1],dy[1],16);
   shadow[1]  = (unsigned char*)aligned_calloc(dx[1],dy[1],16);

   dx[2]=dx[0]>>1;
   dy[2]=dy[0]>>1;
   dx[2]=(dx[2]/alignXsize+1)*alignXsize;
  }
 else
  {
   //RGB32
   dx[1]=dx[0] * size_of_rgb32;
   dy[1]=dy[0];
   bmp[1]     = (unsigned char*)aligned_malloc(dx[1]*dy[1]+16,16);
   outline[1] = (unsigned char*)aligned_malloc(dx[1]*dy[1]+16,16);
   shadow[1]  = (unsigned char*)aligned_malloc(dx[1]*dy[1]+16,16);
   msk[1]     = (unsigned char*)aligned_malloc(dx[1]*dy[1]+16,16);
  }
 updateMask();

 if (props.karaokeMode != TSubtitleProps::KARAOKE_NONE)
  secondaryColoredWord = new TrenderedTextSubtitleWord(*this, true);

 if (matrix)
  aligned_free(matrix);
}

void TrenderedTextSubtitleWord::updateMask(int fader, int create) const
{
 // shadowMode 0: glowing, 1:classic with gradient, 2: classic with no gradient, >=3: no shadow
 if (create == 0 && oldFader == fader)
  return;
 oldFader = fader;
 unsigned int _dx=dx[0];
 unsigned int _dy=dy[0];
 unsigned int count=_dx*_dy;
 unsigned int shadowSize = m_shadowSize;

 unsigned int bodyYUVa = m_bodyYUV.A * fader >> 16;
 unsigned int outlineYUVa = m_outlineYUV.A * fader >> 16;
 unsigned int shadowYUVa = m_shadowYUV.A * fader >> 16;
 if (bodyYUVa != 256 || outlineYUVa != 256 || create == 2 || bodyYUVa != oldBodyYUVa || outlineYUVa != oldOutlineYUVa)
  {
   oldBodyYUVa = bodyYUVa;
   oldOutlineYUVa = outlineYUVa;
   for (unsigned int c=0;c<count;c++)
    {
     msk[0][c]=(bmp[0][c] * bodyYUVa >> 8) + (outline[0][c] * outlineYUVa >> 8);
    }
  }

 unsigned char* mskptr;
 if (m_outlineWidth)
  mskptr=msk[0];
 else
  mskptr=bmp[0];

 unsigned int shadowAlpha = 255;
 if (shadowSize > 0 && (create || mskptr != bmp[0]))
  if (m_shadowMode == 0) //Gradient glowing shadow (most complex)
   {
    _mm_empty();
    if (_dx<shadowSize) shadowSize=_dx;
    if (_dy<shadowSize) shadowSize=_dy;
    unsigned int circle[1089]; // 1089=(16*2+1)^2
    if (shadowSize>16) shadowSize=16;
    int circleSize=shadowSize*2+1;
    for (int y=0;y<circleSize;y++)
     {
      for (int x=0;x<circleSize;x++)
       {
        unsigned int rx=ff_abs(x-(int)shadowSize);
        unsigned int ry=ff_abs(y-(int)shadowSize);
        unsigned int r=(unsigned int)sqrt((double)(rx*rx+ry*ry));
        if (r>shadowSize)
         circle[circleSize*y+x] = 0;
        else
         circle[circleSize*y+x] = shadowAlpha*(shadowSize+1-r)/(shadowSize+1);
       }
     }
    for (unsigned int y=0; y<_dy;y++)
     {
      int starty = y>=shadowSize ? 0 : shadowSize-y;
      int endy = y+shadowSize<_dy ? circleSize : _dy-y+shadowSize;
      for (unsigned int x=0; x<_dx;x++)
       {
        unsigned int pos = _dx*y+x;
        int startx = x>=shadowSize ? 0 : shadowSize-x;
        int endx = x+shadowSize<_dx ? circleSize : _dx-x+shadowSize;
        if (mskptr[pos] == 0) continue;
        for (int ry=starty; ry<endy;ry++)
         {
          for (int rx=startx; rx<endx;rx++)
           {
             unsigned int alpha = circle[circleSize*ry+rx];
             if (alpha)
              {
               unsigned int dstpos = _dx*(y+ry-shadowSize)+x+rx-shadowSize;
               unsigned int s = mskptr[pos] * alpha >> 8;
               if (shadow[0][dstpos]<s)
                shadow[0][dstpos] = (unsigned char)s;
              }
           }
         }
       }
     }
   }
  else if (m_shadowMode == 1) //Gradient classic shadow
   {
    unsigned int shadowStep = shadowAlpha/shadowSize;
    for (unsigned int y=0; y<_dy;y++)
     {
      for (unsigned int x=0; x<_dx;x++)
       {
        unsigned int pos = _dx*y+x;
        if (mskptr[pos] == 0) continue;

        unsigned int shadowAlphaGradient = shadowAlpha;
        for (unsigned int xx=1; xx<=shadowSize; xx++)
         {
          unsigned int s = mskptr[pos]*shadowAlphaGradient>>8;
          if (x + xx < _dx)
           {
            if (y+xx < _dy && shadow[0][_dx*(y+xx)+x+xx] <s)
             shadow[0][_dx*(y+xx)+x+xx] = s;
           }
          shadowAlphaGradient -= shadowStep;
         }
       }
     }
   }
  else if (m_shadowMode == 2) //Classic shadow
   {
    for (unsigned int y=shadowSize; y<_dy;y++)
     memcpy(shadow[0]+_dx*y+shadowSize,mskptr+_dx*(y-shadowSize),_dx-shadowSize);
   }

 // Preparation for each color space
 if (csp==FF_CSP_420P)
  {
   if (create == 1)
    {
     int isColorOutline=(m_outlineYUV.U!=128 || m_outlineYUV.V!=128);
     if (Tconfig::cpu_flags&FF_CPU_MMX || Tconfig::cpu_flags&FF_CPU_MMXEXT)
      {
       unsigned int edxAlign=(_dx & ~0xf) >> 1;
       unsigned int edx=_dx >> 1;
       for (unsigned int y=0 ; y<dy[1] ; y++)
        for (unsigned int x=0 ; x<edx ; x+=8)
         {
          if (x>=edxAlign)
           x=edx - 8;
          unsigned int lum0=2*y*_dx+x*2;
          unsigned int lum1=(2*y+1)*_dx+x*2;
          unsigned int chr=y*dx[1]+x;
          YV12_lum2chr_max(&bmp[0][lum0],&bmp[0][lum1],&bmp[1][chr]);
          if (isColorOutline)
           YV12_lum2chr_max(&outline[0][lum0],&outline[0][lum1],&outline[1][chr]);
          else
           YV12_lum2chr_min(&outline[0][lum0],&outline[0][lum1],&outline[1][chr]);
          YV12_lum2chr_min(&shadow[0][lum0],&shadow [0][lum1],&shadow [1][chr]);
         }
      }
     else
      {
       unsigned int _dx1=_dx/2;
       for (unsigned int y=0;y<dy[1];y++)
        for (unsigned int x=0;x<_dx1;x++)
         {
          unsigned int lum0=2*y*_dx+x*2;
          unsigned int lum1=(2*y+1)*_dx+x*2;
          unsigned int chr=y*dx[1]+x;
          bmp[1][chr]=std::max(std::max(std::max(bmp[0][lum0],bmp[0][lum0+1]),bmp[0][lum1]),bmp[0][lum1+1]);
          if (isColorOutline)
           outline[1][chr]=std::max(std::max(std::max(outline[0][lum0],outline[0][lum0+1]),outline[0][lum1]),outline[0][lum1+1]);
          else
           outline[1][chr]=std::min(std::min(std::min(outline[0][lum0],outline[0][lum0+1]),outline[0][lum1]),outline[0][lum1+1]);
          shadow[1][chr]=std::min(std::min(std::min(shadow[0][lum0],shadow[0][lum0+1]),shadow[0][lum1]),shadow[0][lum1+1]);
         }
      }
    }
  }
 else
  {
   //RGB32
   unsigned int xy=(_dx*_dy)>>2;

   #define Y2RGB(bmp)                                                              \
    DWORD *bmp##RGB=(DWORD *)bmp[1];                                               \
    unsigned char *bmp##Y=bmp[0];                                                  \
    for (unsigned int i=xy;i;bmp##RGB+=4,bmp ## Y+=4,i--)                          \
     {                                                                             \
      *(bmp##RGB)      =*bmp##Y<<16         | *bmp##Y<<8         | *bmp##Y;        \
      *(bmp##RGB+1)    =*(bmp##Y+1)<<16     | *(bmp##Y+1)<<8     | *(bmp##Y+1);    \
      *(bmp##RGB+2)    =*(bmp##Y+2)<<16     | *(bmp##Y+2)<<8     | *(bmp##Y+2);    \
      *(bmp##RGB+3)    =*(bmp##Y+3)<<16     | *(bmp##Y+3)<<8     | *(bmp##Y+3);    \
     }
   if (create == 1)
    {
     Y2RGB(bmp)
     Y2RGB(outline)
    }
   if (create)
    {
     Y2RGB(shadow)
    }
   Y2RGB(msk)
  }
 _mm_empty();
}

size_t TrenderedTextSubtitleWord::getMemorySize() const
{
    return 4 * (((dx[0] +  m_outlineWidth*2) * (dy[0] +  m_outlineWidth*2) + 32) + (dx[1] * dy[1] +32));
}

TrenderedTextSubtitleWord::~TrenderedTextSubtitleWord()
{
 if (secondaryColoredWord)
  delete secondaryColoredWord;
}

unsigned int TrenderedTextSubtitleWord::getShadowSize(LONG fontHeight, unsigned int gdi_font_scale)
{
 if (prefs.shadowSize==0 || prefs.shadowMode==3)
  return 0;
 if (prefs.shadowSize < 0) // SSA/ASS/ASS2
  {
   //if (prefs.clipdy && props.refResY)
   // return -1 * prefs.shadowSize * prefs.clipdy / props.refResY;
   //else
    return -1 * prefs.shadowSize;
  }

 unsigned int shadowSize = prefs.shadowSize*fontHeight/(gdi_font_scale * 45)+2.6;
 if (prefs.shadowMode==0)
  shadowSize*=0.6;
 else if (prefs.shadowMode==1)
  shadowSize/=1.4142;  // 1.4142 = sqrt(2.0)
 else if (prefs.shadowMode==2)
  shadowSize*=0.4;

 if (shadowSize==0)
  shadowSize = 1;
 if (shadowSize>16)
  shadowSize = 16;
 return shadowSize;
}

unsigned char* TrenderedTextSubtitleWord::blur(unsigned char *src,stride_t Idx,stride_t Idy,int startx,int starty,int endx, int endy, bool mild)
{
 /*
  *  Copied and modified from guliverkli, Rasterizer.cpp
  *
  *  Copyright (C) 2003-2006 Gabest
  *  http://www.gabest.org
  */
 unsigned char *dst=(unsigned char*)aligned_calloc3(Idx,Idy,16,16);
 int sx=startx <= 0 ? 1 : startx;
 int sy=starty <= 0 ? 1 : starty;
 int ex=endx >= Idx ? Idx-1 : endx;
 int ey=endy >= Idy ? Idy-1 : endy;

 if (mild)
  {
   for (int y=sy ; y < ey ; y++)
    for (int x=sx ; x < ex ; x++)
     {
      int pos=Idx*y+x;
      unsigned char *srcpos=src+pos;
      dst[pos] =   (srcpos[-1-Idx]   + (srcpos[-Idx] << 1) +  srcpos[+1-Idx]
                 + (srcpos[-1] << 1) + (srcpos[0]*20)      + (srcpos[+1] << 1)
                 +  srcpos[-1+Idx]   + (srcpos[+Idx] << 1) +  srcpos[+1+Idx]) >> 5;
     }
  }
 else
  {
   for (int y=sy ; y < ey ; y++)
    for (int x=sx ; x < ex ; x++)
     {
      int pos=Idx*y+x;
      unsigned char *srcpos=src+pos;
      dst[pos] =   (srcpos[-1-Idx]   + (srcpos[-Idx] << 1) +  srcpos[+1-Idx]
                 + (srcpos[-1] << 1) + (srcpos[0] << 2)    + (srcpos[+1] << 1)
                 +  srcpos[-1+Idx]   + (srcpos[+Idx] << 1) +  srcpos[+1+Idx]) >> 4;
     }
  }
 if (startx==0)
  for (int y=starty ; y<endy ; y++)
   dst[Idx*y]=src[Idx*y];
 if (endx==Idx-1)
  for (int y=starty ; y<endy ; y++)
   dst[Idx*y+endx]=src[Idx*y+endx];
 if (starty==0)
  memcpy(dst,src,Idx);
 if (endy==Idy-1)
  memcpy(dst+Idx*endy,src+Idx*endy,Idx);
 aligned_free(src);
 return dst;
}

unsigned int TrenderedTextSubtitleWord::getBottomOverhang(void)
{
 return m_shadowSize + m_outlineWidth;
}
unsigned int TrenderedTextSubtitleWord::getRightOverhang(void)
{
 return m_shadowSize + m_outlineWidth;
}
unsigned int TrenderedTextSubtitleWord::getTopOverhang(void)
{
 if (prefs.shadowMode==0)
  return m_shadowSize + m_outlineWidth;
 else
  return m_outlineWidth;
}
unsigned int TrenderedTextSubtitleWord::getLeftOverhang(void)
{
 return getTopOverhang();
}
int TrenderedTextSubtitleWord::get_baseline() const
{
 return baseline;
}
int TrenderedTextSubtitleWord::get_ascent64() const
{
 return props.m_ascent64;
}
int TrenderedTextSubtitleWord::get_descent64() const
{
 return props.m_descent64;
}

void TrenderedTextSubtitleWord::print(int startx, int starty, unsigned int sdx[3], int sdy[3], unsigned char *dstLn[3], const stride_t stride[3], const unsigned char *Ibmp[3], const unsigned char *Imsk[3],REFERENCE_TIME rtStart) const
{
 if (sdy[0]<=0 || sdy[1]<0)
  return;

 // karaoke: use secondaryColoredWord if not highlighted.
 if ((props.karaokeMode == TSubtitleProps::KARAOKE_k || props.karaokeMode == TSubtitleProps::KARAOKE_ko) && rtStart < props.karaokeStart && secondaryColoredWord)
  return secondaryColoredWord->print(startx, starty, sdx, sdy, dstLn, stride, Ibmp, Imsk, rtStart);

 int srcOffset = startx < 0 ? -startx : 0;
 if (props.karaokeMode == TSubtitleProps::KARAOKE_kf && secondaryColoredWord)
  {
   if (rtStart < props.karaokeStart)
    {
     secondaryColoredWord->dstOffset = 0;
     return secondaryColoredWord->print(startx, starty, sdx, sdy, dstLn, stride, Ibmp, Imsk, rtStart);
    }
   if (rtStart < props.karaokeStart + props.karaokeDuration && props.karaokeDuration)
    {
     unsigned int sdx2[3];
     int offset = (double)(rtStart - props.karaokeStart) / props.karaokeDuration * dx[0];
     if (offset < srcOffset)
      {
       sdx2[0] = sdx[0];
       sdx2[1] = sdx[1];
      }
     else
      {
       if ((int)sdx[0] > offset - srcOffset)
        {
         sdx2[0] = sdx[0] - offset + srcOffset;
         sdx2[1] = sdx2[0] >> prefs.shiftX[1];
        }
       else
        {
         sdx2[0] = 0;
         sdx2[1] = 0;
        }
      }
     int startx2 = -std::max(srcOffset, offset);
     secondaryColoredWord->dstOffset = std::max(offset - srcOffset, 0);
     secondaryColoredWord->print(startx2, starty, sdx2, sdy, dstLn, stride, Ibmp, Imsk, rtStart);
     sdx[0] -= sdx2[0];
     sdx[1] = sdx[0] >> prefs.shiftX[1];
    }
  }

 int srcOffsetUV = srcOffset >> 1;
 int srcOffsetRGB = srcOffset << 2;
 int dstOffsetUV = dstOffset >> 1;
 int dstOffsetRGB = dstOffset << 2;
 int startyUV = starty >> 1;
 int bodyYUVa = m_bodyYUV.A;
 int outlineYUVa = m_outlineYUV.A;
 int shadowYUVa = m_shadowYUV.A;
 if (props.isFad)
  {
   double fader = 1.0;
   if      (rtStart < props.fadeT1)
    fader = props.fadeA1 / 255.0;
   else if (rtStart < props.fadeT2)
    fader = (double)(rtStart - props.fadeT1) / (props.fadeT2 - props.fadeT1) * (props.fadeA2 - props.fadeA1) / 255.0 + props.fadeA1 / 255.0; 
   else if (rtStart < props.fadeT3)
    fader = props.fadeA2 / 255.0;
   else if (rtStart < props.fadeT4)
    fader = (double)(props.fadeT4 - rtStart) / (props.fadeT4 - props.fadeT3) * (props.fadeA2 - props.fadeA3) / 255.0 + props.fadeA3 / 255.0;
   else
    fader = props.fadeA3 / 255.0;
   bodyYUVa = bodyYUVa * fader;
   outlineYUVa = outlineYUVa * fader;
   shadowYUVa = shadowYUVa * fader;
   updateMask(int(fader * (1 << 16)), false);  // updateMask doesn't accept floating point because it use MMX.
  }
#ifdef WIN64
 if (Tconfig::cpu_flags&FF_CPU_SSE2)
  {
   unsigned char xmmregs[16*16];
   storeXmmRegs(xmmregs);
#else
 if (Tconfig::cpu_flags&(FF_CPU_SSE2|FF_CPU_MMX))
  {
#endif
   if (csp==FF_CSP_420P)
    {
     //YV12
     unsigned int halfAlingXsize=alignXsize>>1;
     unsigned short* colortbl=(unsigned short *)aligned_malloc(192,16);
     for (unsigned int i=0;i<halfAlingXsize;i++)
      {
       colortbl[i]   =(short)m_bodyYUV.Y;
       colortbl[i+8] =(short)m_bodyYUV.U;
       colortbl[i+16]=(short)m_bodyYUV.V;
       colortbl[i+24]=(short)bodyYUVa;
       colortbl[i+32]=(short)m_outlineYUV.Y;
       colortbl[i+40]=(short)m_outlineYUV.U;
       colortbl[i+48]=(short)m_outlineYUV.V;
       colortbl[i+56]=(short)outlineYUVa;
       colortbl[i+64]=(short)m_shadowYUV.Y;
       colortbl[i+72]=(short)m_shadowYUV.U;
       colortbl[i+80]=(short)m_shadowYUV.V;
       colortbl[i+88]=(short)shadowYUVa;
      }
     // Y
     const unsigned char *mask0=msk[0];
     //if (bodyYUVa!=256)
     // mask0=outline[0];

     int endx=sdx[0] & ~(alignXsize-1);
     for (int y=0 ; y < sdy[0] ; y++)
      if (y + starty >=0)
       for (int x=0 ; x < endx ; x+=alignXsize)
        {
         int srcPos=y * dx[0] + x + srcOffset;
         int dstPos=y * stride[0] + x + dstOffset;
         TtextSubtitlePrintY(&bmp[0][srcPos],&outline[0][srcPos],&shadow[0][srcPos],colortbl,&dstLn[0][dstPos],&msk[0][srcPos]);
        }
     if (endx < (int)sdx[0])
      {
       for (int y=0;y<sdy[0];y++)
        if (y + starty >=0)
         for (unsigned int x=endx;x<sdx[0];x++)
          {
           #define YV12_Y_FONTRENDERER                                              \
           int srcPos=y * dx[0] + x + srcOffset;                                    \
           int dstPos=y * stride[0] + x + dstOffset;                                \
           int s=shadowYUVa * shadow[0][srcPos] >> 8;                               \
           int d=((256-s) * dstLn[0][dstPos] >> 8) + (s * m_shadowYUV.Y >> 8);      \
           int o=outlineYUVa * outline[0][srcPos] >> 8;                             \
           int b=bodyYUVa *bmp[0][srcPos] >> 8;                                     \
           int m=msk[0][srcPos];                                                    \
               d=((256-m) * d >> 8) + (o * m_outlineYUV.Y >> 8);                    \
               dstLn[0][dstPos]=d + (b * m_bodyYUV.Y >> 8);

           YV12_Y_FONTRENDERER
          }
       }
     // UV
     const unsigned char *mask1=msk[1];
     //if (bodyYUVa!=256)
     // mask1=outline[1];

     endx=sdx[1] & ~(alignXsize-1);
     for (int y=0;y<sdy[1];y++)
      if (y + startyUV >= 0)
       for (int x=0 ; x < endx ; x+=alignXsize)
        {
         int srcPos=y * dx[1] + x + srcOffsetUV;
         int dstPos=y * stride[1] + x + dstOffsetUV;
         TtextSubtitlePrintUV(&bmp[1][srcPos],&outline[1][srcPos],&shadow[1][srcPos],colortbl,&dstLn[1][dstPos],&dstLn[2][dstPos]);
        }
     if (endx < (int)sdx[1])
      {
       for (int y=0;y<sdy[1];y++)
        if (y + startyUV >= 0)
         for (int x=0 ; x < (int)sdx[1] ; x++)
          {
           #define YV12_UV_FONTRENDERER                                             \
           int srcPos=y * dx[1] + x + srcOffsetUV;                                  \
           int dstPos=y * stride[1] + x + dstOffsetUV;                              \
           /* U */                                                                  \
           int s=shadowYUVa * shadow[1][srcPos] >> 8;                               \
           int d=((256-s) * dstLn[1][dstPos] >> 8) + (s * m_shadowYUV.U >> 8);      \
           int o=outlineYUVa * outline[1][srcPos] >> 8;                             \
           int b=bodyYUVa * bmp[1][srcPos] >> 8;                                    \
               d=((256-o) * d >> 8) + (o * m_outlineYUV.U >> 8);                    \
               dstLn[1][dstPos]=((256-b) * d >> 8) + (b * m_bodyYUV.U >> 8);        \
           /* V */                                                                  \
               d=((256-s) * dstLn[2][dstPos] >> 8) + (s * m_shadowYUV.V >> 8);      \
               d=((256-o) * d >> 8) + (o * m_outlineYUV.V >> 8);                    \
               dstLn[2][dstPos]=((256-b) * d >> 8) + (b * m_bodyYUV.V >> 8);

           YV12_UV_FONTRENDERER
          }
      }
     aligned_free(colortbl);
    }
   else
    {
     //RGB32
     unsigned int halfAlingXsize=alignXsize>>1;
     unsigned short* colortbl=(unsigned short *)aligned_malloc(192,16);
     colortbl[ 0]=colortbl[ 4]=(short)m_bodyYUV.b;
     colortbl[ 1]=colortbl[ 5]=(short)m_bodyYUV.g;
     colortbl[ 2]=colortbl[ 6]=(short)m_bodyYUV.r;
     colortbl[ 3]=colortbl[ 7]=0;
     colortbl[32]=colortbl[36]=(short)m_outlineYUV.b;
     colortbl[33]=colortbl[37]=(short)m_outlineYUV.g;
     colortbl[34]=colortbl[38]=(short)m_outlineYUV.r;
     colortbl[35]=colortbl[39]=0;
     colortbl[64]=colortbl[68]=(short)m_shadowYUV.b;
     colortbl[65]=colortbl[69]=(short)m_shadowYUV.g;
     colortbl[66]=colortbl[70]=(short)m_shadowYUV.r;
     colortbl[67]=colortbl[71]=0;
     for (unsigned int i=0;i<halfAlingXsize;i++)
      {
       colortbl[i+24]=(short)bodyYUVa;
       colortbl[i+56]=(short)outlineYUVa;
       colortbl[i+88]=(short)shadowYUVa;
      }

     int endx2=sdx[0]*4;
     int endx=endx2 & ~(alignXsize-1);
     for (int y=0;y<sdy[0];y++)
      if (y + starty >=0)
       for (int x=0 ; x < endx ; x+=alignXsize)
        {
         int srcPos=y * dx[1] + x + srcOffsetRGB;
         int dstPos=y * stride[0] + x + dstOffsetRGB;
         TtextSubtitlePrintY(&bmp[1][srcPos],&outline[1][srcPos],&shadow[1][srcPos],colortbl,&dstLn[0][dstPos],&msk[1][srcPos]);
        }
     if (endx<endx2)
      {
       for (int y=0 ; y < sdy[1] ; y++)
        if (y + starty >=0)
         for (int x=endx ; x < endx2 ; x+=4)
          {
           #define RGBFONTRENDERER \
           int srcPos=y * dx[1] + x + srcOffsetRGB;                               \
           int dstPos=y * stride[0] + x + dstOffsetRGB;                           \
           /* B */                                                                \
           int s=shadowYUVa * shadow[1][srcPos] >> 8;                             \
           int d=((256-s) * dstLn[0][dstPos] >> 8) + (s * m_shadowYUV.b >> 8);    \
           int o=outlineYUVa * outline[1][srcPos] >> 8;                           \
           int b=bodyYUVa * bmp[1][srcPos] >> 8;                                  \
           int m=msk[1][srcPos];                                                  \
               d=((256-m) * d >> 8)+(o * m_outlineYUV.b >> 8);                    \
               dstLn[0][dstPos]=d + (b * m_bodyYUV.b >> 8);                       \
           /* G */                                                                \
               d=((256-s) * dstLn[0][dstPos+1] >> 8)+(s * m_shadowYUV.g >> 8);    \
               d=((256-m) * d >> 8)+(o * m_outlineYUV.g >> 8);                    \
               dstLn[0][dstPos + 1]=d + (b * m_bodyYUV.g >> 8);                   \
           /* R */                                                                \
               d=((256-s) * dstLn[0][dstPos+2] >> 8)+(s * m_shadowYUV.r >> 8);    \
               d=((256-m) * d >> 8)+(o * m_outlineYUV.r >> 8);                    \
               dstLn[0][dstPos + 2]=d + (b * m_bodyYUV.r >> 8);

           RGBFONTRENDERER
          }
      }
     aligned_free(colortbl);
    }
#ifdef WIN64
   restoreXmmRegs(xmmregs);
#endif
  }
 else
  {
   if (csp==FF_CSP_420P)
    {
     // YV12-Y
     const unsigned char *mask0=msk[0];
     //if (bodyYUVa!=256)
     // mask0=outline[0];

     for (int y=0 ; y < sdy[0] ; y++)
      if (y + starty >=0)
       for (int x=0 ; x < (int)sdx[0] ; x++)
        {
         YV12_Y_FONTRENDERER
        }
     const unsigned char *mask1=msk[1];
     if (bodyYUVa!=256)
      mask1=outline[1];

     for (int y=0 ; y < sdy[1] ; y++)
      if (y + startyUV >=0)
       for (int x=0 ; x < (int)sdx[1] ; x++)
        {
         YV12_UV_FONTRENDERER
        }
    }
   else
    {
     //RGB32
     for (int y=0 ; y < sdy[1] ; y++)
      if (y + starty >=0)
       for (int x=0 ; x < (int)sdx[0]*4 ; x+=4)
        {
         RGBFONTRENDERER
        }
    }
  }
 _mm_empty();
}

//============================== TrenderedSubtitleLine ===============================
unsigned int TrenderedSubtitleLine::width(void) const
{
 if (empty())
  return 0;
 unsigned int dx=0;
 for (const_iterator w=begin();w!=end();w++)
  dx+=(*w)->dxCharY;
 return dx;
}
unsigned int TrenderedSubtitleLine::height(void) const
{
 if (empty())
  return 0;
 int aboveBaseline=0,belowBaseline=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   aboveBaseline=std::max<int>(aboveBaseline,(*w)->get_baseline());
   belowBaseline=std::max<int>(belowBaseline,(*w)->dy[0]-(*w)->get_baseline());
  }
 return aboveBaseline+belowBaseline;
}
double TrenderedSubtitleLine::charHeight(void) const
{
 if (empty())
  return (double)(props.m_ascent64 + props.m_descent64)/16.0;
 int aboveBaseline=0,belowBaseline=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   aboveBaseline=std::max<int>(aboveBaseline,(*w)->get_ascent64());
   belowBaseline=std::max<int>(belowBaseline,(*w)->get_descent64());
  }
 return (double)(aboveBaseline + belowBaseline)/8.0;
}
unsigned int TrenderedSubtitleLine::baselineHeight(void) const
{
 if (empty())
  return 0;
 unsigned int aboveBaseline=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   aboveBaseline=std::max<unsigned int>(aboveBaseline,(*w)->get_baseline());
  }
 return aboveBaseline;
}
int TrenderedSubtitleLine::get_topOverhang(void) const
{
 if (empty())
  return 0;
 int baseline=baselineHeight();
 int topOverhang=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   topOverhang=std::min(topOverhang,baseline-(*w)->get_baseline()-(*w)->get_topOverhang());
  }
 return -topOverhang;
}
int TrenderedSubtitleLine::get_bottomOverhang(void) const
{
 if (empty())
  return 0;
 int baseline=baselineHeight();
 int bottomOverhang=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   bottomOverhang=std::max(bottomOverhang,(int)(*w)->dxCharY-(int)(*w)->get_baseline()+(*w)->get_bottomOverhang());
  }
 return bottomOverhang+baseline-charHeight();
}
int TrenderedSubtitleLine::get_leftOverhang(void) const
{
 if (empty())
  return 0;
 int dx=0;
 int leftOverhang=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   leftOverhang=std::min(leftOverhang,dx-(*w)->get_leftOverhang());
   dx+=(*w)->dxCharY;
  }
 return -leftOverhang;
}
int TrenderedSubtitleLine::get_rightOverhang(void) const
{
 if (empty())
  return 0;
 int dx=0;
 int rightOverhang=0;
 for (const_iterator w=begin();w!=end();w++)
  {
   dx+=(*w)->dxCharY;
   rightOverhang=std::max(rightOverhang,dx+(*w)->get_rightOverhang());
  }
 return rightOverhang-dx;
}
void TrenderedSubtitleLine::prepareKaraoke(void)
{
 if (!firstrun)
  return;
 firstrun = false;
 int sequenceWidth;
 REFERENCE_TIME karaokeStart = REFTIME_INVALID;
 for (iterator w = begin() ; w != end() ; w++)
  {
   if (((TrenderedTextSubtitleWord *)(*w))->props.karaokeNewWord)
    {
     sequenceWidth = (*w)->dxCharY;
     for (iterator s = w + 1 ; s != end() ; s++)
      {
       if (((TrenderedTextSubtitleWord *)(*s))->props.karaokeNewWord)
        break;
       else
        sequenceWidth += (*s)->dxCharY;
      }
     if (sequenceWidth <= 0) continue;
     if (karaokeStart == REFTIME_INVALID)
      karaokeStart = ((TrenderedTextSubtitleWord *)(*w))->props.karaokeStart;
     for (iterator s = w ; s != end() ; s++)
      {
       if (((TrenderedTextSubtitleWord *)(*s))->props.karaokeNewWord && s != w)
        break;
       ((TrenderedTextSubtitleWord *)(*s))->props.karaokeDuration *= (double)(*s)->dxCharY / sequenceWidth;
       ((TrenderedTextSubtitleWord *)(*s))->props.karaokeStart = karaokeStart;
       karaokeStart += ((TrenderedTextSubtitleWord *)(*s))->props.karaokeDuration;
      }
    }
  }
}
void TrenderedSubtitleLine::print(int startx,int starty,const TrenderedSubtitleLines::TprintPrefs &prefs,unsigned int prefsdx,unsigned int prefsdy) const
{
 int baseline=baselineHeight();
 for (const_iterator w=begin();w!=end() && startx<(int)prefsdx;startx+=(*w)->dxCharY,w++)
  {
   const unsigned char *msk[3],*bmp[3];
   unsigned char *dstLn[3];
   int x[3];
   unsigned int dx[3];
   int dy[3];
   for (int i=0;i<3;i++)
    {
     x[i]=startx>>prefs.shiftX[i];
     msk[i]=(*w)->msk[i];
     bmp[i]=(*w)->bmp[i];
     if (prefs.align!=ALIGN_FFDSHOW && x[i]<0)
      {
       msk[i]+=-x[i];
       bmp[i]+=-x[i];
      }
     int sy=(starty+baseline-(*w)->get_baseline())>>prefs.shiftY[i];
     dy[i]=std::min((int(prefsdy)>>prefs.shiftY[i])-sy,int((*w)->dy[i]));
     dstLn[i]=prefs.dst[i] + int(sy * prefs.stride[i]);
     if (x[i]>0)
      dstLn[i]+=x[i]*prefs.cspBpp;

     if (x[i]+(*w)->dx[i]>(prefsdx>>prefs.shiftX[i]))
      dx[i]=(prefsdx>>prefs.shiftX[i])-x[i];
     else if (x[i]<0)
      dx[i]=(*w)->dx[i]+x[i];
     else
      dx[i]=(*w)->dx[i];
     dx[i]=std::min(dx[i],prefsdx>>prefs.shiftX[i]);
    }
   (*w)->print(startx, starty,dx,dy,dstLn,prefs.stride,bmp,msk,prefs.rtStart);
  }
}
void TrenderedSubtitleLine::clear(void)
{
 foreach (TrenderedSubtitleWordBase *word, *this)
  delete word;
 std::vector<value_type>::clear();
}
size_t TrenderedSubtitleLine::getMemorySize() const
{
    size_t memSize = 0;
    foreach (const TrenderedSubtitleWordBase *word, *this)
        memSize += word->getMemorySize();
    return memSize;
}

//============================== TrenderedSubtitleLines ==============================
void TrenderedSubtitleLines::print(const TprintPrefs &prefs)
{
 // Use the same renderer for SSA and SRT if extended tags option is checked (both formats can hold SSA tags and HTML tags)
 if ((prefs.subformat & Tsubreader::SUB_FORMATMASK) == Tsubreader::SUB_SSA
     || ((prefs.subformat & Tsubreader::SUB_FORMATMASK) == Tsubreader::SUB_SUBVIEWER) 
     && prefs.deci->getParam2(IDFF_subExtendedTags))
  return printASS(prefs);
 double y=0;
 if (empty()) return;
 unsigned int prefsdx,prefsdy;
 if (prefs.sizeDx && prefs.sizeDy)
  {
   prefsdx=prefs.sizeDx;
   prefsdy=prefs.sizeDy;
  }
 else
  {
   prefsdx=prefs.dx;
   prefsdy=prefs.dy;
  }

 double h=0,h1=0;
 for (const_iterator i=begin();i!=end();i++)
  {
   double h2=h1+(*i)->height();
   h1+=(double)prefs.linespacing*(*i)->charHeight()/100;
   if (h2>h) h=h2;
  }
 
 if (prefs.isOSD && prefs.ypos>=0)        // IffdshowDecVideo::drawOSD
  y = (double)(prefs.ypos*prefsdy)/100.0;
 else if (prefs.ypos<0)                   // prefs.ypos<0 means -prefs.ypos is absolute potision. OSD use this.
  y = -(double)prefs.ypos;
 else
  y = ((double)prefs.ypos*prefsdy)/100.0-h/2;

 if (prefs.ypos>=0 && y+h >= (double)prefsdy) y=(double)prefsdy-h-1;

 for (const_iterator i=begin();i!=end();y+=(double)prefs.linespacing*(*i)->charHeight()/100,i++)
  {
   if (y<0) continue;

   if ((unsigned int)y>=prefsdy) break;
   int x;
   unsigned int cdx=(*i)->width();
   if (prefs.xpos<0)
    x=-prefs.xpos; // OSD
   else
    {
     // subtitles
     x=(prefs.xpos * prefsdx)/100 + prefs.posXpix;
     switch (prefs.align)
      {
       case ALIGN_FFDSHOW:x=x-cdx/2;if (x<0) x=0;if (x+cdx>=prefsdx) x=prefsdx-cdx;break;
       case ALIGN_LEFT:break;
       case ALIGN_CENTER:x=x-cdx/2;break;
       case ALIGN_RIGHT:x=x-cdx;break;
      }
    }
   if (x+cdx>=prefsdx && !prefs.isOSD) x=prefsdx-cdx-1;
   if (x<0) x=0;
   (*i)->print(x,y,prefs,prefsdx,prefsdy); // print a line (=print words).
  }
}

void TrenderedSubtitleLines::printASS(const TprintPrefs &prefs)
{
 double y=0;
 if (empty()) return;
 unsigned int prefsdx,prefsdy;
 if (prefs.sizeDx && prefs.sizeDy)
  {
   prefsdx=prefs.sizeDx;
   prefsdy=prefs.sizeDy;
  }
 else
  {
   prefsdx=prefs.dx;
   prefsdy=prefs.dy;
  }

 std::map<ParagraphKey,ParagraphValue> paragraphs;
 


 // pass 1: prepare paragraphs : a paragraph is a set of lines that have the same properties
 // (same margins, alignment and position)
 for (const_iterator i=begin();i!=end();i++)
 {
   (*i)->prepareKaraoke();
   ParagraphKey pkey;
   prepareKey(i,pkey,prefsdx,prefsdy);
   std::map<ParagraphKey,ParagraphValue>::iterator pi=paragraphs.find(pkey);
   if (pi != paragraphs.end())
    {
     pi->second.topOverhang = std::min(pi->second.topOverhang ,double(pi->second.height-(*i)->get_topOverhang()));
     pi->second.bottomOverhang = std::max(pi->second.bottomOverhang ,double((*i)->get_bottomOverhang()));
     pi->second.height+=(*i)->charHeight();
    }
   else
    {
     ParagraphValue pval;
     pval.topOverhang = -(*i)->get_topOverhang();
     pval.bottomOverhang = (*i)->get_bottomOverhang();
     pval.height = (*i)->charHeight();
     paragraphs.insert(std::pair<ParagraphKey,ParagraphValue>(pkey,pval));
    }
  }

 // pass 2: print
 for (const_iterator i=begin();i!=end();i++)
  {
   ParagraphKey pkey;
   prepareKey(i,pkey,prefsdx,prefsdy);
   std::map<ParagraphKey,ParagraphValue>::iterator pi=paragraphs.find(pkey);
   if (pi != paragraphs.end())
    {
     ParagraphValue &pval=pi->second;
     if (pval.firstuse)
      {
       pval.firstuse=false;
       switch (pkey.alignment)
        {
         case 9: // SSA mid
         case 10:
         case 11:
          // With middle alignment and position/move tag we position the paragraph to the requested
          // position basing on the anchor point set at the middle
          if (pkey.isPos || pkey.isMove)
           pval.y=pkey.marginTop-pval.height/2.0;
          else // otherwise put the paragraph on the center of the screen (vertical margin is ignored)
           pval.y=(prefsdy - pval.height)/2.0;
          break;
         case 5: // SSA top
         case 6:
         case 7:
          pval.y = pkey.marginTop + pval.topOverhang;
          break;
         case 1: // SSA bottom
         case 2:
         case 3:
         default:
             // If the text is supposed to be placed at the bottom of the screen 
             // or has no vertical alignment defined
             // then apply the vertical position setting
             if (pkey.marginBottom == 0 && (prefs.deci->getParam2(IDFF_subSSAOverridePlacement)
                  || (prefs.subformat & Tsubreader::SUB_FORMATMASK) == Tsubreader::SUB_SUBVIEWER))
              pval.y=((double)prefs.ypos*prefsdy)/100.0-pval.height + pval.topOverhang;
             else
              pval.y=(double)prefsdy-pval.height - pkey.marginBottom + pval.topOverhang;
          break;
        }

       // If option is checked (or if subs are SUBVIEWER), correct vertical placement if text goes out of the screen
       if ((prefs.deci->getParam2(IDFF_subSSAMaintainInside) 
           || (prefs.subformat & Tsubreader::SUB_FORMATMASK) == Tsubreader::SUB_SUBVIEWER) && !(*i)->props.isMove)
       {
        if (pval.y+pval.height>prefsdy) pval.y=prefsdy-pval.height;
        if (pval.y<0) pval.y=0;
       }


       // Moving (scrolling text) : scroll from t1 to t2. Calculate vertical position
       if ((*i)->props.isMove && prefs.rtStart >= (*i)->props.get_moveStart())
       {
        // Stop scrolling if beyond t2
        if (prefs.rtStart >= (*i)->props.get_moveStop())
         pval.y+=(*i)->props.get_movedistanceV(prefsdy);
        else
         pval.y+=(*i)->props.get_movedistanceV(prefsdy)*
           (prefs.rtStart-(*i)->props.get_moveStart())/((*i)->props.get_moveStop()-(*i)->props.get_moveStart());
        }
      }
     y=pval.y;
     pval.y += (*i)->charHeight();
    }

   if (y>=(double)prefsdy) break;
   int x=0;
   unsigned int cdx=(*i)->width();
   // Left and right margins need to be recalculated according to the length of the line
   int marginL=(*i)->props.get_marginL(prefsdx, cdx);
   int marginR=(*i)->props.get_marginR(prefsdx, cdx);
   int leftOverhang=(*i)->get_leftOverhang();
   
   switch ((*i)->props.alignment)
   {
     case 1: // left(SSA)
     case 5:
     case 9:
      x=marginL - leftOverhang;
      break;
     case 3: // right(SSA)
     case 7:
     case 11:
      x=prefsdx - cdx - marginR - leftOverhang;
      break;
     case 2: // center(SSA)
     case 6:
     case 10:
     default:
      // If the text is supposed to be placed at the center of the screen 
      // or has no horizontal alignment defined
      // then apply the horizontal position setting
      if (marginL==0 && (prefs.deci->getParam2(IDFF_subSSAOverridePlacement)
           || (prefs.subformat & Tsubreader::SUB_FORMATMASK) == Tsubreader::SUB_SUBVIEWER))
       x=((double)prefs.xpos*prefsdx)/100.0 - (int)(cdx+marginR)/2 - leftOverhang;
      else if ((*i)->props.isPos) // If position defined, then marginL is relative to left border of the screen
       x=marginL-leftOverhang;
      else // else marginL is relative to the center of the screen
        x=((int)prefsdx - marginL - marginR - (int)cdx)/2 + marginL - leftOverhang;
      break;
  }

   // If option is checked (or if subs are SUBVIEWER), correct horizontal placement if text goes out of the screen
   if ((prefs.deci->getParam2(IDFF_subSSAMaintainInside) 
     || (prefs.subformat & Tsubreader::SUB_FORMATMASK) == Tsubreader::SUB_SUBVIEWER) && !(*i)->props.isMove)
   {
    if (x+cdx>prefsdx) x=prefsdx-cdx;
    if (x < 0) x=0;
   }

 
  // Moving (scrolling text) : scroll from t1 to t2. Calculate horizontal position
  if ((*i)->props.isMove && prefs.rtStart >= (*i)->props.get_moveStart())
  {
    // Stop scrolling if beyond t2
    if (prefs.rtStart >= (*i)->props.get_moveStop())
     x+=(*i)->props.get_movedistanceH(prefsdx);
    else
     x+=(*i)->props.get_movedistanceH(prefsdx)*
       (prefs.rtStart-(*i)->props.get_moveStart())/((*i)->props.get_moveStop()-(*i)->props.get_moveStart());
   }

   (*i)->print(x,y,prefs,prefsdx,prefsdy); // print a line (=print words).
  }
}

void TrenderedSubtitleLines::prepareKey(const_iterator i,ParagraphKey &pkey,unsigned int prefsdx,unsigned int prefsdy)
{
 pkey.alignment = (*i)->props.alignment;
 pkey.marginBottom = (*i)->props.get_marginBottom(prefsdy);
 pkey.marginTop = (*i)->props.get_marginTop(prefsdy);
 pkey.marginL = (*i)->props.get_marginL(prefsdx);
 pkey.marginR = (*i)->props.get_marginR(prefsdx);
 pkey.isPos = (*i)->props.isPos;
 pkey.isMove = (*i)->props.isMove;

 //pkey.layer = (*i)->props.layer; // TODO : uncomment when layers implemented
 pkey.layer = 0;
 if (pkey.isPos || pkey.isMove)
  {
   pkey.posx = (*i)->props.posx;
   pkey.posy = (*i)->props.posy;
  }
}

void TrenderedSubtitleLines::clear(void)
{
 foreach (TrenderedSubtitleLine *line, *this)
  {
   line->clear();
   delete line;
  }
 reset();
}

size_t TrenderedSubtitleLines::getMemorySize() const
{
    size_t memSize = 0;
    foreach (const TrenderedSubtitleLine *line, *this)
        memSize += line->getMemorySize();
    return memSize;
}

bool operator < (const TrenderedSubtitleLines::ParagraphKey &a, const TrenderedSubtitleLines::ParagraphKey &b)
{
    if (a.alignment<b.alignment) return true;
    if (a.alignment>b.alignment) return false;
    if (a.marginTop<b.marginTop) return true;
    if (a.marginTop>b.marginTop) return false;
    if (a.marginBottom<b.marginBottom) return true;
    if (a.marginBottom>b.marginBottom) return false;
    if (a.marginL<b.marginL) return true;
    if (a.marginL>b.marginL) return false;
    if (a.marginR<b.marginR) return true;
    if (a.marginR>b.marginR) return false;
    if (a.isPos<b.isPos) return true;
    if (a.isPos>b.isPos) return false;
    if (a.posx<b.posx) return true;
    if (a.posx>b.posx) return false;
    if (a.posy<b.posy) return true;
    //if (a.posy>b.posy) return false;
    return false;
};

//============================== TrenderedVobsubWord ===============================
void TrenderedVobsubWord::print(int startx, int starty /* not used */, unsigned int sdx[3],int sdy[3],unsigned char *dstLn[3],const stride_t stride[3],const unsigned char *bmp[3],const unsigned char *msk[3],REFERENCE_TIME rtStart) const
{
 if (sdy[0]<=0 || sdy[1]<0)
  return;
 int sdx15=sdx[0]-15;
 for (unsigned int y=0;y<(unsigned int)sdy[0];y++,dstLn[0]+=stride[0],msk[0]+=bmpmskstride[0],bmp[0]+=bmpmskstride[0])
  {
   int x=0;
   for (;x<sdx15;x+=16)
    {
     __m64 mm0=*(__m64*)(dstLn[0]+x),mm1=*(__m64*)(dstLn[0]+x+8);
     mm0=_mm_subs_pu8(mm0,*(__m64*)(msk[0]+x));mm1=_mm_subs_pu8(mm1,*(__m64*)(msk[0]+x+8));
     mm0=_mm_adds_pu8(mm0,*(__m64*)(bmp[0]+x));mm1=_mm_adds_pu8(mm1,*(__m64*)(bmp[0]+x+8));
     *(__m64*)(dstLn[0]+x)=mm0;*(__m64*)(dstLn[0]+x+8)=mm1;
    }
   for (;x<int(sdx[0]);x++)
    {
     int c=dstLn[0][x];
     c-=msk[0][x];if (c<0) c=0;
     c+=bmp[0][x];if (c>255) c=255;
     dstLn[0][x]=(unsigned char)c;
    }
  }
 __m64 m128=_mm_set1_pi8((char)-128/* 0x80 */),m0=_mm_setzero_si64(),mAdd=shiftChroma?m128:m0;
 int add=shiftChroma?128:0;
 int sdx7=sdx[1]-7;
 for (unsigned int y=0;y<dy[1];y++,dstLn[1]+=stride[1],dstLn[2]+=stride[2],msk[1]+=bmpmskstride[1],bmp[1]+=bmpmskstride[1],bmp[2]+=bmpmskstride[2])
  {
   int x=0;
   for (;x<sdx7;x+=8)
    {
     __m64 mm0=*(__m64*)(dstLn[1]+x);
     __m64 mm1=*(__m64*)(dstLn[2]+x);

     psubb(mm0,m128);
     psubb(mm1,m128);

     const __m64 msk8=*(const __m64*)(msk[1]+x);

     __m64 mskU=_mm_cmpgt_pi8(m0,mm0); //what to be negated
     mm0=_mm_or_si64(_mm_and_si64(mskU,_mm_adds_pu8(mm0,msk8)),_mm_andnot_si64(mskU,_mm_subs_pu8(mm0,msk8)));

     __m64 mskV=_mm_cmpgt_pi8(m0,mm1);
     mm1=_mm_or_si64(_mm_and_si64(mskV,_mm_adds_pu8(mm1,msk8)),_mm_andnot_si64(mskV,_mm_subs_pu8(mm1,msk8)));

     mm0=_mm_add_pi8(_mm_add_pi8(mm0,*(__m64*)(bmp[1]+x)),mAdd);
     mm1=_mm_add_pi8(_mm_add_pi8(mm1,*(__m64*)(bmp[2]+x)),mAdd);

     *(__m64*)(dstLn[1]+x)=mm0;
     *(__m64*)(dstLn[2]+x)=mm1;
    }
   for (;x<int(sdx[1]);x++)
    {
     int m=msk[1][x],c;
     c=dstLn[1][x];
     c-=128;
     if (c<0) {c+=m;if (c>0) c=0;}
     else     {c-=m;if (c<0) c=0;}
     c+=bmp[1][x];
     c+=add;
     dstLn[1][x]=c;//(unsigned char)limit(c,0,255);

     c=dstLn[2][x];
     c-=128;
     if (c<0) {c+=m;if (c>0) c=0;}
     else     {c-=m;if (c<0) c=0;};
     c+=bmp[2][x];
     c+=add;
     dstLn[2][x]=c;//(unsigned char)limit(c,0,255);
    }
  }
 _mm_empty();
}

//==================================== Tfont ====================================
Tfont::Tfont(IffdshowBase *Ideci):
 fontManager(NULL),
 deci(Ideci),
 oldsub(NULL),
 hdc(NULL),oldFont(NULL),
 height(0),
 fontSettings((TfontSettings*)malloc(sizeof(TfontSettings)))
{
}
Tfont::~Tfont()
{
 done();
 free(fontSettings);
}
void Tfont::init(const TfontSettings *IfontSettings)
{
 done();
 memcpy(fontSettings,IfontSettings,sizeof(TfontSettings));
 hdc=CreateCompatibleDC(NULL);
 if (!hdc) return;
 SetBkMode(hdc,TRANSPARENT); 
 SetTextColor(hdc,0xffffff); 
 SetMapMode(hdc,MM_TEXT);
}
void Tfont::done(void)
{
 if (hdc)
  {
   if (oldFont) SelectObject(hdc,oldFont);oldFont=NULL;
   DeleteDC(hdc);hdc=NULL;
  }
 oldsub=NULL;
}

void Tfont::prepareC(TsubtitleText *sub,const TrenderedSubtitleLines::TprintPrefs &prefs,bool forceChange)
{
    sub->prepareRendering(prefs,*this,forceChange);
    lines.insert(lines.end(),sub->lines.begin(),sub->lines.end());
}

int Tfont::print(TsubtitleText *sub,bool forceChange,const TrenderedSubtitleLines::TprintPrefs &prefs)
{
    if (!sub) return 0;
    prepareC(sub,prefs,forceChange);
    lines.print(prefs);
    int h = 0;
    foreach (TrenderedSubtitleLine *line, lines)
        h += line->height();
    reset();
    return h;
}

void Tfont::print(const TrenderedSubtitleLines::TprintPrefs &prefs)
{
    lines.print(prefs);
}
